/*
    flame - cosmic recursive fractal flames
    Copyright (C) 2003  Scott Draves <source@flam3.com>

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

static char *libifs_c_id =
"@(#) $Id: convert-flame.c,v 1.7 2004/03/04 06:35:17 spotspot Exp $";

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "libifs.h"


#define MAXARGS 1000
#define streql(x,y) (!strcmp(x,y))


/*
 * split a string passed in ss into tokens on whitespace.
 * # comments to end of line.  ; terminates the record
 */
void tokenize(ss, argv, argc)
   char **ss;
   char *argv[];
   int *argc;
{
   char *s = *ss;
   int i = 0, state = 0;

   while (*s != ';') {
      char c = *s;
      switch (state) {
       case 0:
	 if ('#' == c)
	    state = 2;
	 else if (!isspace(c)) {
	    argv[i] = s;
	    i++;
	    state = 1;
	 }
       case 1:
	 if (isspace(c)) {
	    *s = 0;
	    state = 0;
	 }
       case 2:
	 if ('\n' == c)
	    state = 0;
      }
      s++;
   }
   *s = 0;
   *ss = s+1;
   *argc = i;
}

/*
 * given a pointer to a string SS, fill fields of a control point CP.
 * return a pointer to the first unused char in SS.  totally barfucious,
 * must integrate with tcl soon...
 */

void parse_control_point_old(ss, cp) 
   char **ss;
   control_point *cp;
{
   char *argv[MAXARGS];
   int argc, i, j;
   int set_cm = 0, set_image_size = 0, set_nbatches = 0, set_white_level = 0, set_cmap_inter = 0;
   int set_spatial_oversample = 0, set_hr = 0;
   double *slot, xf, cm, t, nbatches, white_level, spatial_oversample, cmap_inter;
   double image_size[2];

   memset(cp, 0, sizeof(control_point));

   for (i = 0; i < NXFORMS; i++) {
      cp->xform[i].density = 0.0;
      cp->xform[i].color = (i == 0);
      cp->xform[i].symmetry = 0;
      cp->xform[i].var[0] = 1.0;
      for (j = 1; j < NVARS; j++)
	 cp->xform[i].var[j] = 0.0;
      cp->xform[i].c[0][0] = 1.0;
      cp->xform[i].c[0][1] = 0.0;
      cp->xform[i].c[1][0] = 0.0;
      cp->xform[i].c[1][1] = 1.0;
      cp->xform[i].c[2][0] = 0.0;
      cp->xform[i].c[2][1] = 0.0;
   }

   tokenize(ss, argv, &argc);
   for (i = 0; i < argc; i++) {
      if (streql("xform", argv[i]))
	 slot = &xf;
      else if (streql("time", argv[i]))
	 slot = &cp->time;
      else if (streql("brightness", argv[i]))
	 slot = &cp->brightness;
      else if (streql("contrast", argv[i]))
	 slot = &cp->contrast;
      else if (streql("gamma", argv[i]))
	 slot = &cp->gamma;
      else if (streql("vibrancy", argv[i]))
	 slot = &cp->vibrancy;
      else if (streql("hue_rotation", argv[i])) {
	 slot = &cp->hue_rotation;
	 set_hr = 1;
      } else if (streql("zoom", argv[i]))
	 slot = &cp->zoom;
      else if (streql("image_size", argv[i])) {
	 slot = image_size;
	 set_image_size = 1;
      } else if (streql("center", argv[i]))
	 slot = cp->center;
      else if (streql("background", argv[i]))
	 slot = cp->background;
      else if (streql("pixels_per_unit", argv[i]))
	 slot = &cp->pixels_per_unit;
      else if (streql("spatial_filter_radius", argv[i]))
	 slot = &cp->spatial_filter_radius;
      else if (streql("sample_density", argv[i]))
	 slot = &cp->sample_density;
      else if (streql("nbatches", argv[i])) {
	 slot = &nbatches;
	 set_nbatches = 1;
      } else if (streql("white_level", argv[i])) {
	 slot = &white_level;
	 set_white_level = 1;
      } else if (streql("spatial_oversample", argv[i])) {
	 slot = &spatial_oversample;
	 set_spatial_oversample = 1;
      } else if (streql("cmap", argv[i])) {
	 slot = &cm;
	 set_cm = 1;
      } else if (streql("palette", argv[i])) {
	  slot = &cp->cmap[0][0];
      } else if (streql("density", argv[i]))
	 slot = &cp->xform[(int)xf].density;
      else if (streql("color", argv[i]))
	 slot = &cp->xform[(int)xf].color;
      else if (streql("coefs", argv[i])) {
	 slot = cp->xform[(int)xf].c[0];
	 cp->xform[(int)xf].density = 1.0;
       } else if (streql("var", argv[i]))
	 slot = cp->xform[(int)xf].var;
      else if (streql("cmap_inter", argv[i])) {
	slot = &cmap_inter;
	set_cmap_inter = 1;
      } else
	 *slot++ = atof(argv[i]);
   }
   if (set_cm) {
       double hr = set_hr ? cp->hue_rotation : 0.0;
      cp->cmap_index = (int) cm;
      get_cmap(cp->cmap_index, cp->cmap, 256, hr);
   }
   if (set_image_size) {
      cp->width  = (int) image_size[0];
      cp->height = (int) image_size[1];
   }
   if (set_nbatches)
      cp->nbatches = (int) nbatches;
   if (set_spatial_oversample)
      cp->spatial_oversample = (int) spatial_oversample;
   if (set_white_level)
      cp->white_level = (int) white_level;
   for (i = 0; i < NXFORMS; i++) {
      t = 0.0;
      for (j = 0; j < NVARS; j++)
	 t += cp->xform[i].var[j];
      t = 1.0 / t;
      for (j = 0; j < NVARS; j++)
	 cp->xform[i].var[j] *= t;
   }
}

int
main(int argc, char **argv)
{
#if 0
  control_point *cps;
  int i, j, k, ncps = 0;
  char *inf = getenv("in");
  FILE *in;
  double time = 0.0;
  if (inf)
    in = fopen(inf, "rb");
  else
    in = stdin;
  cps = parse_control_points_from_file(in, &ncps);
  for (i = 0; i < ncps; i++) {
    double t = cps[i].time;
    cps[i].time = time / 29.97;
    time += t;
  }
  for (i = 0; i < ncps; i++) {
    double var[NVARS];
    double sum;
    for (j = 0; j < NVARS; j++) var[j] = 0.0;
    for (j = 0; j < NXFORMS; j++) {
      if (cps[i].xform[j].density > 0.0) {
	for (k = 0; k < NVARS; k++)
	  var[k] += cps[i].xform[j].var[k];
      }
    }
    sum = 0.0;
    for (j = 0; j < NVARS; j++) sum += var[j];
    for (j = 0; j < NVARS; j++) var[j] /= sum;
    printf("%g %d %g", cps[i].time, cps[i].cmap_index, lyapunov(&cps[i], 100));
    for (j = 0; j < NVARS; j++) printf(" %g", var[j]);
    printf("\n");
  }
#else
  char *s, *ss;
  FILE *inf = fopen("nice.place", "rb");
  if (1 != argc) {
    fprintf(stderr, "usage: convert-flame < old-format > new-format\n");
    exit(0);
  }
  {
    int i, c, slen = 5000;
    s = malloc(slen);
    i = 0;
    do {
      c = getc(inf); // getchar();
      if (EOF == c) goto done_reading;
      s[i++] = c;
      if (i == slen-1) {
	slen *= 2;
	s = realloc(s, slen);
      }
    } while (1);
  done_reading:
    s[i] = 0;
  }

  ss = s;
  s += strlen(s);
  printf("<conversions>\n");
  while (strchr(ss, ';')) {
    control_point cp;
    parse_control_point_old(&ss, &cp);
    print_control_point(stdout, &cp, NULL);
  }
  printf("</conversions>\n");
#endif
}
