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

static char *pick_flame_c_id =
"@(#) $Id: pick-flame.c,v 1.15 2004/03/04 06:35:17 spotspot Exp $";

#include "rect.h"


#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))
#define argd(s,d)   ((ai = getenv(s)) ? atof(ai) : (d))


#ifdef WIN32
#define M_PI 3.1415926539
#define random()  (rand() ^ (rand()<<15))
#define srandom(x)  (srand(x))
extern int time();
extern int getpid();
#endif

void
test_cp(control_point *cp) {
   cp->time = 0.0;
   cp->background[0] = 0.0;
   cp->background[1] = 0.0;
   cp->background[2] = 0.0;
   cp->center[0] = 0.0;
   cp->center[1] = 0.0;
   cp->pixels_per_unit = 64;
   cp->width = 128;
   cp->height = 128;
   cp->spatial_oversample = 1;
   cp->gamma = 2.0;
   cp->vibrancy = 1.0;
   cp->contrast = 1.0;
   cp->brightness = 1.0;
   cp->spatial_filter_radius = 0.5;
   cp->zoom = 0.0;
   cp->sample_density = 1;
   cp->nbatches = 10;
   cp->white_level = 200;
}

control_point * string_to_cp(char *s) {
  control_point *cp;
  FILE *fp; 
  int n;
  
  fp = fopen(s, "rb");
  if (NULL == fp) {
    perror(s);
    exit(1);
  }
  cp = parse_control_points_from_file(fp, &n);
  if (NULL == cp) exit(1);
  if (1 < n) {
    fprintf(stderr, "more than one control point in %s, "
	    "ignoring all but first.\n", s);
  }
  return cp;
}

double smoother(double t) {
  return 3*t*t - 2*t*t*t;
}

void
spin(int frame, double blend, control_point *parent)
{
  control_point result;

  result = *parent;

  rotate_control_point(&result, blend*360.0);
  result.time = (double)frame;

  print_control_point(stdout, &result, getenv("extras"));
}

void
spin_inter(int frame, double blend, control_point *parents)
{
  control_point spun[2];
  control_point result;

  spun[0] = parents[0];
  spun[1] = parents[1];

  rotate_control_point(&spun[0], blend*360.0);
  rotate_control_point(&spun[1], blend*360.0);

  spun[0].time = 0.0;
  spun[1].time = 1.0;

  interpolate(spun, 2, smoother(blend), &result);

  result.cmap_index = cmap_interpolated;
  result.cmap_index0 = parents[0].cmap_index;
  result.hue_rotation0 = parents[0].hue_rotation;
  result.cmap_index1 = parents[1].cmap_index;
  result.hue_rotation1 = parents[1].hue_rotation;
  result.palette_blend = blend;
  result.time = (double)frame;

  print_control_point(stdout, &result, getenv("extras"));
}

int
main(argc, argv)
   int argc;
   char **argv;
{
   int debug = 0;
   int count = 0;
   char *ai;
   unsigned char *image;
   control_point *templ = NULL;
   control_point cp_orig;
   int i, j;
   double avg_pix, fraction_black, fraction_white;
   double avg_thresh = argd("avg", 20.0);
   double black_thresh = argd("black", 0.01);
   double white_limit = argd("white", 0.05);
   int nframes = argi("nframes", 100);
   int sym = argi("symmetry", 0);
   int enclosed = argi("enclosed", 1);
   char *mutate = getenv("mutate");
   char *cross0 = getenv("cross0");
   char *cross1 = getenv("cross1");
   char *inter = getenv("inter");
   char *rotate = getenv("rotate");
   char *sequence = getenv("sequence");
   int frame = argi("frame", 0);
   int rep, repeat = argi("repeat", 1);
   double speed = argd("speed", 0.1);
   control_point *parent0;
   control_point *parent1;
   int ncp;
   frame_spec f;

   srandom(time(0) + getpid());

   f.temporal_filter_radius = 0.0;
   f.cps = &cp_orig;
   f.ncps = 1;
   f.pixel_aspect_ratio = 1.0;
   test_cp(&cp_orig);  // just for the width & height
   image = (unsigned char *) malloc(3 * cp_orig.width * cp_orig.height);

   if (1 < (!!mutate + !!(cross0 || cross1) + !!inter + !!rotate)) {
     fprintf(stderr, "can only specify one of mutate, cross, rotate, or inter.\n");
     exit(1);
   }
   if ( (!cross0) ^ (!cross1) ) {
     fprintf(stderr, "must specify both crossover arguments.\n");
     exit(1);
   }

   if (mutate) parent0 = string_to_cp(mutate);
   if (cross0) parent0 = string_to_cp(cross0);
   if (cross1) parent1 = string_to_cp(cross1);

   if (sequence) {
     control_point *cp;
     double blend, spread;
     FILE *fp;

     if (nframes <= 0) {
       fprintf(stderr, "nframes must be positive, not %d.\n", nframes);
       exit(1);
     }
       
     fp = fopen(sequence, "rb");
     if (NULL == fp) {
       perror(sequence);
       exit(1);
     }
     cp = parse_control_points_from_file(fp, &ncp);
     if (NULL == cp) exit(1);
     if (enclosed) printf("<sequence>\n");
     spread = 1.0/nframes;
     for (i = 0; i < ncp; i++) {
       for (frame = 0; frame < nframes; frame++) {
	 blend = frame/(double)nframes;
	 spin(frame + 2*i*nframes, blend, &cp[i]);
       }
       if (i < ncp-1)
	 for (frame = 0; frame < nframes; frame++) {
	   blend = frame/(double)nframes;
	   spin_inter(frame + (2*i+1)*nframes, blend, &cp[i]);
	 }
     }
     spin((2*ncp-1)*nframes, 0.0, &cp[ncp-1]);
     if (enclosed) printf("</sequence>\n");
     exit(0);
   }

   if (inter || rotate) {
     control_point *cp;
     double blend, spread;
     char *fname = inter?inter:rotate;
     FILE *fp;

     if (nframes <= 0) {
       fprintf(stderr, "nframes must be positive, not %d.\n", nframes);
       exit(1);
     }
       
     blend = frame/(double)nframes;
     spread = 1.0/nframes;
       
     fp = fopen(fname, "rb");
     if (NULL == fp) {
       perror(fname);
       exit(1);
     }
     cp = parse_control_points_from_file(fp, &ncp);
     if (NULL == cp) exit(1);
     if (enclosed) printf("<pick>\n");
     if (rotate) {
       if (1 != ncp) {
	 fprintf(stderr, "rotation requires one control point, not %d.\n", ncp);
	 exit(1);
       }
       spin(frame - 1, blend - spread, cp);
       spin(frame    , blend         , cp);
       spin(frame + 1, blend + spread, cp);
     } else {
       if (2 != ncp) {
	 fprintf(stderr, "interpolation requires two control points, not %d.\n", ncp);
	 exit(1);
       }
       spin_inter(frame - 1, blend - spread, cp);
       spin_inter(frame    , blend         , cp);
       spin_inter(frame + 1, blend + spread, cp);
     }
     if (enclosed) printf("</pick>\n");
     exit(0);
   }

   if (getenv("template")) {
     char *tf = getenv("template");
     FILE *template = fopen(tf, "rb");
     if (0 == template) {
       perror(tf);
       exit(1);
     }
     templ = parse_control_points_from_file(template, &ncp);
     if (1 < ncp) {
       fprintf(stderr, "more than one control point in template, "
	       "ignoring all but first.\n");
     } else if (0 == ncp) {
       fprintf(stderr, "no control points in template.\n");
     }
   }

   /* pick a control point until it looks good enough */

   if (repeat <= 0) {
     fprintf(stderr, "repeat must be positive, not %d.\n", repeat);
     exit(1);
   }

   if (enclosed) printf("<pick>\n");

   for (rep = 0; rep < repeat; rep++) {
     do {
       f.time = (double) 0.0;

       if (mutate) {
	 control_point mutation;
	 double r = random_uniform01();
	 cp_orig = *parent0;
	 if (r < 0.1) {
	   int done = 0;
	   if (debug) fprintf(stderr, "mutating variation\n");
	   // randomize the variations, usually a large visual effect
	   do {
	     random_control_point(&mutation, variation_random, sym);
	     for (i = 0; i < NXFORMS; i++) {
	       if (cp_orig.xform[i].density > 0.0) {
		 for (j = 0; j < NVARS; j++) {
		   if (cp_orig.xform[i].var[j] !=
		       mutation.xform[i].var[j]) {
		     cp_orig.xform[i].var[j] = 
		       mutation.xform[i].var[j];
		     done = 1;
		   }
		 }
	       }
	     }
	   } while (!done);
	 } else if (r < 0.3) {
	   // change one xform coefs but not the vars
	   int xf, nxf = 0;
	   if (debug) fprintf(stderr, "mutating one xform\n");
	   random_control_point(&mutation, variation_random, sym);
	   for (i = 0; i < NXFORMS; i++) {
	     if (cp_orig.xform[i].density > 0.0) {
	       nxf++;
	     }
	   }

	   if (0 == nxf) {
	     fprintf(stderr, "no xforms in control point.\n");
	     exit(1);
	   }
	   xf = random()%nxf;

	   // if only two xforms, then change only the translation part
	   if (2 == nxf) {
	     for (j = 0; j < 2; j++)
	       cp_orig.xform[xf].c[2][j] = mutation.xform[0].c[2][j];
	   } else {
	     for (i = 0; i < 3; i++)
	       for (j = 0; j < 2; j++)
		 cp_orig.xform[xf].c[i][j] = mutation.xform[0].c[i][j];
	   }
	 } else if (r < 0.5) {
	   add_symmetry_to_control_point(&cp_orig, 0);
	 } else if (r < 0.7) {
	   // change just the color
	   if (debug) fprintf(stderr, "mutating color\n");
	   cp_orig.hue_rotation = random_uniform01();
	   cp_orig.cmap_index =
	     get_cmap(cmap_random, cp_orig.cmap, 256, cp_orig.hue_rotation);
	 } else {
	   int x;
	   if (debug) fprintf(stderr, "mutating all coefs\n");
	   random_control_point(&mutation, variation_random, sym);

	   // change all the coefs by a little bit
	   for (x = 0; x < NXFORMS; x++) {
	     if (cp_orig.xform[x].density > 0.0) {
	       for (i = 0; i < 3; i++)
		 for (j = 0; j < 2; j++)
		   cp_orig.xform[x].c[i][j] +=
		     speed * mutation.xform[x].c[i][j];
	     }
	   }
	 }
       } else if (cross0) {
	 int nxf;
	 if (random()&1) {
	   /* alternate xforms from parents */
	   int i, first = random()&1;
	   cp_orig = (random()&1) ? (*parent0) : (*parent1);
	   for (i = 0; i < NXFORMS; i++) {
	     cp_orig.xform[i] =
	       (first++&1) ? (parent0->xform[i]) : (parent1->xform[i]);
	   }
	 } else {
	   /* linearly interpolate somewhere between the two */
	   control_point parents[2];
	   double t = random_uniform01();
	   parents[0] = *parent0;
	   parents[1] = *parent1;
	   parents[0].time = 0.0;
	   parents[1].time = 1.0;
	   interpolate(parents, 2, t, &cp_orig);
	   /* except pick a simple palette */
	   cp_orig.cmap_index = (random()&1) ? parent0->cmap_index : parent1->cmap_index;
	 }
	 /* find the last xform */
	 nxf = 0;
	 for (i = 0; i < NXFORMS; i++) {
	   if (cp_orig.xform[i].density > 0.0) {
	     nxf = i;
	   }
	 }
	 /* then reset color coords */
	 if (nxf > 0) {
	   for (i = 0; i < NXFORMS; i++) {
	     cp_orig.xform[i].color = i/(double)nxf;
	   }
	 }
       } else {
	 random_control_point(&cp_orig, variation_random, sym);
       }
       test_cp(&cp_orig);
       render_rectangle(&f, image, cp_orig.width, field_both, 3, 0);

#if 0
       if (0) {
	 FILE *fp;
	 char n[] = "/tmp/pick-flame.ppm.XXXXXX";
	 int tmp_fd = mkstemp(n);
	 fp = fopen(n, "wb");
	 if (!fp) {
	   perror("image out");
	   exit(1);
	 }
	 fprintf(fp, "P6\n");
	 fprintf(fp, "%d %d\n255\n", cp_orig.width, cp_orig.height);
	 fwrite(image, 1, 3 * cp_orig.width * cp_orig.height, fp);
	 fclose(fp);
       }
#endif

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

	 if (debug)
	   fprintf(stderr,
		   "avg_pix=%g fraction_black=%g fraction_white=%g n=%g\n",
		   avg_pix, fraction_black, fraction_white, (double)n);
       } else {
	 avg_pix = avg_thresh + 1.0;
	 fraction_black = black_thresh + 1.0;
	 fraction_white = white_limit - 1.0;
       }

     } while ((avg_pix < avg_thresh ||
	       fraction_black < black_thresh ||
	       fraction_white > white_limit) &&
	      count++ < 50);

     if (50 == count) {
       fprintf(stderr, "warning: reached maximum attempts, giving up.\n");
     }

     if (templ) {
       cp_orig.gamma = templ->gamma;
       cp_orig.vibrancy = templ->vibrancy;
       cp_orig.contrast = templ->contrast;
       cp_orig.brightness = templ->brightness;
       cp_orig.background[0] = templ->background[0];
       cp_orig.background[1] = templ->background[1];
       cp_orig.background[2] = templ->background[2];
       cp_orig.center[0] = templ->center[0];
       cp_orig.center[1] = templ->center[1];
       cp_orig.zoom = templ->zoom;
       cp_orig.spatial_oversample = templ->spatial_oversample;
       cp_orig.spatial_filter_radius = templ->spatial_filter_radius;
       cp_orig.sample_density = templ->sample_density;
       cp_orig.nbatches = templ->nbatches;
       cp_orig.white_level = templ->white_level;
       cp_orig.width = templ->width;
       cp_orig.height = templ->height;
       cp_orig.pixels_per_unit = templ->pixels_per_unit;
     }

     cp_orig.time = (double)rep;
     
     print_control_point(stdout, &cp_orig, getenv("extras"));
   }
   if (enclosed) printf("</pick>\n");

   exit(0);
}
