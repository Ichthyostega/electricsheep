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


static char *anim_flame_c_id =
"@(#) $Id: anim-flame.c,v 1.16 2004/03/04 06:35:17 spotspot Exp $";

#include "rect.h"
#include "jpeg.h"
#include <string.h>

/*
  usage is:
  env prefix=default begin=0 end=0 dtime=1 fields=0
      in=filename out=filename
      qs=v ss=v <exe>
      [ < in ]

*/

#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define argf(s,d)   ((ai = getenv(s)) ? atof(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))


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
  int time, channels;
  unsigned char *image;
  control_point *cps;
  int i, ncps = 0;
  char *inf = getenv("in");
  double pixel_aspect = argf("pixel_aspect", 1.0);
  FILE *in;
  frame_spec f;

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

  cps = parse_control_points_from_file(in, &ncps);
  if (NULL == cps) {
    exit(1);
  }
  if (0 == ncps) {
    fprintf(stderr, "error: no control points.\n");
    exit(1);
  }
  for (i = 0; i < ncps; i++) {
    cps[i].sample_density *= qs;
    cps[i].height = (int)(cps[i].height * ss);
    cps[i].width = (int)(cps[i].width * ss);
    cps[i].pixels_per_unit *= ss;
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
  f.cps = cps;
  f.ncps = ncps;
  image = (unsigned char *) malloc(channels * cps[0].width * cps[0].height);
   
  for (time = first_frame; time <= last_frame; time += dtime) {
    f.time = (double) time;
    if (do_fields) {
	render_rectangle(&f, image, cps[0].width, field_even, channels, 0);
	f.time += 0.5;
	render_rectangle(&f, image, cps[0].width, field_odd, channels, 0);
    } else {
	render_rectangle(&f, image, cps[0].width, field_both, channels, 0);
    }
    if (1) {
	FILE *fp;
	if (getenv("out"))
	    fname = getenv("out");
	else
	    sprintf(fname, "%s%04d.%s", prefix, time, format);
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
