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

/*
  usage is
  env prefix=default begin=0 end=100 nstrips=1 dtime=1 fields=0
      template=filename in=filename cmd=postprocessor <exe>

*/

#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define argf(s,d)   ((ai = getenv(s)) ? atof(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))



int
main(argc, argv)
   int argc;
   char **argv;
{
   char *s, *ai, fname[50];
   char *prefix = args("prefix", "");
   int first_frame = argi("begin", 0);
   int last_frame = argi("end", 100);
   int nstrips = argi("nstrips", 1);
   int first_strip = argi("first", 0);
   int last_strip = argi("last", 0);
   int dtime = argi("dtime", 1);
   int do_fields = argi("fields", 0);
   int time_base = argi("time_base", 0);
   int time;
   unsigned char *image;
   control_point templ;
   control_point *cps = (control_point *) malloc(sizeof(control_point) * 300);
   char *ss;
   int c, i, ncps = 0;
   frame_spec f;
   s = malloc(1000 * 1000);

   /* init the template */
   templ.background[0] = 0.0;
   templ.background[1] = 0.0;
   templ.background[2] = 0.0;
   templ.center[0] = 0.0;
   templ.center[1] = 0.0;
   templ.pixels_per_unit = 50;
   templ.width = 100;
   templ.height = 100;
   templ.spatial_oversample = 1;
   templ.gamma = 2.0;
   templ.vibrancy = 1.0;
   templ.contrast = 1.0;
   templ.brightness = 1.0;
   templ.spatial_filter_radius = 0.5;
   templ.zoom = 0.0;
   templ.sample_density = 50;
   templ.nbatches = 10;
   templ.white_level = 200;
   templ.cmap_inter = 0;

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
      parse_control_point(&ss, &templ);
   }

   /* read in all the cps from in */
   if (1) {
      FILE *in = fopen(getenv("in"), "r");
      if (NULL == in) {
	 fprintf(stderr, "cannot read %s\n", getenv("in"));
	 exit(1);
      }
      while (1) {
	 i = 0;
	 ss = s;
	 do {
	    c = getc(in);
	    if (EOF == c)
	       goto done_reading;
	    s[i++] = c;
	 } while (';' != c);
	 cps[ncps] = templ;
	 get_cmap(cmap_random, cps[ncps].cmap, CMAP_SIZE, 0.0);
	 parse_control_point(&ss, cps + ncps);
	 ncps++;
      }
   }
 done_reading:
   
   f.temporal_filter_radius = argf("blur", 0.5);
   f.cps = cps;
   f.ncps = ncps;
   image = (unsigned char *) malloc(3 * cps[0].width * cps[0].height / nstrips);
   
   for (i = 0; i < ncps; i++) {
       cps[i].sample_density *= nstrips;
       cps[i].height /= nstrips;
   }

   for (time = first_frame; time <= last_frame; time += dtime) {
      int strip;
      control_point cp;
      double height = cps[0].height / (double) cps[0].pixels_per_unit;
      double center_base = cps[0].center[1] - ((nstrips-1)/2.0)*height;
      for (strip = first_strip; strip <= last_strip; strip++) {
	  //image + cps[0].height * strip * cps[0].width * 3;
	  unsigned char *strip_start = image;
	  if (getenv("ofile"))
	      strcpy(fname, getenv("ofile"));
	  else if (1 == nstrips) {
	      sprintf(fname, "%s%04d.ppm", prefix, time);
	  } else {
	      sprintf(fname, "%s%04d-%04d.ppm", prefix, time, strip);
	  }
	 cps[0].center[1] = center_base + height * (double) strip;

	 f.time = (double) time;
	 if (do_fields) {
	    render_rectangle(&f, strip_start, cps[0].width, field_even, 3, 0);
	    f.time += 0.5;
	    render_rectangle(&f, strip_start, cps[0].width, field_odd, 3, 0);
	 } else {
	    render_rectangle(&f, strip_start, cps[0].width, field_both, 3, 0);
	 }
	 if (1) {
	     FILE *fp;
	     fp = fopen(fname, "w");
	     fprintf(fp, "P6\n");
	     fprintf(fp, "%d %d\n255\n", cps[0].width, cps[0].height);
	     fwrite(image, 1, 3 * cps[0].width * cps[0].height, fp);
	     fclose(fp);
	 }
	 if (getenv("cmd")) {
	     char buf[1000];
	     sprintf(buf, "%s %s %d", getenv("cmd"), fname, time + time_base);
	     system(buf);
	 }
	 
      }
   }
   return 0;
}
