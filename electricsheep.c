/*
    electricsheep - collaborative screensaver
    Copyright (C) 1999-2001 Scott Draves <spot@draves.org>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/param.h>
#include "config.h"

// should get this from configure, note: this should have / at the end
char *splash_prefix = "/usr/share/electricsheep/";

char *proxy_name = 0;

char *leave_prefix = NULL;
int leave_max_megabytes = 1000;
int reset_fuse_length = 300;

#define max_anims 10000

int generation = -1;

int debug = 0;
int stats = 0;

int nrestarts = 0;
int nplays = 0;
int nloopplays = 0;
int reset_fuse = 0;

int nplays_by_id[max_anims];

int nrepeats = 2;
int nthreads = 1;

char *dream_server = "electricsheep.org/v" VERSION;

#define max_cp_size 30000
#define MAXBUF (5*MAXPATHLEN)


char *play_prog = "mpeg-loop";
char *nick_name = "";
char *url_name = "";

char *cps_name = "", *ppm_name = "", *jpg_name = "", *mpeg_name = "";
char fifo_name[MAXBUF] = {0};

char curl_cmd[MAXBUF];

#define history_size 3
int path_history[history_size];


// by default xscreensaver starts us at priority 10.
// everything but the mpeg decoder runs at that (10) plus this:
int nice_level = 5;


#define bufmax 300
char nick_buf[bufmax];
char url_buf[bufmax];

int frame_rate = 23;
char *window_id = NULL;
int timeout = 201;
int tryagain = 96;

int display_anim = 1;
int parasite = 0;
int standalone = 0;

pid_t displayer_pid = 0;
pid_t downloader_pid = 0;
pid_t decoder_pid = 0;
pid_t ui_pid = 0;

int current_anim_id = -1;

typedef struct {
    int id;
    int type;
    time_t ctime;
    int size;
    int rating;
    int first;
    int last;
    char url[1001];
} anim_t;


int nserver_anims;
int ncached_anims;

anim_t server_anims[max_anims];
anim_t cached_anims[max_anims];

char *thread_name = NULL;

void
cleanup() {
    if (debug) printf("cleanup.\n");
    unlink(mpeg_name);
    unlink(cps_name);
    unlink(ppm_name);
    unlink(jpg_name);
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
      fprintf(stderr, "subprocess error: %s, %d=%d<<8+%d\n", msg, n, n>>8, n&255);
	  cleanup_and_exit(1);
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
	    fprintf(stderr, "subprocess failure: %s, %d=%d<<8+%d\n", msg, n, n>>8, n&255);
	else
	    fprintf(stderr, "control-c during %s\n", msg);
    }
}


static void
encode(char *dst, char *src) {
  static char *hex = "0123456789ABCDEF";
  char t;
  while (t = *src++) {
    *dst++ = '%';
    *dst++ = hex[(t >> 4) & 15];
    *dst++ = hex[t & 15];
  }
}

void
print_anims(char *name, anim_t *an, int nanims) {
    int i;
    printf("%s=%d\n", name, nanims);
    
    // return;
    
    for (i = 0; i < nanims; i++) {
	printf("id=%d type=%d ctime=%d size=%d rating=%d first=%d last=%d url=%s\n",
	       an[i].id, an[i].type, an[i].ctime, an[i].size, an[i].rating,
	       an[i].first, an[i].last, an[i].url);
    }
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
	if (!filename_is_mpg(e->d_name)) continue;
	sprintf(fbuf, "%s%s", leave_prefix, e->d_name);
	// check error code xxx
	sscanf(e->d_name, "%d=%d=%d.mpg",
	       &an->id, &an->first, &an->last);
	    
	if (-1 == stat(fbuf, &sbuf)) continue;

	an->rating = 0.0;
	for (i = 0; i < nserver_anims; i++) {
	    if (server_anims[i].id == an->id) {
		an->rating = server_anims[i].rating;
		break;
	    }
	}

	an->size = sbuf.st_size;
	an->ctime = sbuf.st_ctime;
	an->url[0] = 0;
	an->type = 0;
	ncached_anims++;
    }
    closedir(d);
    if (debug) print_anims("ncached_anims", cached_anims, ncached_anims);

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
		filename_is_mpg(e->d_name)) {
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
    sprintf(buf, "%s%04d=%04d=%04d.mpg",
	    leave_prefix, an->id, an->first, an->last);
}

void
not_playing() {
    char pbuf[MAXBUF];
    sprintf(pbuf, "echo none > %sid", leave_prefix);
    mysystem(pbuf, "writing none to id file");
}    

void
default_background(char *more) {
    char ob[MAXBUF];
    char pbuf[MAXBUF];

    if (more)
	sprintf(ob, "-merge -at 500,0 %selectricsheep-%s.tif",
		splash_prefix, more);
    else
	ob[0] = 0;

    sprintf(pbuf, "xsetbg "
	    "-border black -at 0,0 %selectricsheep-splash-0.tif " 
	    "-merge -center %selectricsheep-splash-1.tif %s",
	    splash_prefix, splash_prefix, ob);
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

/* traverse graph of anims, fork an mpeg decoder, wait for it to finish, repeat */
void
do_display() {
    int i;
    int idx;
    int niters;
	
    update_cached_anims(0);

    idx = -1;
    if (-1 != current_anim_id) {
	for (i = 0; i < ncached_anims; i++) {
	    if (cached_anims[i].id == current_anim_id) {
		idx = i;
		break;
	    }
	}
    }
    if (-1 == idx) {
	if (0 == ncached_anims) {
	    if (debug) printf("nothing to play, sleeping.\n");
	    not_playing();
	    default_background(0);
	    sleep(tryagain);
	    return;
	}
	idx = irandom(ncached_anims);
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
	int decoder_status;
	int h;
	char pbuf[MAXBUF];
	char fbuf[MAXBUF];
	FILE *idf;

	if (debug) printf("play anim x=%d id=%d iters=%d.\n", idx, current_anim_id, niters);
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
	fclose(idf);
	sprintf(pbuf, "%sid", leave_prefix);
	if (-1 == rename(fbuf, pbuf)) {
	    fprintf(stderr, "rename %s %s\n", fbuf, pbuf);
	    perror(pbuf);
	    goto skipped;
	}

    skipped:
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
	sprintf(pbuf, "%s -root -dither color2 -iters %d -framerate %d %s",
		play_prog, niters, frame_rate, fbuf);
	mysystem(pbuf, "play decoder");
    }

    if (0 == reset_fuse--) {
	if (debug) "reset fuse\n";
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
		if (cached_anims[i].first == sym) {
		    int hh;
		    for (hh = 0; hh < h; hh++) {
			if (path_history[hh] == cached_anims[i].id)
			    break;
		    }
		    if (hh == h) {
			if (debug) printf("succ x=%d id=%d h=%d\n", i, cached_anims[i].id, h);
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
	    if (debug) printf("lsucc to %d, lnsuccs=%d.\n", current_anim_id, lnsuccs);
	} else if (nsuccs) {
	    idx = succs[irandom(nsuccs)];
	    current_anim_id = cached_anims[idx].id;
	    if (debug) printf("succ to %d, nsuccs=%d.\n", current_anim_id, nsuccs);
	} else {
	    if (debug) printf("no succ.\n");
	    current_anim_id = -1;
	    nrestarts++;
	}
    }
}



/* make enough room in cache to download SIZE more bytes */
void
delete_cached(int size) {
    int i;
    int total;
    char buf[MAXBUF];
    time_t oldest_time = 0;
    int worst_rating;
    int best;

    while (1) {

	total = size;
    
	for (i = 0; i < ncached_anims; i++) {
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
	if (total > (1000 * 1000 * leave_max_megabytes)) {
	    cached_file_name(buf, &cached_anims[best]);
	    if (debug) printf("deleting %s\n", buf);
	    unlink(buf);
	    for (i = best; i < ncached_anims; i++) {
		cached_anims[i] = cached_anims[i+1];
	    }
	    ncached_anims--;
	} else {
	    return;
	}
    }
}



void
download_anim(int idx) {
    char pbuf[MAXBUF];
    char tfb[MAXBUF];
    char cfb[MAXBUF];
    struct stat sbuf;
    char *p;

    if (debug) printf("download %d\n", idx);
    
    cached_file_name(cfb, &server_anims[idx]);
    sprintf(tfb, "%s.tmp", cfb);
    
    sprintf(pbuf, "%s --silent %s > %s",
	    curl_cmd, server_anims[idx].url, tfb);

    if (debug) printf("about to %s\n", pbuf);

    mysystem(pbuf, "anim download");

    if (-1 == stat(tfb, &sbuf)) {
	fprintf(stderr, "download failed of sheep %d\n", server_anims[idx].id);
	return;
    }
    if (sbuf.st_size != server_anims[idx].size) {
	fprintf(stderr, "incomplete sheep %d %d %d\n",
		server_anims[idx].id, sbuf.st_size, server_anims[idx].size);
	unlink(tfb);
	sleep(tryagain);
	// xxx limited retries
	return;
    }

    if (-1 == rename(tfb, cfb)) {
	perror("move download temp to cache");
	fprintf(stderr, "move %s to %s\n", tfb, cfb);
	cleanup_and_exit(1);
    }
}

void
get_control_points(char *buf, int buf_size) {
  int n;
  char pbuf[MAXBUF];
  FILE *cp;
  
  sprintf(pbuf, "%s --max-time %d --silent http://%s/cgi/get.cgi?nick=%s&url=%s&programs=flame",
	  curl_cmd, timeout, dream_server, nick_buf, url_buf);

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
	  "'http://%s/cgi/put.cgi?frame=%d&id=%d&size=%d&gen=%d'",
	  curl_cmd, timeout, fname, dream_server, frame, anim_id,
	  sbuf.st_size, gen);
  if (debug) printf("about to put %s\n", pbuf);
  mysystem(pbuf, "put image");
}

void
do_render() {
  char anim_text[max_cp_size];
  char *cp_text, *image;
  int frame, this_size, anim_id, type, tile, ntiles;
  char prog[101];
  int ppm_fd, cps_fd, jpg_fd, render_gen;
  double blur;
  FILE *fp;

  get_control_points(anim_text, max_cp_size);

  if (0 == anim_text[0]) {
    /* this happens when the server is busy and getting the url command times out.
       i don't know why the error is detected sooner. */
      if (debug) printf("mysterious empty control point, sleeping.\n");
    sleep(tryagain);
    return;
  }

  if (!strcmp("NA\n", anim_text)) {
      if (debug) printf("nothing to render, sleeping.\n");
      sleep(tryagain);
      return;
  }

  {
    char n[] = "/tmp/electricsheep.cps.XXXXXX";
    cps_fd = mkstemp(n);
    cps_name = n;
  }

  /* scan variables out of header */

  cp_text = anim_text;

  if (1 != sscanf(cp_text, "gen=%d\n", &render_gen)) {
    fprintf(stderr, "could not scan gen {%s}\n", cp_text);
    cleanup_and_exit(1);
  }
  cp_text = strchr(cp_text, '\n') + 1;

  if (1 != sscanf(cp_text, "id=%d\n", &anim_id)) {
    fprintf(stderr, "could not scan id {%s}\n", cp_text);
    cleanup_and_exit(1);
  }
  cp_text = strchr(cp_text, '\n') + 1;

  if (1 != sscanf(cp_text, "type=%d\n", &type)) {
    fprintf(stderr, "could not scan type {%s}\n", cp_text);
    cleanup_and_exit(1);
  }
  cp_text = strchr(cp_text, '\n') + 1;

  if (1 != sscanf(cp_text, "prog=%100s\n", prog)) {
    fprintf(stderr, "could not scan prog {%s}\n", cp_text);
    cleanup_and_exit(1);
  }
  cp_text = strchr(cp_text, '\n') + 1;

  if (1 != sscanf(cp_text, "frame=%d\n", &frame)) {
      fprintf(stderr, "could not scan frame {%s}\n", cp_text);
      cleanup_and_exit(1);
  }
  cp_text = strchr(cp_text, '\n') + 1;

  if (1 == type) {
      if (1 != sscanf(cp_text, "ntiles=%d\n", &ntiles)) {
	  fprintf(stderr, "could not scan ntiles {%s}\n", cp_text);
	  cleanup_and_exit(1);
      }
      cp_text = strchr(cp_text, '\n') + 1;
      if (1 != sscanf(cp_text, "tile=%d\n", &tile)) {
	  fprintf(stderr, "could not scan tile {%s}\n", cp_text);
	  cleanup_and_exit(1);
      }
      cp_text = strchr(cp_text, '\n') + 1;
      if (1 != sscanf(cp_text, "blur=%lf\n", &blur)) {
	  fprintf(stderr, "could not scan blur {%s}\n", cp_text);
	  cleanup_and_exit(1);
      }
      cp_text = strchr(cp_text, '\n') + 1;
  }

  /* write the rest (the control points) to file */

  fp = fopen(cps_name, "w");
  if (NULL == fp) {
      perror(cps_name);
      cleanup_and_exit(1);
  }
  if (1 != fwrite(cp_text, strlen(cp_text), 1, fp)) {
      perror(cps_name);
      cleanup_and_exit(1);
  }
  fclose(fp);

  {
      char n[] = "/tmp/electricsheep.ppm.XXXXXX";
      ppm_fd = mkstemp(n);
      ppm_name = n;
  }

  {
      char b[MAXBUF];
      char strips[MAXBUF];
      if (0 == type) {
	  strips[0] = 0;
      } else {
	  sprintf(strips, "first=%d last=%d nstrips=%d blur=%g", tile, tile, ntiles, blur);
      }
      sprintf(b, "nice -%d env in=%s begin=%d end=%d ofile=%s %s anim-%s",
	      nice_level, cps_name, frame, frame, ppm_name, strips, prog);
      if (debug) printf("about to render %s\n", b);
      mysystem(b, "render");
  }

  {
      char sb[MAXBUF];
      char n[] = "/tmp/electricsheep.jpg.XXXXXX";
      jpg_fd = mkstemp(n);
      jpg_name = n;

      sprintf(sb, "cjpeg -quality 90 %s > %s", ppm_name, jpg_name);
      mysystem(sb, "cjpeg");
      if (0 == type) {
	  put_image(jpg_name, frame, anim_id, render_gen);
      } else if (1 == type) {
	  put_image(jpg_name, tile, anim_id, render_gen);
      }
  }

  close(cps_fd);
  close(ppm_fd);
  close(jpg_fd);

  /* hm should we close after unlink? */

  if (unlink(jpg_name)) {
      perror(jpg_name);
      cleanup_and_exit(1);
  }
  if (unlink(cps_name)) {
      perror(cps_name);
      cleanup_and_exit(1);
  }
  if (unlink(ppm_name)) {
      perror(ppm_name);
      cleanup_and_exit(1);
  }
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
	    fprintf(stderr, "ignored because there is nothing to vote for.\n");
	    continue;
	}

	id = atoi(gb);

	if (vote == 0) {
	    if (debug) printf("not voting zero\n");
	} else {
	    sprintf(pbuf, "%s --max-time %d --silent "
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


/* download anims, delete old anims, update cache */
void
do_download() {
    if (1) {
	char pbuf[MAXBUF];
	char state[MAXBUF];
	FILE *lf;
    
	sprintf(pbuf, "%s --max-time %d --silent http://%s/cgi/list.cgi",
		curl_cmd, timeout, dream_server);

	lf = popen(pbuf, "r");

	if (NULL == lf) {
	    perror("could not fork/pipe\n");
	    cleanup_and_exit(1);
	}

	nserver_anims = 0;

	generation = -1;
	if (NULL != fgets(pbuf, MAXBUF, lf)) {
	    generation = atoi(pbuf);
	    while (NULL != fgets(pbuf, MAXBUF, lf)) {
		anim_t *an = &server_anims[nserver_anims];
		int len = strlen(pbuf);
		int tm;

		if (0 == len) break;
		if (pbuf[len-1] != '\n') break;
	
		sscanf(pbuf, "%d %d %s %d %d %d %d %d %1000s\n",
		       &an->id, &an->type, state, &tm, &an->size, &an->rating,
		       &an->first, &an->last, an->url);
		an->ctime = (time_t) tm;

		if (!strcmp(state, "done") && (0 == an->type)) {
		    nserver_anims++;
		    if (max_anims == nserver_anims) {
			fprintf(stderr, "too many lines from server.\n");
			break;
		    }
		}
	    }
	}
	pclose(lf);
    }

    if (-1 == generation) {
	fprintf(stderr, "lost contact with %s, cannot retrieve sheep.\n", dream_server);
	sleep(tryagain);
	return;
    }

    if (debug) print_anims("nserver_anims", server_anims, nserver_anims);

    update_cached_anims(1);

    if (1) {
	int i, j;
	int best_rating;
	time_t best_ctime = 0;
	int best_anim = -1;
	for (i = 0; i < nserver_anims; i++) {
	    for (j = 0; j < ncached_anims; j++) {
		if (server_anims[i].id == cached_anims[j].id)
		    break;
	    }
	    if (j == ncached_anims &&
		server_anims[i].size < (1024 * 1024 * leave_max_megabytes)) {
		/* anim on the server fits in cache but is not in cache */
		if (best_ctime == 0 ||
		    (server_anims[i].rating > best_rating) ||
		    (server_anims[i].rating == best_rating &&
		     server_anims[i].ctime > best_ctime)) {
		    best_rating = server_anims[i].rating;
		    best_ctime = server_anims[i].ctime;
		    best_anim = i;
		}
	    }
	}
	if (debug) printf("best_anim=%d best_rating=%d best_ctime=%d\n",
	       best_anim, best_rating, (int)best_ctime);
	if (-1 != best_anim) {
	    delete_cached(server_anims[best_anim].size);
	    download_anim(best_anim);
	} else {
	    if (debug) printf("nothing to download, sleeping.\n");
	    sleep(tryagain);
	}
    }
}

void
make_download_process() {
    
    if (-1 == (downloader_pid = fork()))
	perror("downloader fork");
    else if (0 == downloader_pid) {
	/* child */
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
    thread_name = "render";
    while (1) {
	do_render();
    }
}


void
make_display_process() {

    int h;
    for (h = 0; h < history_size; h++)
	path_history[h] = -1;
    
    if (-1 == (displayer_pid = fork()))
	perror("displayer fork");
    else if (0 == displayer_pid) {
	/* child */
	thread_name = "display";
	while (1) {
	    do_display();
	}
    }
    /* parent returns */
}


void
logo_only() {
  char pbuf[MAXBUF];
  sprintf(pbuf, "xsetbg -at 0,0 -windowid %s %selectricsheep-splash-1.tif",
	  window_id, splash_prefix);
  mysystem2(pbuf, "logo");
  while (1) sleep(60);
}

int
filename_is_mpg(char *name) {
    int n = strlen(name);
    return !(n <= 4 || 0 != strcmp(&name[n-4], ".mpg"));
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

void
flags_init(int *argc, char ***argv) {
  char *arg0 = (*argv)[0];
  while (*argc > 1 &&
	 (*argv)[1][0] == '-') {
    char *o = (*argv)[1];
    if (!strcmp("--help", o)) {
      printf("electricsheep v%s - the collective dream of sleeping computers from all over the internet.\n"
	     "                     http://electricsheep.org\n"
	     "\n"
	     "usage: electricsheep [options]\n"
	     "\n"
	     "--nick name (vanity)\n"
	     "--url name (vanity)\n"
	     "--nthreads N (number of rendering threads, default 1)\n"
	     "--frame-rate N (frames/second)\n"
	     "--timeout N (seconds)\n"
	     "--tryagain N (seconds between retries to server)\n"
	     "--server host/path (a hostname, possibly with a path)\n"
	     "--display-anim 0/1 (invisibility if 0, default 1)\n"
	     "--standalone 0/1 (disables render & download, default 0)\n"
	     "--renderer exec_name\n"
	     "--save-dir path (directory in which to save anims)\n"
	     "--reset-fuse N (maximum number of transitions before resetting\n"
	     "\tto avoid loops, -1 means never)\n"
	     "--max-megabytes N (maximum disk used to save anims)\n"
	     "--splash-prefix /some/path/ (finds image files)\n"
	     "--nice n (priority adjustment for render process, default 10)\n"
	     "--nrepeats n (number of times to repeat loopable animations, default 2)\n"
	     "--proxy url (connect to server through proxy (see curl(1)))\n"
	     "--player exec_name (eg mpeg_play)\n", VERSION);
      exit(0);
    }
    else iarg("--frame-rate", frame_rate)
    else iarg("--timeout", timeout)
    else iarg("--tryagain", tryagain)
    else sarg("--server", dream_server)
    else sarg("--nick", nick_name)
    else sarg("--url", url_name)
    else iarg("--nice", nice_level)
    else sarg("--splash-prefix", splash_prefix)
    else sarg("--save-dir", leave_prefix)
    else iarg("--max-megabytes", leave_max_megabytes)
    else iarg("--reset-fuse", reset_fuse_length)
    else iarg("--display-anim", display_anim)
    else iarg("--parasite", parasite)
    else iarg("--standalone", standalone)
    else iarg("--nrepeats", nrepeats)
    else iarg("--nthreads", nthreads)
    else sarg("--player", play_prog)
    else sarg("--proxy", proxy_name)
    else {
      fprintf(stderr, "bad option: %s, try --help\n", o);
      exit(1);
    }
  }
  (*argv)[0] = arg0;

  if (nice_level < -20) {
      fprintf(stderr, "nice level must be -20 or greater, not %d.\n", nice_level);
      nice_level = -20;
  }
  if (nice_level > 19) {
      fprintf(stderr, "nice level must be 19 or less, not %d.\n", nice_level);
      nice_level = 19;
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

  return;
 fail:
  fprintf(stderr, "no argument to %s\n", (*argv)[1]);
  exit(1);
}


int
main(int argc, char **argv) {
    int n = 0;

    thread_name = "main";

    /* create our own group so all workers/children may be killed together
       without hassle */
    if (-1 == setpgrp())
	perror("setpgrp");
    signal(SIGTERM, handle_sig_term);
    signal(SIGINT, handle_sig_term);

    if (1) {
	int i;
	for (i = 0; i < max_anims; i++)
	    nplays_by_id[i] = 0;
    }
	
    flags_init(&argc, &argv);

    reset_fuse = reset_fuse_length;

    if (proxy_name)
	sprintf(curl_cmd, "nice -%d curl --proxy %s", nice_level, proxy_name);
    else
	sprintf(curl_cmd, "nice -%d curl", nice_level);

    srandom(time(0));

    if (window_id) logo_only();

    not_playing();
    default_background(0);
    
    // forks
    if (display_anim) {
	make_ui_process();
	make_display_process();
    }
    if (!standalone) make_download_process();

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
}
