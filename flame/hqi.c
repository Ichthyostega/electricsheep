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

/* read control points from stdin,

 * defaults: render quick 100x100 pencil tests. 
 * camera is biunit square.
 */

#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))

int main() {
   frame_spec f;
   char *ai;
   control_point cps, tmpl_cp;
   char s[4000];
   char *ss;
   int c, i, count = 0;
   unsigned char *image;
   FILE *fp;
   char name[100];
   int this_size, last_size = -1;
   int strip;
   double center_y, center_base;
   int nstrips = argi("nstrips", 1);

   srandom(time(0));

   tmpl_cp.background[0] = 0.0;
   tmpl_cp.background[1] = 0.0;
   tmpl_cp.background[2] = 0.0;
   tmpl_cp.center[0] = 0.0;
   tmpl_cp.center[1] = 0.0;
   tmpl_cp.pixels_per_unit = 50;
   tmpl_cp.width = 100;
   tmpl_cp.height = 100;
   tmpl_cp.spatial_oversample = 1;
   tmpl_cp.gamma = 1.0;
   tmpl_cp.vibrancy = 1.0;
   tmpl_cp.contrast = 1.0;
   tmpl_cp.brightness = 1.0;
   tmpl_cp.spatial_filter_radius = 0.5;
   tmpl_cp.sample_density = 50;
   tmpl_cp.zoom = 0.0;
   tmpl_cp.nbatches = 10;
   tmpl_cp.white_level = 200;
   
   if (getenv("template")) {
      FILE *template = fopen(getenv("template"), "r");
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
      parse_control_point(&ss, &tmpl_cp);
   }

   while (1) {
      i = 0;
      ss = s;
      do {
	 c = getchar();
	 if (EOF == c)
	    exit(0);
	 s[i++] = c;
      } while (';' != c);

      cps = tmpl_cp;
      cps.cmap_index = get_cmap(cmap_random, cps.cmap, CMAP_SIZE, 0.0);
      parse_control_point(&ss, &cps);

      f.temporal_filter_radius = 0.0;
      f.cps = &cps;
      f.ncps = 1;
      f.time = 0.0;

      this_size = 3 * cps.width * cps.height;
      if (this_size != last_size) {
	 if (last_size != -1)
	    free(image);
	 last_size = this_size;
	 image = (unsigned char *) malloc(this_size);
      }

      cps.sample_density *= nstrips;
      cps.height /= nstrips;
      center_y = cps.center[1];
      center_base = center_y - ((nstrips - 1) * cps.height) /
	(2 * cps.pixels_per_unit);

      for (strip = 0; strip < nstrips; strip++) {
	 unsigned char *strip_start =
	    image + cps.height * strip * cps.width * 3;
	 cps.center[1] = center_base +
	   cps.height * (double) strip / cps.pixels_per_unit;

	 fprintf(stderr, "hqi: working on frame %d, strip %d\n", count, strip);
	 render_rectangle(&f, strip_start, cps.width, field_both, 3, 0);
      }
      
      sprintf(name, "%s.%d.ppm", args("out", "hqi"), count);
      fp = fopen(name, "w");
      fprintf(fp, "P6\n");
      /* restore the cps values to their original values as read */
      cps.sample_density /= nstrips;
      cps.height *= nstrips;
      cps.center[1] = center_y;
      /* and write it out */
      // print_control_point(fp, &cps, 1);
      fprintf(fp, "%d %d\n255\n", cps.width, cps.height);
      fwrite(image, 1, this_size, fp);
      fclose(fp);
      count++;
   }
}
