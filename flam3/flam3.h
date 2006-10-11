/*
    flam3 - cosmic recursive fractal flames
    Copyright (C) 1992-2004  Scott Draves <source@flam3.com>

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


#ifndef flam3_included
#define flam3_included

#include <stdio.h>

static char *flam3_h_id =
"@(#) $Id: flam3.h,v 1.6 2005/07/20 06:06:16 spotspot Exp $";

char *flam3_version();

#define flam3_palette_random       (-1)
#define flam3_palette_interpolated (-2)

typedef double flam3_palette[256][3];

int flam3_get_palette(int palette_index, flam3_palette p, double hue_rotation);
int flam3_get_npalettes();


#define flam3_variation_random (-1)

extern char *flam3_variation_names[];

#define flam3_nvariations 23
#define flam3_nxforms     12

typedef struct {
   double var[flam3_nvariations];   /* interp coefs between variations */
   double c[3][2];      /* the coefs to the affine part of the function */
   double post[3][2];   /* the post transform */
   double density;      /* probability that this function is chosen. 0 - 1 */
   double color[2];     /* color coords for this function. 0 - 1 */
   double symmetry;     /* 1=this is a symmetry xform, 0=not */
} flam3_xform;

typedef struct {
   flam3_xform xform[flam3_nxforms];
   int symmetry;                /* 0 means none */
   flam3_palette palette;
   char *input_image;           /* preview/temporary! */
   double time;
   int  palette_index;
   double brightness;           /* 1.0 = normal */
   double contrast;             /* 1.0 = normal */
   double gamma;
   int  width, height;          /* of the final image */
   int  spatial_oversample;
   double center[2];             /* of camera */
   double rot_center[2];         /* really the center */
   double rotate;                /* camera */
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

  /* for cmap_interpolated hack */
  int palette_index0;
  double hue_rotation0;
  int palette_index1;
  double hue_rotation1;
  double palette_blend;
} flam3_genome;


/* samples is array nsamples*4 long of x,y,color triples.
   using (samples[0], samples[1]) as starting XY point and
   (samples[2], samples[3]) as starting color coordinate,
   perform fuse iterations and throw them away, then perform
   nsamples iterations and save them in the samples array */
void flam3_iterate(flam3_genome *g, int nsamples, int fuse, double *samples);

/* genomes is array ngenomes long, with times set and in ascending order.
   interpolate to the requested time and return in result */
void flam3_interpolate(flam3_genome *genomes, int ngenomes, double time, flam3_genome *result);

/* print genome to given file with extra_attributes if not NULL */
void flam3_print(FILE *f, flam3_genome *g, char *extra_attributes);

/* ivar is either a variation or flam3_variation_random,
   and sym is either a symmetry group or 0 meaning random or no symmetry */
void flam3_random(flam3_genome *g, int ivar, int sym);

/* return NULL in case of error */
flam3_genome *flam3_parse(char *s, int *ncps);
flam3_genome *flam3_parse_from_file(FILE *f, int *ncps);

int flam3_add_symmetry(flam3_genome *g, int sym);

void flam3_estimate_bounding_box(flam3_genome *g, double eps, double *bmin, double *bmax);
void flam3_rotate(flam3_genome *g, double angle); /* angle in degrees */

double flam3_dimension(flam3_genome *g, int ntries, int clip_to_camera);
double flam3_lyapunov(flam3_genome *g, int ntries);

typedef struct {
   double         temporal_filter_radius;
   double         pixel_aspect_ratio;    /* width over height of each pixel */
   flam3_genome  *genomes;
   int            ngenomes;
   int            verbose;
   int            bits;
   double         time;
   int            (*progress)(void *, double);
   void          *progress_parameter;
} flam3_frame;


#define flam3_field_both  0
#define flam3_field_even  1
#define flam3_field_odd   2

/* out is pixel array with stride of out_width.
   pixels are rgb or rgba if nchan is 3 or 4. */
void flam3_render(flam3_frame *f, unsigned char *out, int out_width, int field, int nchan);

double flam3_render_memory_required(flam3_frame *f);


double flam3_random01();
double flam3_random11();
int flam3_random_bit();


#endif
