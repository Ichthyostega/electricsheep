/*
    flame - cosmic recursive fractal flames
    Copyright (C) 1992-2003  Scott Draves <source@flam3.com>

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


static char *flam3_animate_c_id =
"@(#) $Id: flam3-animate.c,v 1.8 2005/07/20 06:06:13 spotspot Exp $";

#include "private.h"
#include "img.h"

int main(int argc, char **argv) {
  char *ai, *fname;
  char *prefix = args("prefix", "");
  int first_frame = argi("begin", 0);
  int last_frame = argi("end", 0);
  int frame_time = argi("time", 0);
  int dtime = argi("dtime", 1);
  int do_fields = argi("fields", 0);
  double qs = argf("qs", 1.0);
  double ss = argf("ss", 1.0);
  char *format = getenv("format");
  int verbose = argi("verbose", 0);
  int bits = argi("bits", 64);
  int seed = argi("seed", 0);
  int ftime, channels;
  unsigned char *image;
  flam3_genome *cps;
  int i, ncps = 0;
  char *inf = getenv("in");
  double pixel_aspect = argf("pixel_aspect", 1.0);
  FILE *in;
  flam3_frame f;

  if (1 != argc) {
      puts(docstring);
      exit(0);
  }

  srandom(seed ? seed : (time(0) + getpid()));

  if (getenv("frame")) {
    if (getenv("time")) {
      fprintf(stderr, "cannot specify both time and frame.\n");
      exit(1);
    }
    if (getenv("begin") || getenv("end")) {
      fprintf(stderr, "cannot specify both time and begin or end.\n");
      exit(1);
    }
    first_frame = last_frame = atoi(getenv("frame"));
  }

  if (getenv("time")) {
    if (getenv("begin") || getenv("end")) {
      fprintf(stderr, "cannot specify both time and begin or end.\n");
      exit(1);
    }
    first_frame = last_frame = frame_time;
  }

  if (NULL == format) format = "jpg";
  if (strcmp(format, "jpg") &&
      strcmp(format, "ppm") &&
      strcmp(format, "png")) {
      fprintf(stderr, "format must be either jpg, ppm, or png, not %s.\n", format);
      exit(1);
  }

  fname = malloc(strlen(prefix) + 20);

   if (pixel_aspect <= 0.0) {
     fprintf(stderr, "pixel aspect ratio must be positive, not %g.\n",
	     pixel_aspect);
     exit(1);
   }

  if (inf)
    in = fopen(inf, "rb");
  else
    in = stdin;
  if (NULL == in) {
    perror(inf);
    exit(1);
  }

  cps = flam3_parse_from_file(in, &ncps);
  if (NULL == cps) {
    exit(1);
  }
  if (0 == ncps) {
    fprintf(stderr, "error: no genomes.\n");
    exit(1);
  }
  for (i = 0; i < ncps; i++) {
    cps[i].sample_density *= qs;
    cps[i].height = (int)(cps[i].height * ss);
    cps[i].width = (int)(cps[i].width * ss);
    cps[i].pixels_per_unit *= ss;
    if ((cps[i].width != cps[0].width) ||
	(cps[i].height != cps[0].height)) {
      fprintf(stderr, "warning: flame %d at time %g size mismatch.  "
	      "(%d,%d) should be (%d,%d).\n",
	      i, cps[i].time,
	      cps[i].width, cps[i].height,
	      cps[0].width, cps[0].height);
      cps[i].width = cps[0].width;
      cps[i].height = cps[0].height;
    }
  }
  if (!getenv("time") && !getenv("frame")) {
    if (!getenv("begin")) {
      first_frame = (int) cps[0].time;
    }
    if (!getenv("end")) {
      last_frame = (int) cps[ncps-1].time - 1;
      if (last_frame < first_frame) last_frame = first_frame;
    }
  }
  channels = strcmp(format, "png") ? 3 : 4;
  f.temporal_filter_radius = argf("blur", 0.5);
  f.pixel_aspect_ratio = pixel_aspect;
  f.genomes = cps;
  f.ngenomes = ncps;
  f.verbose = verbose;
  f.bits = bits;
  f.progress = 0;
  image = (unsigned char *) malloc(channels * cps[0].width * cps[0].height);

  if (dtime < 1) {
    fprintf(stderr, "dtime must be positive, not %d.\n", dtime);
    exit(1);
  }
   
  for (ftime = first_frame; ftime <= last_frame; ftime += dtime) {
    f.time = (double) ftime;

    if (verbose && ((last_frame-first_frame)/dtime) > 1) {
       fprintf(stderr, "time = %d/%d/%d\n", ftime, last_frame, dtime);
    }

    if (do_fields) {
	flam3_render(&f, image, cps[0].width, flam3_field_even, channels);
	f.time += 0.5;
	flam3_render(&f, image, cps[0].width, flam3_field_odd, channels);
    } else {
	flam3_render(&f, image, cps[0].width, flam3_field_both, channels);
    }
    if (1) {
	FILE *fp;
	if (getenv("out"))
	    fname = getenv("out");
	else
	    sprintf(fname, "%s%04d.%s", prefix, ftime, format);
	fp = fopen(fname, "wb");
	if (NULL == fp) {
	    perror(fname);
	    exit(1);
	}
	if (!strcmp(format, "png")) {
	   write_png(fp, image, cps[0].width, cps[0].height);
	} else if (!strcmp(format, "jpg")) {
	    write_jpeg(fp, image, cps[0].width, cps[0].height);
	} else {
	    fprintf(fp, "P6\n");
	    fprintf(fp, "%d %d\n255\n", cps[0].width, cps[0].height);
	    fwrite(image, 1, 3 * cps[0].width * cps[0].height, fp);
	}
	fclose(fp);
    }
  }
  return 0;
}
