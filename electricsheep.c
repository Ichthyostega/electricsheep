/*
    electricsheep - collaborative screensaver
    Copyright (C) 1999-2006 Scott Draves <source@electricsheep.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

static char *electricsheep_c_id =
"@(#) $Id: electricsheep.c,v 1.67 2006/07/25 20:45:56 spotspot Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/file.h>
#include <fcntl.h>
#include <expat.h>

#include "config.h"
#include "getdate.h"

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#define STATFS statvfs
#else
#define STATFS statfs
#endif

char *splash_prefix = PACKAGE_DATA_DIR "/electricsheep";

#define uniqueid_len 16
char uniqueid[uniqueid_len+1];

char *proxy_name = 0;
char *proxy_user = 0;

char *leave_prefix = NULL;
int leave_max_megabytes = -1;
int min_megabytes = 100;
int reset_fuse_length = 300;

/* only update cache every 100 sheep =~ 6 minutes. */
int cache_update_delay = 100;

/* wait this long before going to work */
int init_delay = 600;
int init_delay_list = 60;
int list_freshness = 600;

#define copy_buffer_size 64000

int generation = -1;

int debug = 0;
int stats = 0;

int nrestarts = 0;
int nplays = 0;
int nloopplays = 0;
int reset_fuse = 0;

int bracket_begin_id = -1;
int bracket_end_id = -1;

char *logfile = NULL;

time_t bracket_begin_time = (time_t)(-1);
time_t bracket_end_time = (time_t)(-1);

int nrepeats = 2;
int nthreads = -1;

char *dream_server = "v2d6.sheepserver.net";

// cheeze XXX
#define max_cp_size 300000
#define MAXBUF (5*PATH_MAX)

char *play_prog = "mpeg2dec_onroot";
char *nick_name = "";
char *url_name = "";
char *vote_prog = "electricsheep-voter";
char *client_version = "LNX_" VERSION;

char cps_name[PATH_MAX] = "";
char jpg_name[PATH_MAX] = "";
char mpg_name[PATH_MAX] = "";
char fifo_name[PATH_MAX] = "";

char curl_cmd[MAXBUF];

char id_file[MAXBUF];

int history_size = 300;
int *path_history;
int last_sheep = -1;
int nrepeated = 0;
int max_repeats = 2;


// by default xscreensaver starts us at priority 10.
// everything but the mpeg decoder runs at that (10) plus this:
int nice_level = 10;

#define bufmax 300
char nick_buf[bufmax];
char url_buf[bufmax];

int frame_rate = 23;
char *window_id = NULL;
int on_root = 0;
int timeout = 401;
int tryagain = 696;

char *hide_stderr = "2> /dev/null";

int display_anim = 1;
int voting = 1;
int nobg = 0;
int parasite = 0;
int standalone = 0;
int read_only = 0;
int show_errors = 1;

int display_zoomed = 0;
int use_mplayer = 0;

pid_t displayer_pid = 0;
pid_t downloader_pid = 0;
pid_t decoder_pid = 0;
pid_t ui_pid = 0;


FILE *mpeg_pipe = NULL;

int current_anim_id = -1;
int start_id = -1;

#define max_url_length 1000

typedef struct {
    int generation;
    int id;
    int deleted;
    int readonly;
    int checked;
    int type;
    time_t ctime;
    int size;
    int rating;
    int first;
    int last;
    char path[PATH_MAX];
    char url[max_url_length];
} anim_t;


int nserver_anims, ncached_anims;
anim_t *server_anims = NULL;
anim_t *cached_anims = NULL;

char *thread_name = NULL;

void
cleanup() {
    if (debug) printf("cleanup.\n");

    if (mpg_name[0]) unlink(mpg_name);
    if (cps_name[0]) unlink(cps_name);
    if (jpg_name[0]) unlink(jpg_name);
    unlink(fifo_name);
}
  
void
handle_sig_term(int sig) {
    if (debug)
	printf("handle_sig_term %s %d\n",
	       (thread_name ? thread_name : "(nyet)"), sig);

    cleanup();

    /* remove handler to prevent infinite loop */
    signal(SIGTERM, SIG_DFL);

    /* terminate process group, ie self & all children */
    kill(0, SIGTERM);
}

void
cleanup_and_exit(int status) {
    if (debug) printf("cleanup_and_exit %d\n", status);
    handle_sig_term(0);
}


/* from the system(3) manpage */
int
interruptable_system(char *command) {
    int pid, status;

    if (command == 0)
	return 1;
    pid = fork();
    if (pid == -1)
	return -1;
    if (pid == 0) {
	char *argv[4];
	argv[0] = "sh";
	argv[1] = "-c";
	argv[2] = command;
	argv[3] = 0;
	execv("/bin/sh", argv);
	exit(127);
    }
    do {
	if (waitpid(pid, &status, 0) == -1) {
	    if (EINTR == errno)
		cleanup_and_exit(0);
	} else
	    return status;
    } while(1);
    // notreached
}


int
mysystem(char *cmd, char *msg) {
    int n;
    if (0) fprintf(stderr, "subprocess; (%s)\n", cmd);
    if (0 != (n = interruptable_system(cmd))) {
	if (SIGINT != n) {
	    if (show_errors)
		fprintf(stderr, "subprocess error: %s, %d=%d<<8+%d\n",
			msg, n, n>>8, n&255);
	    return 1;
	}
	fprintf(stderr, "control-c during %s, exiting\n", msg);
	cleanup_and_exit(1);
    }
    return 0;
}


/* not fatal if subprocess fails */
void
mysystem2(char *cmd, char *msg) {
    int n;
    if (0) fprintf(stderr, "subprocess; (%s)\n", cmd);
    if (0 != (n = interruptable_system(cmd))) {
	if (SIGINT != n)
	    fprintf(stderr, "subprocess failure: %s, %d=%d<<8+%d\n",
		    msg, n, n>>8, n&255);
	else
	    fprintf(stderr, "control-c during %s\n", msg);
    }
}

void timestamp() {
  time_t t;
  if (debug) {
      time(&t);
      printf("time %s", ctime(&t));
  }
}

static void
encode(char *dst, char *src) {
    static char *hex = "0123456789ABCDEF";
    char t;
    while (t = *src++) {
	if (isalnum(t)) {
	    *dst++ = t;
	} else {
	    *dst++ = '%';
	    *dst++ = hex[(t >> 4) & 15];
	    *dst++ = hex[t & 15];
	}
    }
}

void
print_anims(char *name, anim_t *an, int nanims) {
    int i;
    printf("%s=%d\n", name, nanims);
    for (i = 0; i < nanims; i++) {
	printf("gen=%d id=%d deleted=%d readonly=%d checked=%d "
	       "type=%d ctime=%d size=%d rating=%d "
	       "first=%d last=%d path=%s url=%s\n",
	       an[i].generation,
	       an[i].id, an[i].deleted, an[i].readonly, an[i].checked, an[i].type,
	       an[i].ctime, an[i].size, an[i].rating, an[i].first,
	       an[i].last, an[i].path, an[i].url);
    }
}

int
filename_is_mpg(char *name) {
    int n = strlen(name);
    return !(n <= 4 || 0 != strcmp(&name[n-4], ".mpg"));
}

int
filename_is_xxx(char *name) {
    int n = strlen(name);
    return !(n <= 4 || 0 != strcmp(&name[n-4], ".xxx"));
}

void clear_path(char *path, int generation) {
    DIR *d;
    struct dirent *e;
    char fbuf[PATH_MAX];
    struct stat sbuf;
    
    if (debug) printf("killing generation %d in %s!\n", generation, path);

    d = opendir(path);

    if (!d) {
	perror(path);
	cleanup_and_exit(1);
    }

    while (e = readdir(d)) {
	int g;
	snprintf(fbuf, PATH_MAX, "%s%s", path, e->d_name);
	
	if (e->d_name[0] == '.') continue;
	if (0 == strncmp("gen", e->d_name, 3)) {
	    unlink(fbuf);
	    continue;
	}
	if (filename_is_mpg(e->d_name) || filename_is_xxx(e->d_name)) {
	    if (1 == sscanf(e->d_name, "%d=", &g) && generation == g) {
		unlink(fbuf);
		continue;
	    }
	}
	if (-1 == stat(fbuf, &sbuf)) continue;
	if (S_ISDIR(sbuf.st_mode)) {
	    strncat(fbuf, "/", PATH_MAX);
	    clear_path(fbuf, generation);
	}
    }
    closedir(d);
}

#define bt_zero_block_size (1 << 10)

int bt_zero_blocks(char *fname, int size) {
    int fd, i;
    int piece_size = 1 << 18;
    char buf[bt_zero_block_size];

    if (-1 == (fd = open(fname, O_RDONLY))) {
	perror(fname);
	return 1;
    }

    while (size >= bt_zero_block_size) {
	int nread;
	if (bt_zero_block_size != (nread = read(fd, buf, bt_zero_block_size))) {
	    perror(fname);
	    return 1;
	}
	for (i = 0; i < bt_zero_block_size; i++)
	    if (buf[i] != 0) break;
	if (i == bt_zero_block_size)
	    return 1;
	if (size < (piece_size + bt_zero_block_size))
	    break;
	if (-1 == lseek(fd, piece_size-bt_zero_block_size, SEEK_CUR)) {
	    perror(fname);
	    return 1;
	}
	size -= piece_size;
    }
    return 0;
}

    

void update_cached_anims_path(char *path, int match_gen) {
    int write_gen = 0;
    int checked_gen = 0;
    int clear_gen = 0;
    int finished = 0;
    int ncached_anims_saved = ncached_anims;
    DIR *d;
    struct dirent *e;
    struct stat sbuf;
    char fbuf[MAXBUF];
    int old_gen = -1;

    if (debug) printf("updating cache path=%s match_gen=%d\n", path, match_gen);

    d = opendir(path);
    if (!d) {
	perror(path);
	cleanup_and_exit(1);
    }
    while (e = readdir(d)) {
	anim_t *an = &cached_anims[ncached_anims];
	int i, dummy;
	if (match_gen) {
	    if (0 == strncmp("gen", e->d_name, 3)) {
		sscanf(e->d_name, "gen%d", &old_gen);
		checked_gen = 1;
		if (old_gen != generation && generation != -1) {
		    if (debug)
			printf("old_gen=%d generation=%d\n", old_gen, generation);
		    write_gen = 1;
		    clear_gen = 1;
		}
	    }
	}
	if (e->d_name[0] == '.') continue;
	if (!strcmp("finished", e->d_name)) {
	    finished = 1;
	}
	if (filename_is_xxx(e->d_name)) {
	    if (4 != sscanf(e->d_name, "%d=%d=%d=%d.xxx",
			    &an->generation, &an->id,
			    &an->first, &an->last)) {
		continue;
	    }
	    an->deleted = 1;
	    for (i = 0; i < nserver_anims; i++) {
		if (server_anims[i].id == an->id) {
		    break;
		}
	    }
	    if (i == nserver_anims && nserver_anims) {
		snprintf(fbuf, MAXBUF, "%s%s", path, e->d_name);
		if (debug)
		    printf("removing marker %s nserver_anims=%d\n",
			   fbuf, nserver_anims);
		unlink(fbuf);
		continue;
	    }
	    ncached_anims++;
	    cached_anims = realloc(cached_anims, (1+ncached_anims)*sizeof(anim_t));
	    continue;
	}

	snprintf(fbuf, PATH_MAX, "%s%s", path, e->d_name);
	if (-1 == stat(fbuf, &sbuf)) continue;

	if (0 == sbuf.st_size) continue;

	if (S_ISDIR(sbuf.st_mode)) {
	    strncat(fbuf, "/", PATH_MAX);
	    update_cached_anims_path(fbuf, 0);
	    continue;
	}
 
	if (!filename_is_mpg(e->d_name)) continue;

	if (4 != sscanf(e->d_name, "%d=%d=%d=%d.mpg",
			&an->generation, &an->id, &an->first, &an->last)) {
	    continue;
	}

	if ((bracket_begin_id != -1 && an->id < bracket_begin_id) ||
	    (bracket_end_id != -1 && an->id > bracket_end_id))
	  continue;

	if ((bracket_begin_time != (time_t)(-1) &&
	     sbuf.st_ctime < bracket_begin_time) ||
	    (bracket_end_time != (time_t)(-1) &&
	     sbuf.st_ctime > bracket_end_time))
	  continue;

	an->rating = 0;
	for (i = 0; i < nserver_anims; i++) {
	    if (server_anims[i].id == an->id) {
		an->rating = server_anims[i].rating;
		break;
	    }
	}

	an->deleted = 0;
	an->checked = !strcmp(path, leave_prefix);
	an->readonly = !((sbuf.st_mode & S_IWUSR) &&
			 (sbuf.st_uid == getuid()));
	an->size = sbuf.st_size;
	an->ctime = sbuf.st_ctime;
	an->url[0] = 0;
	an->type = 0;
	strncpy(an->path, path, PATH_MAX);
	ncached_anims++;
	cached_anims = realloc(cached_anims, (1+ncached_anims)*sizeof(anim_t));
    }
    closedir(d);

    if (finished) {
	int i;
	for (i = ncached_anims_saved; i < ncached_anims; i++) {
	    if (!strcmp(cached_anims[i].path, path)) {
		cached_anims[i].checked = 1;
	    }
	}
    }

    if (match_gen && !checked_gen) write_gen = 1;

    if (clear_gen) clear_path(path, old_gen);
	
    if (write_gen) {
	if (debug) printf("write gen %d.\n", generation);
	snprintf(fbuf, MAXBUF, "touch %sgen%d", leave_prefix, generation);
	mysystem(fbuf, "touch gen");
    }
}

void update_cached_anims(int match_gen) {
    ncached_anims = 0;
    if (NULL == cached_anims)
	cached_anims = malloc(sizeof(anim_t));
    update_cached_anims_path(leave_prefix, match_gen);
    if (debug > 1)
	print_anims("ncached_anims", cached_anims, ncached_anims);
}


static int
irandom(int n) {
    return random()%n;
}

void
cached_file_name(char *buf, anim_t *an) {
    snprintf(buf, MAXBUF, "%s%05d=%05d=%05d=%05d.mpg",
	    an->path, an->generation, an->id, an->first, an->last);
}

void
deleted_file_name(char *buf, anim_t *an) {
    snprintf(buf, MAXBUF, "%s%05d=%05d=%05d=%05d.xxx",
	    an->path, an->generation, an->id, an->first, an->last);
}

void
not_playing() {
    char pbuf[MAXBUF];
    if (!read_only) {
	snprintf(pbuf, MAXBUF, "echo none > %sid", leave_prefix);
	mysystem(pbuf, "writing none to id file");
    }
}    

void
print_stats() {
    printf("nplays = %d, ", nplays);
    printf("nloopplays = %d, ", nloopplays);
    printf("nrestarts = %d, ", nrestarts);
    printf("ncached_anims = %d\n", ncached_anims);
}

int check_for_eddy() {
    int i, j, c = 0, n = history_size;
    if (n > 50) n = 50;
    for (i = 0; i < n; i++) {
	int diff = 1;
	if (-1 == path_history[i]) break;
	if (debug > 1)
	    printf("edd checking %d %d %d\n", c, i, path_history[i]);
	for (j = 0; j < i; j++) {
	    if (path_history[j] == path_history[i])
		diff = 0;
	}
	c += diff;
	if (c <= i/3) {
	    if (debug) printf("eddy %d/%d\n", c, i);
	    return 1;
	}
    }
    return 0;
}

int touch(char *fname) {
    FILE *fp = fopen(fname, "w");
    if (NULL == fp) {
	return 1;
    }
    fclose(fp);
    return 0;
}


int check_sheep(int idx) {
    char fbuf[MAXBUF];
    int j;
    int zeroes = 0;

    if (cached_anims[idx].checked) {
	return 0;
    }
    /* should search locally until mismatch */
    for (j = 0; j < ncached_anims; j++) {
	
	if (!strcmp(cached_anims[idx].path,
		    cached_anims[j].path)) {
	    cached_file_name(fbuf, &cached_anims[j]);
	    if (bt_zero_blocks(fbuf, cached_anims[j].size)) {
		if (debug)
		    printf("zero_blocks %s\n", fbuf);
		zeroes = 1;
		break;
	    }
	}
    }
    if (!zeroes) {
	char pbuf[MAXBUF];
	snprintf(pbuf, MAXBUF, "%sfinished", cached_anims[idx].path);
	if (debug)
	    printf("marking finished %s\n", pbuf);
	touch(pbuf);
    } else if (debug)
	printf("unfinished %s\n", cached_anims[idx].path);
    for (j = 0; j < ncached_anims; j++) {
	if (!strcmp(cached_anims[idx].path,
		    cached_anims[j].path)) {
	    if (zeroes)
		cached_anims[j].deleted = 1;
	    else
		cached_anims[j].checked = 1;
	}
    }
    return zeroes;
}

int *succs = NULL, *lsuccs = NULL;
int succs_len, lsuccs_len;

/* traverse the graph of anims, and writes the mpeg files into stdin
   of the child.  another process decodes the mpeg and displays it on
   the screen. */

void
do_display() {
    int i;
    int idx;
    int niters;
    static patience=1;
    char fbuf[MAXBUF];


    if (start_id >= 0) {
	current_anim_id = start_id;
	start_id = -1;
    }

    idx = -1;
    if (-1 != current_anim_id) {
	for (i = 0; i < ncached_anims; i++) {
	    if (!cached_anims[i].deleted &&
		cached_anims[i].id == current_anim_id) {
		idx = i;
		break;
	    }
	}
    }

    if (idx != -1 && check_sheep(idx)) idx = -1;

    while (-1 == idx) {
	int n = 0;
	for (i = 0; i < ncached_anims; i++) {
	    if (!cached_anims[i].deleted) n++;
	}
	if (0 == n) {
	    if (patience)
		fprintf(stderr, "please be patient while the "
			"first sheep is downloaded...\n");
	    patience = 0;
	    not_playing();
	    sleep(10);
	    update_cached_anims(0);
	    return;
	}
	do {
	    idx = irandom(ncached_anims);
	} while (cached_anims[idx].deleted);
	
	current_anim_id = cached_anims[idx].id;
	if (debug) printf("found new anim id=%d.\n", current_anim_id);

	if (check_sheep(idx)) idx = -1;
    }


    if (cached_anims[idx].first == cached_anims[idx].last) {
	niters = nrepeats;
    } else {
	niters = 1;
    }



    if (1) {
	// play an anim
	int h, i;
	char pbuf[MAXBUF];
	FILE *idf;

	if (debug) printf("play anim x=%d id=%d iters=%d path=%s.\n",
			  idx, current_anim_id, niters, cached_anims[idx].path);
	if (stats) print_stats();

	nplays++;
	if (cached_anims[idx].first == cached_anims[idx].last) {
	    nloopplays++;
	}

	snprintf(fbuf, MAXBUF, "%sid.tmp", leave_prefix);
	idf = fopen(fbuf, "w");
	if (NULL == idf) {
	    perror(fbuf);
	    goto skipped;
	}
	fprintf(idf, "%d\n", current_anim_id);
	for (h = 0; h < history_size; h++)
	    if (path_history[h] != -1) {
		fprintf(idf, "%d\n", path_history[h]);
	    }
	fclose(idf);
	snprintf(pbuf, MAXBUF, "%sid", leave_prefix);
	if (-1 == rename(fbuf, pbuf)) {
	    fprintf(stderr, "rename %s %s\n", fbuf, pbuf);
	    perror(pbuf);
	    goto skipped;
	}

	for (h = history_size - 1; h > 0; h--) {
	    path_history[h] = path_history[h-1];
	}
	path_history[0] = current_anim_id;

	if (debug > 1) {
	    printf("history =");
	    for (h = 0; h < history_size; h++) {
	      if (-1 == path_history[h])
		break;
	      else
		printf(" %d", path_history[h]);
	    }
	    printf("\n");
	}

	cached_file_name(fbuf, &cached_anims[idx]);

	for (i = 0; i < niters; i++) {
	    FILE *mpeg_file;
	    char copy_buf[copy_buffer_size];
	    size_t n;

	    mpeg_file = fopen(fbuf, "r");
	    if (NULL == mpeg_file) {
		perror(fbuf);
		goto skipped;
	    }
	    do {
		n = fread (copy_buf, 1, copy_buffer_size, mpeg_file);
		if (n <= 0) {
		    perror(fbuf);
		    goto skipped;
		}
		if ( n != fwrite(copy_buf, 1, n, mpeg_pipe)) {
		    perror("writing to mpeg pipe");
		    fclose(mpeg_file);
		    goto skipped;
		}
	    } while (copy_buffer_size == n);
	    fclose(mpeg_file);
	}
    }

 skipped:


    if (last_sheep == current_anim_id)
      nrepeated++;
    else
      nrepeated = 0;
    last_sheep = current_anim_id;

    if (0 == reset_fuse-- || nrepeated >= max_repeats || check_for_eddy()) {
      if (debug) printf("reset nrepeated=%d reset_fuse=%d\n", nrepeated, reset_fuse);
      current_anim_id = -1;
      reset_fuse = reset_fuse_length;
    } else {
	// pick next anim at random from all possible succs,
	// trying to avoid repeats, and giving priority to loops.
	int nsuccs = 0;
	int lnsuccs = 0;
	int sym = cached_anims[idx].last;
	int h;

	if (NULL == succs) {
	    succs = malloc(sizeof(int));
	    succs_len = 1;
	    lsuccs = malloc(sizeof(int));
	    lsuccs_len = 1;
	}

	for (h = history_size; h >= 0; h--) {
	    for (i = 0; i < ncached_anims; i++) {
		if (!cached_anims[i].deleted &&
		    (cached_anims[i].generation == cached_anims[idx].generation) &&
		    cached_anims[i].first == sym) {
		    int hh;
		    for (hh = 0; hh < h; hh++) {
			if (path_history[hh] == cached_anims[i].id)
			    break;
		    }
		    if (hh == h) {
			if (debug > 1) printf("succ x=%d id=%d h=%d\n",
					      i, cached_anims[i].id, h);
			if (cached_anims[i].first == cached_anims[i].last) {
			    lsuccs[lnsuccs++] = i;
			    if (lnsuccs == lsuccs_len) {
				lsuccs = realloc(lsuccs, (1+lsuccs_len)*sizeof(int));
				lsuccs_len++;
			    }
			} else {
			    succs[nsuccs++] = i;
			    if (nsuccs == succs_len) {
				succs = realloc(succs, (1+succs_len)*sizeof(int));
				succs_len++;
			    }
			}
		    }
		}
	    }
	    if (lnsuccs || nsuccs) break;
	}
	if (lnsuccs) {
	    idx = lsuccs[irandom(lnsuccs)];
	    current_anim_id = cached_anims[idx].id;
	    if (debug) printf("lsucc to %d, lnsuccs=%d.\n",
			      current_anim_id, lnsuccs);
	} else if (nsuccs) {
	    idx = succs[irandom(nsuccs)];
	    current_anim_id = cached_anims[idx].id;
	    if (debug) printf("succ to %d, nsuccs=%d.\n",
			      current_anim_id, nsuccs);
	} else {
	    if (debug) printf("no succ.\n");
	    current_anim_id = -1;
	    nrestarts++;
	}
    }
}

int
cache_overflow(double bytes) {
    struct STATFS buf;

    if (-1 == STATFS(leave_prefix, &buf)) {
	perror(leave_prefix);
	cleanup_and_exit(1);
    }

    return (leave_max_megabytes &&
	    (bytes > (1024.0 * 1024 * leave_max_megabytes))) ||
	(min_megabytes &&
	 ((buf.f_bavail * (double) buf.f_bsize) <
	  (min_megabytes * 1024.0 * 1024)));
}

/* make enough room in cache to download SIZE more bytes */
void
delete_cached(int size) {
    int i;
    double total;
    char buf[MAXBUF];
    time_t oldest_time = 0;
    int worst_rating;
    int best;

    if (debug) printf("begin delete cached\n");
      
    while (ncached_anims) {

	total = size;
	oldest_time=0;
	worst_rating=0;
	best=0;

	for (i = 0; i < ncached_anims; i++) {
	    if (cached_anims[i].deleted ||
		cached_anims[i].readonly) continue;
	    if (debug)
	      printf("delete cached total=%g id=%d rating=%d\n",
		     total, cached_anims[i].id, cached_anims[i].rating);
    
	    total += cached_anims[i].size;
	    if (!oldest_time ||
		(cached_anims[i].rating < worst_rating) ||
		((cached_anims[i].rating == worst_rating) &&
		 (cached_anims[i].ctime < oldest_time))) {
		best = i;
		oldest_time = cached_anims[i].ctime;
		worst_rating = cached_anims[i].rating;
	    }
	}
	if (oldest_time && cache_overflow(total)) {
	    int fd;
	    cached_file_name(buf, &cached_anims[best]);
	    if (debug) printf("deleting %s\n", buf);
	    if (-1 == unlink(buf)) {
		perror(buf);
		continue;
	    }
	    cached_anims[best].deleted = 1;
	    deleted_file_name(buf, &cached_anims[best]);
	    if (-1 == (fd = creat(buf, S_IRUSR|S_IWUSR))) {
		perror(buf);
		exit(1);
	    }
	    close(fd);
	} else {
	    return;
	}
    }
    if (debug) printf("nothing cached\n");
}



void
download_anim(int idx) {
    char pbuf[MAXBUF];
    char tfb[MAXBUF];
    char cfb[MAXBUF];
    struct stat sbuf;
    char *p;

    if (debug) printf("download %d id=%d\n", idx, server_anims[idx].id);
    
    cached_file_name(cfb, &server_anims[idx]);
    snprintf(tfb, MAXBUF, "%s.tmp", cfb);
    strcpy(mpg_name, tfb);
    
    snprintf(pbuf, MAXBUF, "%s --output %s %s",
	     curl_cmd, tfb, server_anims[idx].url);

    if (debug) printf("about to %s\n", pbuf);

    mysystem(pbuf, "anim download");

    if (-1 == stat(tfb, &sbuf)) {
	if (show_errors)
	    fprintf(stderr, "download failed of sheep %d\n",
		    server_anims[idx].id);
	unlink(tfb);
	mpg_name[0] = 0;
	sleep(tryagain);
	return;
    }
    if (sbuf.st_size != server_anims[idx].size) {

      if (debug)
	printf("deleted incomplete sheep id=%d got=%ld want=%ld\n",
	       server_anims[idx].id, (long)sbuf.st_size,
	       (long)server_anims[idx].size);

	unlink(tfb);
	mpg_name[0] = 0;
	sleep(tryagain);
	return;
    }

    if (-1 == rename(tfb, cfb)) {
	perror("move download temp to cache");
	fprintf(stderr, "move %s to %s\n", tfb, cfb);
	cleanup_and_exit(1);
    }
    mpg_name[0] = 0;
    if (debug) printf("download complete %d id=%d\n",
		      idx, server_anims[idx].id);
}

void
get_control_points(char *buf, int buf_size) {
    int n;
    char pbuf[MAXBUF];
    FILE *cp;
  
    snprintf(pbuf, MAXBUF, "%s 'http://%s/cgi/get?"
	    "n=%s&w=%s&v=%s&u=%s' | gunzip -c %s",
	    curl_cmd, dream_server, nick_buf,
	    url_buf, client_version, uniqueid, hide_stderr);

    if (debug) printf("get_control_points %s\n", pbuf);

    cp = popen(pbuf, "r");

    if (NULL == cp) {
	perror("could not fork/pipe\n");
	cleanup_and_exit(1);
    }

    while (n = fread(buf, 1, buf_size, cp)) {
	buf += n;
	buf_size -= n;
	if (1 >= buf_size) {
	    fprintf(stderr, "cp buffer overflow - the server is spamming me!\n");
	    cleanup_and_exit(1);
	}
    }
    *buf = 0; /* null terminate */

    if (ferror(cp)) {
	perror("get pipe error\n");
	cleanup_and_exit(1);
    }

    if (-1 == pclose(cp)) {
	perror("pclose of get failed\n");
	cleanup_and_exit(1);
    }
}

void
put_image(char *fname, int frame, int anim_id, int gen) {
    int n, type;
    char pbuf[MAXBUF];
    struct stat sbuf;

    if (-1 == stat(fname, &sbuf)) {
	perror(fname);
	cleanup_and_exit(1);
    }

    snprintf(pbuf, MAXBUF, "%s --upload-file %s "
	    "'http://%s/cgi/put?f=%d&id=%d&s=%ld&g=%d&v=%s&u=%s'",
	    curl_cmd, fname, dream_server, frame, anim_id,
	    (long)sbuf.st_size, gen, client_version, uniqueid);
    if (debug) printf("about to put %s\n", pbuf);
    mysystem(pbuf, "put image");
}

int get_gen;
int get_id;
int get_type;
int get_frame;

int in_message = 0;
int server_error = 0;
int got_sheep = 0;
char server_error_type[MAXBUF];

void
get_start_element(void *userData, const char *name, const char **atts) {
    int i = 0;
    if (!strcmp("get", name)) {
	while (atts[i]) {
	    const char *a = atts[i+1];
	    if (!strcmp(atts[i], "gen")) {
		get_gen = atoi(a);
	    } else if (!strcmp(atts[i], "id")) {
		get_id = atoi(a);
	    } else if (!strcmp(atts[i], "type")) {
		get_type = atoi(a);
	    } else if (!strcmp(atts[i], "frame")) {
		get_frame = atoi(a);
	    }
	    i += 2;
	}
    } else if (!strcmp("message", name)) {
	in_message = 1;
    } else if (!strcmp("flame", name)) {
	got_sheep = 1;
    } else if (!strcmp("error", name)) {
	server_error = 1;
	while (atts[i]) {
	    if (!strcmp(atts[i], "type")) {
		strncpy(server_error_type, atts[i+1], MAXBUF);
	    }
	    i += 2;
	}
    }
}

void
get_end_element(void *userData, const char *name) {
    if (!strcmp("message", name)) {
	in_message = 0;
    }
}

void
character_handler(void *userData, const XML_Char *s, int len) {
    if (in_message) {
	fwrite(s, sizeof(XML_Char), len, stderr);
    }
}

void get_args(char *args, char *fname) {
  FILE *f;
  int n, i;
  char lbuf[MAXBUF];
  f = fopen(fname, "r");
  if (NULL == f) {
    perror(fname);
    cleanup_and_exit(1);
  }
  lbuf[0] = 0;
  fgets(lbuf, MAXBUF, f);
  fclose(f);
  n = strlen(lbuf);
  args[0] = 0;
  for (i = 0; i < n; i++) {
    if (0 == strncmp("args=", lbuf+i, 5)) {
      char *end, *beg = strchr(lbuf+i, '"');
      if (NULL == beg) {
	fprintf(stderr, "missing begin quote after args: %s", lbuf);
	cleanup_and_exit(1);
      }
      end = strchr(beg+1, '"');
      if (NULL == end) {
	fprintf(stderr, "missing end quote after args: %s", lbuf);
	cleanup_and_exit(1);
      }
      strncpy(args, beg+1, end-beg-1);
      args[end-beg-1] = 0;
      break;
    }
  }
}

void
do_render() {
    char anim_text[max_cp_size];
    char *cp_text, *image;
    int this_size;
    char prog[101];
    int cps_fd, jpg_fd;
    FILE *fp;
    XML_Parser parser;

    timestamp();

    get_control_points(anim_text, max_cp_size);

    if (0 == anim_text[0]) {
	if (show_errors)
	    fprintf(stderr,
		    "lost contact with %s, cannot render frames.\n",
		    dream_server);
	sleep(tryagain);
	return;
    }

    get_gen = get_id = get_type = get_frame = -1;

    parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, get_start_element, get_end_element);
    XML_SetCharacterDataHandler(parser, character_handler);
    in_message = 0;
    server_error = 0;
    got_sheep = 0;
    if (!XML_Parse(parser, anim_text, strlen(anim_text), 1)) {
	fprintf(stderr, "%s at line %d\n",
		XML_ErrorString(XML_GetErrorCode(parser)),
		XML_GetCurrentLineNumber(parser));
	cleanup_and_exit(1);
    }
    XML_ParserFree(parser);
    if (server_error) {
	fprintf(stderr, "server reported error for get: %s\n",
		server_error_type);
	sleep(tryagain);
	return;
    }
    if (!got_sheep) {
	if (debug) printf("nothing to render, will try again later.\n");
	sleep(tryagain);
	return;
    }

    cp_text = anim_text;

    strcpy(cps_name, "/tmp/electricsheep.cps.XXXXXX");
    cps_fd = mkstemp(cps_name);

    if (strlen(cp_text) != write(cps_fd, cp_text, strlen(cp_text))) {
	perror(cps_name);
	cleanup_and_exit(1);
    }

    strcpy(jpg_name, "/tmp/electricsheep.jpg.XXXXXX");
    jpg_fd = mkstemp(jpg_name);

    {
	char b[MAXBUF];
	char args[MAXBUF];
	get_args(args, cps_name);
	snprintf(b, MAXBUF, "nice -n %d env in=%s time=%d out=%s %s flam3-animate",
		nice_level, cps_name, get_frame, jpg_name, args);
	if (debug) printf("about to render %s\n", b);
	if (mysystem(b, "render")) {
	    if (debug) printf("render failed, trying again later\n");
	    unlink(jpg_name);
	    jpg_name[0] = 0;
	    close(jpg_fd);
	    unlink(cps_name);
	    cps_name[0] = 0;
	    close(cps_fd);
	    sleep(tryagain);
	    return;
	}
    }

    put_image(jpg_name, get_frame, get_id, get_gen);

    close(cps_fd);
    close(jpg_fd);

    /* hm should we close after unlink? */

    if (unlink(jpg_name)) {
	perror(jpg_name);
	cleanup_and_exit(1);
    }
    jpg_name[0] = 0;
    if (unlink(cps_name)) {
	perror(cps_name);
	cleanup_and_exit(1);
    }
    cps_name[0] = 0;
}

void
list_start_element(void *userData, const char *name, const char **atts) {
    int i = 0;
    if (!strcmp("list", name)) {
	while (atts[i]) {
	    if (!strcmp(atts[i], "gen")) {
		generation = atoi(atts[i+1]);
		if (generation <= 0) {
		    fprintf(stderr, "generation must be positive.\n");
		    cleanup_and_exit(1);
		}
		break;
	    }
	    i += 2;
	}
    } else if (!strcmp("sheep", name)) {
	anim_t *an = &server_anims[nserver_anims];
	const char *state;
	if (-1 == generation) {
	    fprintf(stderr, "malformed list.  "
		    "received sheep without generation set.\n");
	    cleanup_and_exit(1);
	}
	memset(an, 0, sizeof(an));
	an->generation = generation;
	strncpy(an->path, leave_prefix, PATH_MAX);
	while (atts[i]) {
	    const char *a = atts[i+1];
	    if (!strcmp(atts[i], "id")) {
		an->id = atoi(a);
	    } else if (!strcmp(atts[i], "type")) {
		an->type = atoi(a);
	    } else if (!strcmp(atts[i], "time")) {
		an->ctime = (time_t) atoi(a);
	    } else if (!strcmp(atts[i], "size")) {
		an->size = atoi(a);
	    } else if (!strcmp(atts[i], "rating")) {
		an->rating = atoi(a);
	    } else if (!strcmp(atts[i], "first")) {
		an->first = atoi(a);
	    } else if (!strcmp(atts[i], "last")) {
		an->last = atoi(a);
	    } else if (!strcmp(atts[i], "state")) {
		state = a;
	    } else if (!strcmp(atts[i], "url")) {
		strncpy(an->url, a, max_url_length);
		an->url[max_url_length-1] = 0;
	    }
	    i += 2;
	}
	if (!strcmp(state, "done") && (0 == an->type)) {
	    nserver_anims++;
	    server_anims = realloc(server_anims, (1+nserver_anims) * sizeof(anim_t));
	} else if (!strcmp(state, "expunge")) {
	    char buf[MAXBUF];
	    if (debug) printf("expunging id=%d.\n", an->id);
	    cached_file_name(buf, an);
	    // ok to fail
	    unlink(buf);
	}
    } else if (!strcmp("message", name)) {
	in_message = 1;
    } else if (!strcmp("error", name)) {
	server_error = 1;
	while (atts[i]) {
	    if (!strcmp(atts[i], "type")) {
		strncpy(server_error_type, atts[i+1], MAXBUF);
	    }
	    i += 2;
	}
    } 
}

time_t server_anims_timestamp = -1;

void update_server_anims() {
    char pbuf[MAXBUF];
    FILE *lf;
    int done;
    XML_Parser parser;

    if (-1 == server_anims_timestamp)
      server_anims_timestamp = time(0);
    else {
      time_t now = time(0);
      if ((now - server_anims_timestamp) < list_freshness) {
	if (debug) printf("skipping cgi/list\n");
	return;
      }
      server_anims_timestamp = now;
    }
	
    parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, list_start_element, get_end_element);
    XML_SetCharacterDataHandler(parser, character_handler);
    in_message = 0;
    server_error = 0;
    
    snprintf(pbuf, MAXBUF, "%s 'http://%s/cgi/list?v=%s&u=%s'"
	    "| gunzip -c %s", curl_cmd, dream_server,
	    client_version, uniqueid, hide_stderr);

    if (debug) printf("list %s\n", pbuf);

    lf = popen(pbuf, "r");

    if (NULL == lf) {
	perror("could not fork/pipe\n");
	cleanup_and_exit(1);
    }

    nserver_anims = 0;
    if (NULL == server_anims)
	server_anims = malloc(sizeof(anim_t));

    generation = -1;

    do {
	size_t len = fread(pbuf, 1, MAXBUF, lf);
	done = len < MAXBUF;
	if (0 == len) {
	    // lost contact, no data to parse
	    break;
	}
	if (!XML_Parse(parser, pbuf, len, done)) {
	    fprintf(stderr, "%s at line %d\n",
		    XML_ErrorString(XML_GetErrorCode(parser)),
		    XML_GetCurrentLineNumber(parser));
	    break;
	}
    } while (!done);
    XML_ParserFree(parser);

    pclose(lf);

    if (server_error) {
	fprintf(stderr,
		"server reported error for list: %s\n",
		server_error_type);
	sleep(tryagain);
	return;
    }

    if (-1 == generation) {
	if (show_errors)
	    fprintf(stderr,
		    "lost contact with %s, cannot retrieve sheep.\n",
		    dream_server);
	sleep(tryagain);
	return;
    }
}



/* download anims, delete old anims, update cache */
void
do_download() {
    int i, j;
    int best_rating;
    time_t best_ctime = 0;
    int best_anim = -1;

    timestamp();

    update_server_anims();

    if (debug > 1)
	print_anims("nserver_anims", server_anims, nserver_anims);

    update_cached_anims(1);

    for (i = 0; i < nserver_anims; i++) {
	for (j = 0; j < ncached_anims; j++) {
	    if (server_anims[i].id == cached_anims[j].id)
		break;
	}
	if ((j == ncached_anims) &&
	    !cache_overflow((double)server_anims[i].size)) {
	    /* anim on the server fits in cache but is not in cache */
	    if (best_ctime == 0 ||
		(server_anims[i].rating > best_rating) ||
		(server_anims[i].rating == best_rating &&
		 server_anims[i].ctime < best_ctime)) {
		best_rating = server_anims[i].rating;
		best_ctime = server_anims[i].ctime;
		best_anim = i;
	    }
	}
    }
    if (debug)
	printf("best_anim=%d best_anim_id=%d "
	       "best_rating=%d best_ctime=%d\n",
	       best_anim, server_anims[best_anim].id,
	       best_rating, (int)best_ctime);
    if (-1 != best_anim) {
	delete_cached(server_anims[best_anim].size);
	download_anim(best_anim);
    } else {
	delete_cached(0);
	if (debug) printf("nothing to download, sleeping.\n");
	sleep(tryagain);
    }
}

void
make_download_process() {

    if (-1 == (downloader_pid = fork()))
	perror("downloader fork");
    else if (0 == downloader_pid) {
	/* child */
#ifdef HAVE_SETPROCTITLE
	setproctitle("download");
#endif
	thread_name = "download";
	sleep(init_delay_list);
	while (1) {
	    do_download();
	}
    }
    /* parent returns */
}


void
make_ui_process() {

  if (!on_root && !window_id)
    return;

    if (-1 == (ui_pid = fork()))
	perror("ui fork");
    else if (0 == ui_pid) {
	char vote_format[MAXBUF];

	execlp(vote_prog, vote_prog,
	       on_root ? "-3" : window_id,
	       nick_buf, url_buf, id_file, dream_server,
	       leave_prefix, uniqueid, NULL);
	perror("exec vote_prog");
    }
    /* parent returns */
}


void
make_render_process() {
    pid_t p;

    sleep(init_delay);

    while (nthreads-- > 1) { 
	if (-1 == (p = fork()))
	    perror("render fork");
	else if (0 == p) {
	    break;
	}
    }

    if (debug) printf("entering render loop %d\n", nthreads);
#ifdef HAVE_SETPROCTITLE
    setproctitle("render #%d", nthreads);
#endif
    thread_name = "render";
    while (1) {
	do_render();
    }
}

/* fork off two processes, one to traverse the graph of sheep, and the
   other to decode and display them.  the two are connected by
   pipes.  the decoder is an external program that we just exec. */

void
make_display_process()  {
    int h;
    int cnt;
    pid_t generator_pid;
    for (h = 0; h < history_size; h++)
	path_history[h] = -1;
    
    if (-1 == (displayer_pid = fork()))
	perror("displayer fork");
    else if (0 == displayer_pid) {
	/* child */
	int mpeg_fds[2];
	char fps[15];
	if (-1 == pipe(mpeg_fds)) {
	    perror("pipe1");
	    cleanup_and_exit(1);
	}
	if (-1 == (decoder_pid = fork())) {
	    perror("decoder fork");
	} else if (0 == decoder_pid) {
	    char *argv[10];
	    int c = 0;
	    char *wid;
	    /* child */
	    snprintf(fps, 14, "%d", frame_rate);
	    if (use_mplayer) {
	      int devnull = open("/dev/null", O_WRONLY);
	      if (-1 == devnull) {
		perror("/dev/null");
		cleanup_and_exit(1);
	      }
	      if (-1 == dup2(devnull, STDOUT_FILENO)) {
		perror("dup2a");
		cleanup_and_exit(1);
	      }
	      if (-1 == dup2(devnull, STDERR_FILENO)) {
		perror("dup2b");
		cleanup_and_exit(1);
	      }
		argv[c++] = "mplayer";
		argv[c++] = "-demuxer";
		argv[c++] = "1";
		argv[c++] = "-really-quiet";
		argv[c++] = "-fps";
		argv[c++] = fps;
		if (display_zoomed) {
		    argv[c++] = "-zoom";
		    argv[c++] = "-fs";
		}
		if (on_root) {
		    argv[c++] = "-rootwin";
		} else if (NULL != window_id) {
		    argv[c++] = "-wid";
		    argv[c++] = window_id;
		}
		argv[c++] = "-";
		argv[c++] = NULL;
		
	    } else {
		if (on_root && display_zoomed)
		    wid = "-3";
		else if (on_root)
		    wid = "-2";
		else if (NULL == window_id)
		    wid = "-1";
		else
		    wid = window_id;
		argv[c++] = play_prog;
		argv[c++] = "-f";
		argv[c++] = fps;
		argv[c++] = "-w";
		argv[c++] = wid;
		argv[c++] = NULL;
	    }
	    if (debug) {
		int i;
		printf("decoder execvp ");
		for (i = 0; i < c-1; i++) {
		    printf("%s ", argv[i]);
		}
		printf("\n");
	    }
	    if (-1 == dup2(mpeg_fds[0], STDIN_FILENO)) {
		perror("decoder child dup2 1");
		cleanup_and_exit(1);
	    }
	    execvp(play_prog, argv);
	    perror(play_prog);
	    cleanup_and_exit(1);
	} else {
#ifdef HAVE_SETPROCTITLE
	    setproctitle("display");
#endif
	    thread_name = "display";
	    mpeg_pipe = fdopen(mpeg_fds[1], "w");
	    if (NULL == mpeg_pipe) {
		perror("fdopen 1");
		cleanup_and_exit(1);
	    }

	    if ((generator_pid=fork())==0) {
	      
	      update_cached_anims(0);
	      cnt = 0;
	      while (1) {
		timestamp();
		do_display();
		if (!standalone && ((++cnt%cache_update_delay) == 0)) {
		  update_cached_anims(0);
		}
	      }
	      /* child never gets here */
	    } else {
	      /* wait end of decoder pid (kill window, e.g. screensaver stop on KDE) */
	      if (-1 == waitpid(decoder_pid, 0, 0)) {
		perror("waitpid downloader_pid");
		exit(1);
	      }	    
	      cleanup_and_exit(1);
	    }
	    
	}
    }
    /* parent returns */
}

/* set the number of threads by reading /proc/cpuinfo */
void
auto_nthreads() {
#ifndef _SC_NPROCESSORS_ONLN
    char line[MAXBUF];
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (NULL == f) goto def;
    nthreads = 0;
    while (fgets(line, MAXBUF, f)) {
	if (!strncmp("processor\t:", line, 11))
	    nthreads++;
    }
    fclose(f);
    if (nthreads < 1) goto def;
    return;
 def:
    fprintf(stderr,
	    "could not read /proc/cpuinfo, using one render thread.\n");
    nthreads = 1;
#else
    nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    if (nthreads < 1) nthreads = 1;
#endif
}

void parse_bracket(char *arg, int *bracket_id, time_t *bracket_time) {
  int i, id, n;
  if (0 == arg || 0 == arg[0]) return;
  n = strlen(arg);
  for (i = 0; i < n; i++) {
    if (!isdigit(arg[i])) {
      if (-1 == (*bracket_time = get_date(arg, NULL))) {
	fprintf(stderr, "warning: badly formated date ignored: %s\n", arg);
      }
      return;
    }
  }
  if (1 == sscanf(arg, "%d", &id)) {
    *bracket_id = id;
    return;
  }
  fprintf(stderr, "bad conversion of %s\n", arg);
}

#define iarg(oname, vname) \
if (!strcmp(oname, o)) { \
      if (*argc > 2) \
	vname = atoi((*argv)[2]); \
      else goto fail; \
      (*argc)-=2; \
      (*argv)+=2; \
    }

#define darg(oname, vname) \
if (!strcmp(oname, o)) { \
      if (*argc > 2) \
	vname = atof((*argv)[2]); \
      else goto fail; \
      (*argc)-=2; \
      (*argv)+=2; \
    }

#define sarg(oname, vname) \
if (!strcmp(oname, o)) { \
      if (*argc > 2) \
	vname = (*argv)[2]; \
      else goto fail; \
      (*argc)-=2; \
      (*argv)+=2; \
    }

char *help_string =
"electric sheep v%s - the collective dream of sleeping\n"
"                      computers from all over the internet.\n"
"                      see http://electricsheep.org\n"
"\n"
"usage: electricsheep [options]\n"
"\n"
"--nick name (credit frames to this name on the server, default is none)\n"
"--url url (in server credit, link name to this url, default is none)\n"
"--nthreads N (number of rendering threads, default is 1)\n"
"--frame-rate N (frames/second)\n"
"--timeout N (seconds, default is 401)\n"
"--tryagain N (seconds between retries to server, default is 696)\n"
"--server host/path (a hostname, possibly with a path, no leading\n"
"              \"http://\", default is %s)\n" 
"--display-anim 0/1 (invisibility if 0, default 1)\n"
"--standalone 0/1 (disables render & download, default 0)\n"
"--save-dir path (directory in which to save anims)\n"
"--reset-fuse N (maximum number of transitions before resetting\n"
"               to avoid loops, -1 means never, default is 300)\n"
"--max-megabytes N (maximum disk space used to save anims in megabytes,\n"
"               default is 1000, 0 means no limit)\n"
"--min-megabytes N (minimum disk space to leave free in megabytes,\n"
"               default is 100, 0 means no limit)\n"
"--nice n (priority adjustment for render process, default 10)\n"
"--nrepeats n (number of times to repeat loopable animations, default 2)\n"
"--proxy url (connect to server through proxy (see curl(1)))\n"
"--proxy-user user:password (proxy account (see curl(1)))\n"
"--start-sheep id (play this sheep first)\n"
"--root 0/1 (display on root window, default 0)\n"
"-window-id id (display in an existing window, note single dash!)\n"
"--debug 0/1 (prints additional info)\n"
"--voting 0/1 (enables voting, default 1)\n"
"--anim-only 0/1 (leave background untouched, default 0)\n"
"--mplayer 0/1 (use mplayer instead of the built-in decoder, default 0)\n"
"--zoom 0/1 (zoom to fullscreen, default 0)\n"
"--player exec_name (use the specified decoder)\n"
"--history N (length of history to keep in sheep, default 30)\n"
"--bracket-begin id/date (play no sheep before this one or this time)\n"
"--bracket-end id/date (play no sheep after this one or this time)\n"
"--data-dir dir (directory to find splash images and other data files)\n"
"--logfile file (file to write the log instead of stdout)\n"
;

void
flags_init(int *argc, char ***argv) {
    char *arg0 = (*argv)[0];
    char *bracket_begin = 0;
    char *bracket_end = 0;
    while (*argc > 1 &&
	   (*argv)[1][0] == '-') {
	char *o = (*argv)[1];
	if (!strcmp("--help", o)) {
	    printf(help_string, VERSION, dream_server);
	    exit(0);
	}
	else iarg("--frame-rate", frame_rate)
        else iarg("--timeout", timeout)
	else iarg("--tryagain", tryagain)
	else sarg("--server", dream_server)
	else sarg("--nick", nick_name)
	else sarg("--url", url_name)
	else iarg("--nice", nice_level)
	else sarg("--save-dir", leave_prefix)
	else iarg("--max-megabytes", leave_max_megabytes)
	else iarg("--min-megabytes", min_megabytes)
	else iarg("--reset-fuse", reset_fuse_length)
	else iarg("--display-anim", display_anim)
	else iarg("--parasite", parasite)
	else iarg("--standalone", standalone)
	else iarg("--nrepeats", nrepeats)
	else iarg("--max-repeats", max_repeats)
	else iarg("--nthreads", nthreads)
	else iarg("--debug", debug)
	else sarg("--player", play_prog)
	else sarg("--proxy", proxy_name)
	else sarg("--data-dir", splash_prefix)
	else iarg("--start-sheep", start_id)
	else sarg("--proxy-user", proxy_user)
	else sarg("-window-id", window_id)
	else iarg("--root", on_root)
	else iarg("--voting",voting)
	else iarg("--read-only",read_only)
	else iarg("--show-errors",show_errors)
	else iarg("--anim-only",nobg)
	else iarg("--mplayer",use_mplayer)
	else iarg("--zoom",display_zoomed)
	else iarg("--history",history_size)
	else sarg("--bracket-begin",bracket_begin)
	else sarg("--bracket-end",bracket_end)
	else sarg("--logfile",logfile)
	else {
	    fprintf(stderr, "bad option: %s, try --help\n", o);
	    exit(1);
	}
    }
    (*argv)[0] = arg0;

    parse_bracket(bracket_begin, &bracket_begin_id, &bracket_begin_time);
    parse_bracket(bracket_end, &bracket_end_id, &bracket_end_time);

    if (window_id && strlen(window_id) > 20) {
	fprintf(stderr, "window-id too long: %s.\n", window_id);
	exit(1);
    }

    if (leave_prefix && strlen(leave_prefix) > PATH_MAX) {
	fprintf(stderr, "save-dir too long: %s.\n", leave_prefix);
	exit(1);
    }
	
    if (splash_prefix && strlen(splash_prefix) > PATH_MAX) {
	fprintf(stderr, "data-dir too long: %s.\n", splash_prefix);
	exit(1);
    }

    if (proxy_user && strlen(proxy_user) > 100) {
	fprintf(stderr, "proxy-user too long: %s.\n", proxy_user);
	exit(1);
    }

    if (proxy_name && strlen(proxy_name) > 100) {
	fprintf(stderr, "proxy-name too long: %s.\n", proxy_name);
	exit(1);
    }

    if (dream_server && strlen(dream_server) > 100) {
	fprintf(stderr, "server too long: %s.\n", dream_server);
	exit(1);
    }

    if (nice_level < -20) {
	fprintf(stderr, "nice level must be -20 or greater, not %d.\n",
		nice_level);
	nice_level = -20;
    }
    if (nice_level > 19) {
	fprintf(stderr, "nice level must be 19 or less, not %d.\n",
		nice_level);
	nice_level = 19;
    }

    if (on_root && window_id) {
	on_root = 0;
    }

    if (window_id && display_zoomed) {
      display_zoomed = 0;
    }

    if (history_size <= 0) {
      fprintf(stderr, "history must be positive, not %d.\n",
	      history_size);
      exit(1);
    }
    path_history = malloc(sizeof(int) * history_size);

    if (-1 == nthreads) {
	nthreads = 1;
    } else if (0 > nthreads) {
        auto_nthreads();
    }

    if (debug) {
	hide_stderr = "";
    }

    if (!leave_prefix) {
	char b[MAXBUF];
	char *hom = getenv("HOME");
	if (!hom) {
	    fprintf(stderr, "HOME envar not defined\n");
	    cleanup_and_exit(1);
	}
	if (strlen(hom) > PATH_MAX) {
	    fprintf(stderr, "HOME envar too long: %s.\n", hom);
	    cleanup_and_exit(1);
	}
	snprintf(b, MAXBUF, "%s/.sheep/", hom);
	leave_prefix = strdup(b);
    } else if (leave_prefix[strlen(leave_prefix)-1] != '/') {
	char b[MAXBUF];
	snprintf(b, MAXBUF, "%s/", leave_prefix);
	leave_prefix = strdup(b);
    }

    snprintf(id_file, MAXBUF, "%sid", leave_prefix);

    if (1) {
	char b[MAXBUF];
	snprintf(b, MAXBUF, "mkdir -p %s", leave_prefix);
	mysystem(b, "mkdir leave prefix");
    }

    if (strlen(nick_name)*3 > bufmax-3) {
	fprintf(stderr, "nick_name too long.");
	cleanup_and_exit(1);
    }
    encode(nick_buf, nick_name);

    if (strlen(url_name)*3 > bufmax-3) {
	fprintf(stderr, "url_name too long.");
	cleanup_and_exit(1);
    }
    encode(url_buf, url_name);

    if (use_mplayer) {
	play_prog = "mplayer";
    }
    if (display_zoomed && !(use_mplayer || on_root) && display_anim) {
	fprintf(stderr, "warning: cannot zoom without mplayer or rootwin.\n");
    }

    return;
 fail:
    fprintf(stderr, "no argument to %s\n", (*argv)[1]);
    exit(1);
}

void
do_lock() {
    char fn[MAXBUF];
    int fd;

    struct flock fl;
  
    snprintf(fn, MAXBUF, "%slock", leave_prefix);
    if (-1 == (fd = creat(fn, S_IRWXU))) {
	perror(fn);
	cleanup_and_exit(1);
    }

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (-1 == fcntl(fd, F_SETLK, &fl)) {
	if ((EAGAIN == errno) || (EACCES == errno)) {
	    fprintf(stderr, "detected another electricsheep process.\n"
		    "using read only access, ie disabling "
		    "downloading of sheep.\n");
	    read_only = 1;
	} else {
	    perror(fn);
	    cleanup_and_exit(1);
	}
    }
    /* leave the file open & locked until our process terminates */
}

/* 64 random bits encoded as ascii */
void set_uniqueid() {
  static char *rdevice = "/dev/urandom";
  long d[2];
  int i, rfd;
  struct timeval tv;

  if (debug) printf("setting unique id.\n");
  rfd = open(rdevice, 0);
  if (-1 == rfd) {
    perror(rdevice);
    exit(1);
  }
  if (8 != read(rfd, (void *) d, 8)) {
    perror(rdevice);
    exit(1);
  }
  if (-1 == gettimeofday(&tv, NULL)) {
    perror("gettimeofday");
    exit(1);
  }
  d[0] ^= tv.tv_sec;
  d[1] ^= tv.tv_usec;
  snprintf(uniqueid, uniqueid_len+1, "%08X%08X", d[0], d[1]);
}

int
main(int argc, char **argv) {
    int n = 0;

    // would like to write this into argv[0] so it shows
    // up in ps output, but not clear to me how to allocate
    // the space for that.  assigning argv[0] doesn't work.
    thread_name = "main";

    /* create our own group so all workers/children may
       be killed together without hassle */
#ifdef SETPGRP_VOID
    if (-1 == setpgrp()) perror("setpgrp");
#else
    if (-1 == setpgrp(getpid(), getpid())) perror("setpgrp");
#endif
    signal(SIGTERM, handle_sig_term);
    signal(SIGINT, handle_sig_term);

    flags_init(&argc, &argv);

    if (1) {
      /* read rc file */
      FILE *frc;
      char tbuf[MAXBUF];
      char pbuf[MAXBUF];
      int changed = 0;

      snprintf(pbuf, MAXBUF, "%src", leave_prefix);
      frc = fopen(pbuf, "rb");
      if (NULL != frc) {
	int tlmm;
	if (1 == fscanf(frc, "%d\n", &tlmm)) {
	  if (-1 == leave_max_megabytes) {
	    leave_max_megabytes = tlmm;
	  } else if (leave_max_megabytes != tlmm) {
	    changed = 1;
	  }
	} else {
	  fprintf(stderr, "warning: could not parse line 1 of %s. fixed.\n",
		  pbuf);
	  changed = 1;
	}
	uniqueid[0] = 0;
	if (NULL == fgets(uniqueid, uniqueid_len+1, frc) ||
	    strlen(uniqueid)!=uniqueid_len) {
	  fprintf(stderr, "warning: could not parse line 2 of %s. fixed.\n",
		  pbuf);
	  set_uniqueid();
	  changed = 1;
	}
	fclose(frc);
      } else {
	set_uniqueid();
	changed = 1;
      }

      if (0 > leave_max_megabytes) {
	leave_max_megabytes = 1000;
	changed = 1;
      }

      /* write rc file */
      if (changed) {
	  snprintf(tbuf, MAXBUF, "%src.tmp", leave_prefix);
	if (frc = fopen(tbuf, "wb")) {
	  fprintf(frc, "%d\n%s\n", leave_max_megabytes, uniqueid);
	  fclose(frc);
	  if (-1 == rename(tbuf, pbuf)) {
	    perror(pbuf);
	  }
	} else {
	  perror(tbuf);
	}	  
      }
    }


    if (logfile && logfile[0]) {
	if (NULL == freopen(logfile, "a", stdout)) {
	    perror(logfile);
	}
	setlinebuf(stdout);
	if (NULL == freopen(logfile, "a", stderr)) {
	    perror(logfile);
	}
	setlinebuf(stderr);
    }

    if (debug) {
	printf("=====================================\n"
	       "electric sheep v%s\n", VERSION);
	timestamp();
    }

    do_lock();

    reset_fuse = reset_fuse_length;

    if (proxy_name) {
	snprintf(curl_cmd, MAXBUF, "nice -n %d curl --proxy %s",
		nice_level, proxy_name);
	if (proxy_user) {
	  strcat(curl_cmd, " --proxy-user ");
	  strcat(curl_cmd, proxy_user);
	}
    } else
	snprintf(curl_cmd, MAXBUF, "nice -n %d curl", nice_level);
    strcat(curl_cmd, " --silent");
    if (show_errors) {
	strcat(curl_cmd, " --show-error");
    }

    srandom(time(0));

    not_playing();

    if (display_anim) {
        make_display_process();
	if (voting)
	    make_ui_process();
    }
    if (!standalone && !read_only) make_download_process();

    if (!parasite && !standalone)
	make_render_process(); // does not return

    if (displayer_pid) {
	if (-1 == waitpid(displayer_pid, 0, 0)) {
	    perror("waitpid displayer_pid");
	    exit(1);
	}
    }
    if (downloader_pid) {
	if (-1 == waitpid(displayer_pid, 0, 0)) {
	    perror("waitpid downloader_pid");
	    exit(1);
	}
    }
    if (ui_pid) {
	if (-1 == waitpid(ui_pid, 0, 0)) {
	    perror("waitpid ui_pid");
	    exit(1);
	}
    }
    exit(0);
}
