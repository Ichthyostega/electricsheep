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


#ifndef libifs_included
#define libifs_included

static char *libifs_h_id =
"@(#) $Id: libifs.h,v 1.15 2004/03/28 23:16:53 spotspot Exp $";

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cmap.h"

#define EPS (1e-10)

#define variation_random (-1)
#define variation_none   (-1)

#define NVARS   18
#define NXFORMS 12

typedef double point[3];

typedef struct {
   double var[NVARS];   /* normalized interp coefs between variations */
   double c[3][2];      /* the coefs to the affine part of the function */
   double density;      /* probability that this function is chosen. 0 - 1 */
   double color;        /* color coord for this function. 0 - 1 */
   double symmetry;     /* 1=this is a symmetry xform, 0=not */
} xform;

typedef struct {
   xform xform[NXFORMS];
   int symmetry;                /* 0 means none */
   clrmap cmap;
   double time;
   int  cmap_index;
   double brightness;           /* 1.0 = normal */
   double contrast;             /* 1.0 = normal */
   double gamma;
   int  width, height;          /* of the final image */
   int  spatial_oversample;
   double center[2];             /* camera center */
   double vibrancy;              /* blend between color algs (0=old,1=new) */
   double hue_rotation;          /* applies to cmap, 0-1 */
   double background[3];
   double zoom;                  /* effects ppu, sample density, scale */
   double pixels_per_unit;       /* vertically */
   double spatial_filter_radius; /* variance of gaussian */
   double sample_density;        /* samples per pixel (not bucket) */
   /* in order to motion blur more accurately we compute the logs of the 
      sample density many times and average the results.  we interplate
      only this many times. */
   int nbatches;
   /* this much color resolution.  but making it too high induces clipping */
   int white_level;

  /* for cmap_interpolated hack */
  int cmap_index0;
  double hue_rotation0;
  int cmap_index1;
  double hue_rotation1;
  double palette_blend;
} control_point;



void iterate(control_point *cp, int n, int fuse, point points[]);
void interpolate(control_point cps[], int ncps, double time, control_point *result);
void print_control_point(FILE *f, control_point *cp, char *extra_attributes);
void random_control_point(control_point *cp, int ivar, int sym);
control_point *parse_control_points(char *s, int *ncps);
control_point *parse_control_points_from_file(FILE *f, int *ncps);
int add_symmetry_to_control_point(control_point *cp, int sym);
void estimate_bounding_box(control_point *cp, double eps,
			   double *bmin, double *bmax);
void rotate_control_point(control_point *cp, double by);
double lyapunov(control_point *cp, int ntries);
double random_uniform01();
double random_uniform11();
double random_gaussian();
#endif
