/*
    electricsheep - collaborative screensaver
    Copyright (C) 1999-2001 Scott Draves <source@electricsheep.org>

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
"@(#) $Id: electricsheep.c,v 1.26 2004/04/13 23:29:33 spotspot Exp $";

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

// should be PACKAGE_DATA_DIR
char *splash_prefix = "/usr/local/share";

char *proxy_name = 0;
char *proxy_user = 0;

char *leave_prefix = NULL;
int leave_max_megabytes = 300;
int min_megabytes = 100;
int reset_fuse_length = 300;

// ick xxx
#define max_anims 10000

#define copy_buffer_size 64000

int generation = -1;

int debug = 0;
int stats = 0;

int nrestarts = 0;
int nplays = 0;
int nloopplays = 0;
int reset_fuse = 0;

int nplays_by_id[max_anims];

int nrepeats = 2;
int nthreads = -1;

char *dream_server = "v2d5.sheepserver.net";

// cheeze XXX
#define max_cp_size 300000
#define MAXBUF (5*MAXPATHLEN)


char *play_prog = "mpeg2dec_onroot";
char *nick_name = "";
char *url_name = "";

// would be good to figure out xscreensaver/redhat/suse etc too?  how?
char *client_version = "LNX_" VERSION;

char cps_name[PATH_MAX] = "";
char jpg_name[PATH_MAX] = "";
char mpg_name[PATH_MAX] = "";
char fifo_name[PATH_MAX] = "";

char curl_cmd[MAXBUF];

int history_size = 30;
int *path_history;
int last_sheep = -1;
int nrepeated = 0;
int max_repeats = 10;


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
int jpeg_quality = 90;

char *hide_stderr = "2> /dev/null";

int display_anim = 1;
int voting = 0;
int nobg = 0;
int parasite = 0;
int standalone = 0;
int read_only = 0;

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
    int id;
    int deleted;
    int type;
    time_t ctime;
    int size;
    int rating;
    int first;
    int last;
    char url[max_url_length];
} anim_t;


int nserver_anims;
int ncached_anims;

anim_t server_anims[max_anims];
anim_t cached_anims[max_anims];

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
	printf("id=%d deleted=%d type=%d ctime=%d size=%d rating=%d "
	       "first=%d last=%d url=%s\n",
	       an[i].id, an[i].deleted, an[i].type, an[i].ctime,
	       an[i].size, an[i].rating, an[i].first, an[i].last,
	       an[i].url);
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

void
update_cached_anims(int match_gen) {
    int write_gen = 0;
    int checked_gen = 0;
    int clear_gen = 0;
    DIR *d = opendir(leave_prefix);
    struct dirent *e;
    struct stat sbuf;
    char fbuf[MAXBUF];

    if (!d) {
	perror(leave_prefix);
	cleanup_and_exit(1);
    }
    ncached_anims = 0;
    while (e = readdir(d)) {
	anim_t *an = &cached_anims[ncached_anims];
	int i;
	if (match_gen) {
	    int old_gen = -1;
	    if (0 == strncmp("gen", e->d_name, 3)) {
		sscanf(e->d_name, "gen%d", &old_gen);
		checked_gen = 1;
		if (old_gen != generation) {
		    write_gen = 1;
		    clear_gen = 1;
		}
	    }
	}
	if (filename_is_xxx(e->d_name)) {
	    if (3 != sscanf(e->d_name, "%d=%d=%d.xxx",
			    &an->id, &an->first, &an->last)) {
		continue;
	    }
	    an->deleted = 1;
	    for (i = 0; i < nserver_anims; i++) {
		if (server_anims[i].id == an->id) {
		    break;
		}
	    }
	    if (i == nserver_anims && nserver_anims) {
		sprintf(fbuf, "%s%s", leave_prefix, e->d_name);
		if (debug)
		    printf("removing marker %s nserver_anims=%d\n",
			   fbuf, nserver_anims);
		unlink(fbuf);
		continue;
	    }
	    ncached_anims++;
	    continue;
	}
 
	if (!filename_is_mpg(e->d_name)) continue;
	sprintf(fbuf, "%s%s", leave_prefix, e->d_name);

	if (3 != sscanf(e->d_name, "%d=%d=%d.mpg",
			&an->id, &an->first, &an->last)) {
	    continue;
	}
	    
	if (-1 == stat(fbuf, &sbuf)) continue;

	an->rating = 0;
	for (i = 0; i < nserver_anims; i++) {
	    if (server_anims[i].id == an->id) {
		an->rating = server_anims[i].rating;
		break;
	    }
	}

	an->deleted = 0;
	an->size = sbuf.st_size;
	an->ctime = sbuf.st_ctime;
	an->url[0] = 0;
	an->type = 0;
	ncached_anims++;
    }
    closedir(d);
    if (debug > 1)
	print_anims("ncached_anims", cached_anims, ncached_anims);

    if (match_gen && !checked_gen) write_gen = 1;
	
    if (clear_gen) {
	DIR *d = opendir(leave_prefix);
	if (debug) printf("killing all sheep!\n");
	if (!d) {
	    perror(leave_prefix);
	    cleanup_and_exit(1);
	}
	while (e = readdir(d)) {
	    if ((0 == strncmp("gen", e->d_name, 3)) ||
		filename_is_mpg(e->d_name) ||
		filename_is_xxx(e->d_name)) {
		sprintf(fbuf, "%s%s", leave_prefix, e->d_name);
		unlink(fbuf);
	    }
	}
	closedir(d);
    }
	
    if (write_gen) {
	if (debug) printf("write gen %d.\n", generation);
	sprintf(fbuf, "touch %sgen%d", leave_prefix, generation);
	mysystem(fbuf, "touch gen");
    }
}

static int
irandom(int n) {
    return random()%n;
}

void
cached_file_name(char *buf, anim_t *an) {
    sprintf(buf, "%s%05d=%05d=%05d.mpg",
	    leave_prefix, an->id, an->first, an->last);
}

void
deleted_file_name(char *buf, anim_t *an) {
    sprintf(buf, "%s%05d=%05d=%05d.xxx",
	    leave_prefix, an->id, an->first, an->last);
}

void
not_playing() {
    char pbuf[MAXBUF];
    if (!read_only) {
	sprintf(pbuf, "echo none > %sid", leave_prefix);
	mysystem(pbuf, "writing none to id file");
    }
}    

void
default_background(char *more) {
    char ob[MAXBUF];
    char pbuf[MAXBUF];
    char qbuf[MAXBUF];

    if (nobg || (!on_root && !window_id)) return;
    if (more)
	sprintf(ob, "-merge -at 500,0 %s/electricsheep-%s.tif",
		splash_prefix, more);
    else
	ob[0] = 0;

    if (window_id)
      sprintf(qbuf, "-windowid %s", window_id);
    else
      qbuf[0] = 0;

    sprintf(pbuf, "xsetbg %s "
	    "-border black -at 0,0 %s/electricsheep-splash-0.tif " 
	    "-merge -center %s/electricsheep-splash-1.tif %s",
	    qbuf, splash_prefix, splash_prefix, ob);
    mysystem2(pbuf, "splash0");
}

#define max_plays 1000

void
print_stats() {
    int i, n, j;

    int buck[max_plays];

    for (i = 0; i < max_plays; i++)
	buck[i] = 0;

    printf("nplays = %d, ", nplays);
    printf("nloopplays = %d, ", nloopplays);
    printf("nrestarts = %d, ", nrestarts);
    printf("ncached_anims = %d\n", ncached_anims);
     
    printf("nplays_by_id:");
    for (i = 0; i < max_anims; i++) {
	if (nplays_by_id[i] != 0) {
	    printf(" %d:%d", i, nplays_by_id[i]);
	    if (nplays_by_id[i] < max_plays)
		buck[nplays_by_id[i]]++;
	}
    }
    printf("\n");
    
    printf("zeroes:");
    for (i = 0; i < ncached_anims; i++) {
	if (0 == nplays_by_id[cached_anims[i].id]) {
	    printf(" %d", cached_anims[i].id);
	    buck[0]++;
	}
    }
    printf("\n");

    n = 0;
    for (i = 0; i < max_plays; i++) {
	if (buck[i] > 0) n = i;
    }
    
    printf("histogram:\n");
    for (i = 0; i <= n; i++) {
	printf("%4d %4d ", i, buck[i]);
	for (j = 0; j < buck[i]; j++)
	    putchar('o');
	printf("\n");
    }
}

/* traverse the graph of anims, and writes the mpeg files into stdin
   of the child.  another process decodes the mpeg and displays it on
   the screen. */

void
do_display() {
    int i;
    int idx;
    int niters;
	
    update_cached_anims(0);

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
    if (-1 == idx) {
        int n = 0;
        for (i = 0; i < ncached_anims; i++) {
	    if (!cached_anims[i].deleted) n++;
        }
	if (0 == n) {
	    if (debug) printf("nothing to play, sleeping.\n");
	    not_playing();
	    default_background(0);
	    sleep(10);
	    return;
	}
	idx = irandom(ncached_anims);
	while (cached_anims[idx].deleted)
	    idx = (idx+1)%ncached_anims;
	
	current_anim_id = cached_anims[idx].id;
	if (debug) printf("found new anim id=%d.\n", current_anim_id);
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
	char fbuf[MAXBUF];
	FILE *idf;

	if (debug) printf("play anim x=%d id=%d iters=%d.\n",
			  idx, current_anim_id, niters);
	if (stats) print_stats();

	nplays_by_id[current_anim_id]++;
	nplays++;
	if (cached_anims[idx].first == cached_anims[idx].last) {
	    nloopplays++;
	}

	sprintf(fbuf, "%sid.tmp", leave_prefix);
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
	sprintf(pbuf, "%sid", leave_prefix);
	if (-1 == rename(fbuf, pbuf)) {
	    fprintf(stderr, "rename %s %s\n", fbuf, pbuf);
	    perror(pbuf);
	    goto skipped;
	}

	for (h = history_size - 1; h > 0; h--) {
	    path_history[h] = path_history[h-1];
	}
	path_history[0] = current_anim_id;

	if (debug) {
	    printf("history =");
	    for (h = 0; h < history_size; h++)
		printf(" %d", path_history[h]);
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
    
    if (0 == reset_fuse-- || nrepeated >= max_repeats) {
      if (debug) printf("reset nrepeated=%d reset_fuse=%d\n", nrepeated, reset_fuse);
      current_anim_id = -1;
      reset_fuse = reset_fuse_length;
    } else {
	// pick next anim at random from all possible succs,
	// trying to avoid repeats, and giving priority to loops.
	int succs[max_anims];
	int lsuccs[max_anims];
	int nsuccs = 0;
	int lnsuccs = 0;
	int sym = cached_anims[idx].last;
	int h;
    

	for (h = history_size; h >= 0; h--) {
	    for (i = 0; i < ncached_anims; i++) {
		if (!cached_anims[i].deleted &&
		    cached_anims[i].first == sym) {
		    int hh;
		    for (hh = 0; hh < h; hh++) {
			if (path_history[hh] == cached_anims[i].id)
			    break;
		    }
		    if (hh == h) {
			if (debug) printf("succ x=%d id=%d h=%d\n",
					  i, cached_anims[i].id, h);
			if (cached_anims[i].first == cached_anims[i].last)
			    lsuccs[lnsuccs++] = i;
			else
			    succs[nsuccs++] = i;
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
	    if (cached_anims[i].deleted) continue;
	    if (debug) printf("delete cached total=%g id=%d rating=%d\n",
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
	    unlink(buf);
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
    sprintf(tfb, "%s.tmp", cfb);
    strcpy(mpg_name, tfb);
    
    sprintf(pbuf, "%s --silent --show-error --output %s %s",
	    curl_cmd, tfb, server_anims[idx].url);

    if (debug) printf("about to %s\n", pbuf);

    mysystem(pbuf, "anim download");

    if (-1 == stat(tfb, &sbuf)) {
	fprintf(stderr, "download failed of sheep %d\n",
		server_anims[idx].id);
	unlink(tfb);
	mpg_name[0] = 0;
	sleep(tryagain);
	return;
    }
    if (sbuf.st_size != server_anims[idx].size) {
	fprintf(stderr, "incomplete sheep id=%d got=%ld want=%ld\n",
		server_anims[idx].id, (long)sbuf.st_size,
		(long)server_anims[idx].size);
	unlink(tfb);
	mpg_name[0] = 0;
	sleep(tryagain);
	// xxx limited retries
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
  
    sprintf(pbuf, "%s --max-time %d --silent --show-error 'http://%s/cgi/get.cgi?"
	    "nick=%s&url=%s&version=%s' | gunzip -c %s",
	    curl_cmd, timeout, dream_server, nick_buf,
	    url_buf, client_version, hide_stderr);

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

    sprintf(pbuf, "%s --silent --max-time %d --upload-file %s "
	    "'http://%s/cgi/put.cgi?frame=%d&id=%d&size=%ld&gen=%d&version=%s'",
	    curl_cmd, timeout, fname, dream_server, frame, anim_id,
	    (long)sbuf.st_size, gen, client_version);
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

void
do_render() {
    char anim_text[max_cp_size];
    char *cp_text, *image;
    int this_size;
    char prog[101];
    int cps_fd, jpg_fd;
    FILE *fp;
    XML_Parser parser;

    get_control_points(anim_text, max_cp_size);

    if (0 == anim_text[0]) {
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
	fprintf(stderr, "server reported error for get: %s\n", server_error_type);
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
	char strips[MAXBUF];
	sprintf(b, "nice -%d env in=%s time=%d out=%s quality=%d anim-flame",
		nice_level, cps_name, get_frame, jpg_name, jpeg_quality);
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

/* runs voting, passes votes from xscreensaver to server.
   reads from a named pipe ~/.sheep/vote-fifo */
void
do_ui() {
    char vf_name[MAXBUF];
    char gb[MAXBUF];
    FILE *vf;

    sprintf(fifo_name, "%svote-fifo", leave_prefix);
    unlink(fifo_name);
	
    if (-1 == mkfifo(fifo_name, S_IRWXU)) {
	fprintf(stderr, "error making fifo.\n");
	perror(fifo_name);
	cleanup_and_exit(1);
    }

    vf = fopen(fifo_name, "r");
    if (NULL == vf) {
	fprintf(stderr, "error opening fifo.\n");
	perror(fifo_name);
	cleanup_and_exit(1);
    }

    while (NULL != fgets(gb, MAXBUF, vf)) {
	char pbuf[MAXBUF];
	char idfn[MAXBUF];
	int id, vote;
	FILE *idf;
	vote = atoi(gb);
	sprintf(idfn, "%sid", leave_prefix);
	idf = fopen(idfn, "r");
	if (NULL == idf) {
	    fprintf(stderr, "unable to open current id file.\n");
	    perror(idfn);
	    continue;
	}
	if (NULL == fgets(gb, MAXBUF, idf)) {
	    fprintf(stderr, "unable to read current id file.\n");
	    perror(idfn);
	    continue;
	}
	fclose(idf);

	if (0 == strcmp(gb, "none\n")) {
	    fprintf(stderr,
		    "ignored because there is nothing to vote for.\n");
	    continue;
	}

	id = atoi(gb);

	if (vote == 0) {
	    if (debug) printf("not voting zero\n");
	} else {
	    sprintf(pbuf, "%s --max-time %d --silent --show-error "
		    "'http://%s/cgi/vote.cgi?id=%d&vote=%d'",
		    curl_cmd, timeout, dream_server, id, vote);
	    if (debug) printf("voting %s\n", pbuf);
	    mysystem2(pbuf, "curl vote");
	    default_background((vote > 0) ? "smile" : "frown");
	    sleep(3);
	    default_background(0);
	}
    }

    fclose(vf);
    unlink(fifo_name);
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
	    if (max_anims == nserver_anims+1) {
		fprintf(stderr, "too many sheep.\n");
	    } else
		nserver_anims++;
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

/* download anims, delete old anims, update cache */
void
do_download() {
    char pbuf[MAXBUF];
    char state[MAXBUF];
    FILE *lf;
    int done;
    int i, j;
    int best_rating;
    time_t best_ctime = 0;
    int best_anim = -1;
	
    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, list_start_element, get_end_element);
    XML_SetCharacterDataHandler(parser, character_handler);
    in_message = 0;
    server_error = 0;
    
    sprintf(pbuf, "%s --max-time %d --silent --show-error 'http://%s/cgi/list.cgi'"
	    "| gunzip -c %s", curl_cmd, timeout, dream_server,
	    hide_stderr);

    if (debug) printf("list %s\n", pbuf);

    lf = popen(pbuf, "r");

    if (NULL == lf) {
	perror("could not fork/pipe\n");
	cleanup_and_exit(1);
    }

    nserver_anims = 0;
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
	fprintf(stderr,
		"lost contact with %s, cannot retrieve sheep.\n",
		dream_server);
	sleep(tryagain);
	return;
    }

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
	while (1) {
	    do_download();
	}
    }
    /* parent returns */
}

void
make_ui_process() {
    
    if (-1 == (ui_pid = fork()))
	perror("ui fork");
    else if (0 == ui_pid) {
	/* child */
#ifdef HAVE_SETPROCTITLE
	setproctitle("ui");
#endif
	thread_name = "ui";
	while (1) {
	    do_ui();
	}
    }
    /* parent returns */
}

void
make_render_process() {
    pid_t p;

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
	    sprintf(fps, "%d", frame_rate);
	    if (use_mplayer) {
		argv[c++] = "mplayer";
		argv[c++] = "-fps";
		argv[c++] = fps;
		if (display_zoomed) {
		    argv[c++] = "-zoom";
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
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);
	    execvp(play_prog, argv);
	    perror("exec play_prog");
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
	    while (1) {
		do_display();
	    }
	    /* child never gets here */
	}
    }
    /* parent returns */
}


void
logo_only() {
  char pbuf[MAXBUF];
  char qbuf[MAXBUF];
  if (!on_root && !window_id) return;
  if (window_id)
    sprintf(qbuf, "-windowid %s", window_id);
  else
    qbuf[0] = 0;
  
  sprintf(pbuf, "xsetbg %s -at 0,0 %s/electricsheep-splash-1.tif",
	  qbuf, splash_prefix);
  mysystem2(pbuf, "logo");
  while (1) sleep(60);
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
"--nthreads N (number of rendering threads, default is same\n"
"              as the number of CPUs)\n"
"--frame-rate N (frames/second)\n"
"--timeout N (seconds, default is 401)\n"
"--tryagain N (seconds between retries to server, default is 396)\n"
"--server host/path (a hostname, possibly with a path, no leading\n"
"              \"http://\", default is %s)\n" 
"--display-anim 0/1 (invisibility if 0, default 1)\n"
"--standalone 0/1 (disables render & download, default 0)\n"
"--save-dir path (directory in which to save anims)\n"
"--reset-fuse N (maximum number of transitions before resetting\n"
"               to avoid loops, -1 means never, default is 300)\n"
"--max-megabytes N (maximum disk space used to save anims in megabytes,\n"
"               default is 100, 0 means no limit)\n"
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
"--voting 0/1 (enables voting, default 0)\n"
"--anim-only 0/1 (leave background untouched, default 0)\n"
"--mplayer 0/1 (use mplayer instead of the built-in decoder, default 0)\n"
"--zoom 0/1 (zoom to fullscreen, default 0)\n"
"--player exec_name (use the specified decoder)\n"
"--history N (length of history to keep in sheep, default 30)\n"
;

void
flags_init(int *argc, char ***argv) {
    char *arg0 = (*argv)[0];
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
	else iarg("--start-sheep", start_id)
	else sarg("--proxy-user", proxy_user)
	else sarg("-window-id", window_id)
	else iarg("--root", on_root)
	else iarg("--voting",voting)
	else iarg("--anim-only",nobg)
	else iarg("--mplayer",use_mplayer)
	else iarg("--zoom",display_zoomed)
	else iarg("--history",history_size)
	else {
	    fprintf(stderr, "bad option: %s, try --help\n", o);
	    exit(1);
	}
    }
    (*argv)[0] = arg0;

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
	  
	sprintf(b, "%s/.sheep/", hom);
	leave_prefix = strdup(b);
    } else if (leave_prefix[strlen(leave_prefix)-1] != '/') {
	char b[MAXBUF];
	sprintf(b, "%s/", leave_prefix);
	leave_prefix = strdup(b);
    }

    if (1) {
	char b[MAXBUF];
	sprintf(b, "mkdir -p %s", leave_prefix);
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
  
    sprintf(fn, "%slock", leave_prefix);
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

    if (1) {
	int i;
	for (i = 0; i < max_anims; i++)
	    nplays_by_id[i] = 0;
    }
	
    flags_init(&argc, &argv);

    do_lock();

    reset_fuse = reset_fuse_length;

    if (proxy_name) {
	sprintf(curl_cmd, "nice -%d curl --proxy %s",
		nice_level, proxy_name);
	if (proxy_user) {
	  strcat(curl_cmd, " --proxy-user ");
	  strcat(curl_cmd, proxy_user);
	}
    } else
	sprintf(curl_cmd, "nice -%d curl", nice_level);

    srandom(time(0));

    not_playing();
    default_background(0);

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
