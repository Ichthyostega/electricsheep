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

static char *hqi_flame_c_id =
"@(#) $Id: hqi-flame.c,v 1.9 2004/03/04 06:35:17 spotspot Exp $";

#ifdef WIN32
#define WINVER 0x0500
#include <windows.h>
#endif

#include "rect.h"
#include "jpeg.h"
#include <string.h>

/*
  usage is:
  env prefix=default nstrips=1 in=filename out=filename <exe> [ < in ]
*/

#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define argf(s,d)   ((ai = getenv(s)) ? atof(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))

int calc_nstrips(control_point *cps) {
  double mem_required;
  double mem_available;
  int nstrips;
#ifdef WIN32
  MEMORYSTATUS stat;
  stat.dwLength = sizeof(stat);
  GlobalMemoryStatus(&stat);
  mem_available = (double)stat.dwTotalPhys * 0.8;
#else
  mem_available = 100000000;
#endif
  mem_required =
    (double) cps[0].spatial_oversample * cps[0].spatial_oversample *
    (double) cps[0].width * cps[0].height * 16;
  if (mem_available >= mem_required) return 1;
  nstrips = 1 + (int)(mem_required / mem_available);
  while (cps[0].height % nstrips) {
    nstrips++;
  }
  return nstrips;
}

int main() {
   frame_spec f;
   char *ai;
   control_point *cps;
   int ncps;
   int i;
   unsigned char *image;
   FILE *fp;
   char *fname;
   int this_size, last_size = -1;
   int strip;
   double center_y, center_base;
   int nstrips;
   char *prefix = args("prefix", "");
   char *out = args("out", NULL);
   char *format = getenv("format");
   char *inf = getenv("in");
   double qs = argf("qs", 1.0);
   double ss = argf("ss", 1.0);
   double pixel_aspect = argf("pixel_aspect", 1.0);
   FILE *in;
   double zoom_scale;
   int channels;

   fname = malloc(strlen(prefix) + 20);

   if (NULL == format) format = "jpg";
   if (strcmp(format, "jpg") &&
       strcmp(format, "ppm") &&
       strcmp(format, "png")) {
       fprintf(stderr,
	       "format must be either jpg, ppm, or png, not %s.\n",
	       format);
       exit(1);
   }

   channels = strcmp(format, "png") ? 3 : 4;
   
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
   for (i = 0; i < ncps; i++) {
     cps[i].sample_density *= qs;
     cps[i].height = (int)(cps[i].height * ss);
     cps[i].width = (int)(cps[i].width * ss);
     cps[i].pixels_per_unit *= ss;
   }

   if (out && (ncps > 1)) {
     fprintf(stderr, "hqi-flame: warning: writing multiple images "
	     "to one file.  all but last will be lost.\n");
   }

   if (getenv("nstrips")) {
     nstrips = atoi(getenv("nstrips"));
   } else {
     nstrips = calc_nstrips(cps);
   }

   for (i = 0; i < ncps; i++) {

      f.temporal_filter_radius = 0.0;
      f.cps = &cps[i];
      f.ncps = 1;
      f.time = 0.0;
      f.pixel_aspect_ratio = pixel_aspect;

      this_size = channels * cps[i].width * cps[i].height;
      if (this_size != last_size) {
	 if (last_size != -1)
	    free(image);
	 last_size = this_size;
	 image = (unsigned char *) malloc(this_size);
      }

      cps[i].sample_density *= nstrips;
      cps[i].height /= nstrips;
      center_y = cps[i].center[1];
      zoom_scale = pow(2.0, cps[i].zoom);
      center_base = center_y - ((nstrips - 1) * cps[i].height) /
	(2 * cps[i].pixels_per_unit * zoom_scale);

      for (strip = 0; strip < nstrips; strip++) {
	 unsigned char *strip_start =
	    image + cps[i].height * strip * cps[i].width * channels;
	 cps[i].center[1] = center_base +
	   cps[i].height * (double) strip /
	   (cps[i].pixels_per_unit * zoom_scale);

	 render_rectangle(&f, strip_start, cps[i].width,
			  field_both, channels, 0);
      }

      if (NULL != out) {
	fname = out;
      } else {
	sprintf(fname, "%s%04d.%s", prefix, i, format);
      }
      fp = fopen(fname, "wb");
      if (NULL == fp) {
	perror(fname);
	exit(1);
      }
      /* restore the cps values to their original values */
      cps[i].sample_density /= nstrips;
      cps[i].height *= nstrips;
      cps[i].center[1] = center_y;
      /* and write it out */
      if (!strcmp(format, "png")) {
	  write_png(fp, image, cps[0].width, cps[0].height);
      } else if (!strcmp(format, "jpg")) {
	  write_jpeg(fp, image, cps[0].width, cps[0].height);
      } else {
	  fprintf(fp, "P6\n");
	  fprintf(fp, "%d %d\n255\n", cps[i].width, cps[i].height);
	  fwrite(image, 1, this_size, fp);
      }
      fclose(fp);
   }
}
