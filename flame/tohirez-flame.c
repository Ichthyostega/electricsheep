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
 *        sad creature
 */

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


#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))

int main() {
    frame_spec f;
    char *ai;
    control_point tmpl_cp, spex_cp;
    char s[4000];
    char *ss;
    int c, i, count = 0;
    unsigned char *image;
    FILE *fp;
    char name[100];
    int this_size, last_size = -1;
    int strip;
    double center_y, center_base;
    int total_frames = argi("frames", 100);

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
    tmpl_cp.hue_rotation = 0.0;
    tmpl_cp.contrast = 1.0;
    tmpl_cp.brightness = 1.0;
    tmpl_cp.spatial_filter_radius = 0.5;
    tmpl_cp.sample_density = 50;
    tmpl_cp.zoom = 0.0;
    tmpl_cp.nbatches = 10;
    tmpl_cp.white_level = 200;

    spex_cp.background[0] = 0.0;
    spex_cp.background[1] = 0.0;
    spex_cp.background[2] = 0.0;
    spex_cp.center[0] = 0.0;
    spex_cp.center[1] = 0.0;
    spex_cp.pixels_per_unit = 50;
    spex_cp.width = 100;
    spex_cp.height = 100;
    spex_cp.spatial_oversample = 1;
    spex_cp.gamma = 4.0;
    spex_cp.vibrancy = 1.0;
    spex_cp.hue_rotation = 0.0;
    spex_cp.contrast = 1.0;
    spex_cp.brightness = 1.0;
    spex_cp.spatial_filter_radius = 0.5;
    spex_cp.sample_density = 50;
    spex_cp.zoom = 0.0;
    spex_cp.nbatches = 10;
    spex_cp.white_level = 200;

    if (1) {
	FILE *template = fopen(getenv("template"), "r");
	i = 0;
	ss = s;
	do {
	    c = getc(template);
	    if (EOF == c)
		goto done_reading0;
	    s[i++] = c;
	} while (';' != c);
      
    done_reading0:
	fclose(template);
	parse_control_point(&ss, &tmpl_cp);
    }

    if (1) {
	FILE *template = fopen(getenv("spex"), "r");
	i = 0;
	ss = s;
	do {
	    c = getc(template);
	    if (EOF == c)
		goto done_reading1;
	    s[i++] = c;
	} while (';' != c);
      
    done_reading1:
	fclose(template);
	parse_control_point(&ss, &spex_cp);
    }
    spex_cp.width = tmpl_cp.width;
    spex_cp.height = tmpl_cp.height;
    spex_cp.pixels_per_unit = tmpl_cp.pixels_per_unit;
    spex_cp.spatial_filter_radius = tmpl_cp.spatial_filter_radius;
    spex_cp.sample_density = tmpl_cp.sample_density;
    spex_cp.nbatches = tmpl_cp.nbatches;
    spex_cp.spatial_oversample = tmpl_cp.spatial_oversample;

   for (i = 0; i < 5; i++) {
     spex_cp.time = i * total_frames / 4;
     print_control_point(stdout, &spex_cp, 0);
     rotcp(&spex_cp, 90);
   }
   return 0;
}
