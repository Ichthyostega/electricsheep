/*
    flame - cosmic recursive fractal flames
    Copyright (C) 1992  Scott Draves <spot@cs.cmu.edu>

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


#include "rect.h"


#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))
#define argd(s,d)   ((ai = getenv(s)) ? atof(ai) : (d))



rotcp(control_point *cp, double by)
{
   double s, t;
   int i;
   for (i = 0; i < NXFORMS; i++) {
      double r[2][2];
      double T[2][2];
      double U[2][2];
      double dtheta = by * 2.0 * M_PI / 360.0;

      r[1][1] = r[0][0] = cos(dtheta);
      r[0][1] = sin(dtheta);
      r[1][0] = -r[0][1];
      T[0][0] = cp->xform[i].c[0][0];
      T[1][0] = cp->xform[i].c[1][0];
      T[0][1] = cp->xform[i].c[0][1];
      T[1][1] = cp->xform[i].c[1][1];
      mult_matrix(r, T, U);
      cp->xform[i].c[0][0] = U[0][0];
      cp->xform[i].c[1][0] = U[1][0];
      cp->xform[i].c[0][1] = U[0][1];
      cp->xform[i].c[1][1] = U[1][1];
   }
}

int
main(argc, argv)
   int argc;
   char **argv;
{
   int count = 0;
   char s[4000], *ai, fname[50];
   unsigned char *image;
   control_point templ;
   control_point cp_orig;
   control_point cp_rotated;
   char *ss;
   int c, i, nstrips = 1;
   double avg_pix, fraction_black, fraction_white;
   double avg_thresh = argd("avg", 20.0);
   double black_thresh = argd("black", 0.01);
   double white_limit = argd("white", 0.05);
   int total_frames = argi("frames", 100);

   frame_spec f;

   srandom(time(0));

   strcpy(fname, "/tmp/pick.ppm");

   /* init the template */
   templ.background[0] = 0.0;
   templ.background[1] = 0.0;
   templ.background[2] = 0.0;
   templ.center[0] = 0.0;
   templ.center[1] = 0.0;
   templ.pixels_per_unit = 64;
   templ.width = 128;
   templ.height = 128;
   templ.spatial_oversample = 1;
   templ.gamma = 2.0;
   templ.vibrancy = 1.0;
   templ.hue_rotation = 0.0;
   templ.contrast = 1.0;
   templ.brightness = 1.0;
   templ.spatial_filter_radius = 0.5;
   templ.zoom = 0.0;
   templ.sample_density = 1;
   templ.nbatches = 10;
   templ.white_level = 200;
   templ.cmap_inter = 0;

   cp_orig = templ;

   f.temporal_filter_radius = 0.0;
   f.cps = &cp_orig;
   f.ncps = 1;
   image = (unsigned char *) malloc(3 * cp_orig.width * cp_orig.height);

   /* pick random control point until one looks good enough */

   do {
      int strip = 0;
      control_point cp;
      double height = cp_orig.height / (double) cp_orig.pixels_per_unit;
      double center_base = cp_orig.center[1] - ((nstrips-1)/2.0)*height/nstrips;

      unsigned char *strip_start =
	image + cp_orig.height * strip * cp_orig.width * 3;
      cp_orig.center[1] = center_base + height * (double) strip / nstrips;

      f.time = (double) 0.0;

      random_control_point(&cp_orig, -1);

      render_rectangle(&f, strip_start, cp_orig.width, field_both, 3, 0);

      if (1) {
	 FILE *fp;
	 fp = fopen(fname, "w");
	 if (!fp) {
	   perror("image out");
	   exit(1);
	 }
	 fprintf(fp, "P6\n");
	 fprintf(fp, "%d %d\n255\n", cp_orig.width, cp_orig.height);
	 fwrite(image, 1, 3 * cp_orig.width * cp_orig.height, fp);
	 fclose(fp);
      }

      if (1) {
	int i, n, tot, totb, totw;
	n = 3 * cp_orig.width * cp_orig.height;
	tot = 0;
	totb = 0;
	totw = 0;
	for (i = 0; i < n; i++) {
	  tot += image[i];
	  if (0 == image[i]) totb++;
	  if (255 == image[i]) totw++;

	  // printf("%d ", image[i]);
	}

	avg_pix = (tot / (double)n);
	fraction_black = totb / (double)n;
	fraction_white = totw / (double)n;

	if (0)
	  printf("avg pix = %g %g %g %g\n", avg_pix,
		 fraction_black, fraction_white, (double)n);
      } else {
	avg_pix = avg_thresh + 1.0;
	fraction_black = black_thresh + 1.0;
	fraction_white = white_limit - 1.0;
      }

   } while ((avg_pix < avg_thresh ||
	     fraction_black < black_thresh ||
	     fraction_white > white_limit) &&
	    count++ < 50);



   if (getenv("template")) {
      FILE *template = fopen(getenv("template"), "r");

      if (0 == template) {
	fprintf(stderr, "failed to open %s\n", getenv("template"));
	exit(1);
      }

      i = 0;
      ss = s;
      do {
	 c = getc(template);
	 if (EOF == c)
	    goto done_reading_template;
	 s[i++] = c;
      } while (';' != c);
	 
    done_reading_template:
      fclose(template);
      parse_control_point(&ss, &templ);
   }

   /* ugh */
   cp_orig.gamma = templ.gamma;
   cp_orig.vibrancy = templ.vibrancy;
   cp_orig.brightness = templ.brightness;
   cp_orig.background[0] = templ.background[0];
   cp_orig.background[1] = templ.background[1];
   cp_orig.background[2] = templ.background[2];
   cp_orig.spatial_oversample = templ.spatial_oversample;
   cp_orig.spatial_filter_radius = templ.spatial_filter_radius;
   cp_orig.sample_density = templ.sample_density;
   cp_orig.width = templ.width;
   cp_orig.height = templ.height;
   cp_orig.pixels_per_unit = templ.pixels_per_unit;

   if ((cp_orig.width & 15) ||
       (cp_orig.height & 15)) {
     fprintf(stderr, "pick-flame warning: width and height should be 0 mod 16, %dx%d is not.\n", cp_orig.width, cp_orig.height);
   }

   for (i = 0; i < 5; i++) {
     cp_orig.time = i * total_frames / 4;
     print_control_point(stdout, &cp_orig, 0);
     rotcp(&cp_orig, 90);
   }

   exit(0);
}
