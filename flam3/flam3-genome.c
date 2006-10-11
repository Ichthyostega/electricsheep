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

static char *flam3_genome_c_id =
"@(#) $Id: flam3-genome.c,v 1.11 2005/07/20 06:06:13 spotspot Exp $";

#include "private.h"

char notes[10000];

void note(char *s) {
    strcat(notes, s);
    strcat(notes, " ");
}

int note_int(int i) {
    char b[20];
    sprintf(b, "%d", i);
    note(b);
    return i;
}

double note_float(double f) {
    char b[40];
    sprintf(b, "%g", f);
    note(b);
    return f;
}

char *get_extras() {
    char *e = getenv("extras");
    char *extras;
    if (strlen(notes) == 0) return e;
    if (NULL == e) e = "";
    extras = malloc(strlen(notes) + strlen(e) + 100);
    sprintf(extras, "%s notes=\"%s\"", e, notes);
	
    return extras; /* leaks */
}
	

void test_cp(flam3_genome *cp) {
   cp->time = 0.0;
   cp->background[0] = 0.0;
   cp->background[1] = 0.0;
   cp->background[2] = 0.0;
   cp->center[0] = 0.0;
   cp->center[1] = 0.0;
   cp->rotate = 0.0;
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
}

flam3_genome *string_to_cp(char *s, int *n) {
  flam3_genome *cp;
  FILE *fp; 
  
  fp = fopen(s, "rb");
  if (NULL == fp) {
    perror(s);
    exit(1);
  }
  cp = flam3_parse_from_file(fp, n);
  if (NULL == cp) {
      fprintf(stderr, "could not read genome from %s.\n", s);
      exit(1);
  }
  return cp;
}

double smoother(double t) {
  return 3*t*t - 2*t*t*t;
}

void
spin(int frame, double blend, flam3_genome *parent)
{
  flam3_genome result;

  result = *parent;

  flam3_rotate(&result, blend*360.0);
  result.time = (double)frame;

  flam3_print(stdout, &result, get_extras());
}

void
spin_inter(int frame, double blend, flam3_genome *parents)
{
  flam3_genome spun[2];
  flam3_genome result;

  spun[0] = parents[0];
  spun[1] = parents[1];

  flam3_rotate(&spun[0], blend*360.0);
  flam3_rotate(&spun[1], blend*360.0);

  spun[0].time = 0.0;
  spun[1].time = 1.0;

  flam3_interpolate(spun, 2, smoother(blend), &result);

  if ((parents[0].palette_index != flam3_palette_random) &&
      (parents[1].palette_index != flam3_palette_random)) {
    result.palette_index = flam3_palette_interpolated;
    result.palette_index0 = parents[0].palette_index;
    result.hue_rotation0 = parents[0].hue_rotation;
    result.palette_index1 = parents[1].palette_index;
    result.hue_rotation1 = parents[1].hue_rotation;
    result.palette_blend = blend;
  }
  result.time = (double)frame;

  flam3_print(stdout, &result, get_extras());
}

void truncate_variations(flam3_genome *g, int max_vars) {
    int i, j, nvars, smallest;
    double sv;
    for (i = 0; i < flam3_nxforms; i++) {
	double d = g->xform[i].density;
	if (0.0 < d && d < 0.001) {
	    note("truncate_density");
	    note_int(i);
	    g->xform[i].density = 0.0;
	} else if (d > 0.0) {
	    do {
		nvars = 0;
		smallest = -1;
		for (j = 0; j < flam3_nvariations; j++) {
		    double v = g->xform[i].var[j];
		    if (v != 0.0) {
			nvars++;
			if (-1 == smallest || fabs(v) < sv) {
			    smallest = j;
			    sv = fabs(v);
			}
		    }
		}
		if (nvars > max_vars) {
		    note("truncate");
		    note_int(i);
		    note_int(smallest);
		    g->xform[i].var[smallest] = 0.0;
		}
	    } while (nvars > max_vars);
	}
    }
}
    

int
main(argc, argv)
   int argc;
   char **argv;
{
   int debug = 0;
   int count;
   char *ai;
   unsigned char *image;
   flam3_genome *templ = NULL;
   flam3_genome cp_orig;
   int i, j;
   double avg_pix, fraction_black, fraction_white;
   double avg_thresh = argf("avg", 20.0);
   double black_thresh = argf("black", 0.01);
   double white_limit = argf("white", 0.05);
   int nframes = argi("nframes", 100);
   int sym = argi("symmetry", 0);
   int enclosed = argi("enclosed", 1);
   char *clone = getenv("clone");
   char *mutate = getenv("mutate");
   char *cross0 = getenv("cross0");
   char *cross1 = getenv("cross1");
   char *method = getenv("method");
   char *inter = getenv("inter");
   char *rotate = getenv("rotate");
   char *sequence = getenv("sequence");
   int loops = argi("loops", 1);
   int frame = argi("frame", 0);
   int rep, repeat = argi("repeat", 1);
   double speed = argf("speed", 0.1);
   int verbose = argi("verbose", 0);
   int bits = argi("bits", 64);
   int ntries = argi("tries", 50);
   int seed = argi("seed", 0);
   flam3_genome *parent0, *parent1;
   int parent0_n, parent1_n;
   int ncp;
   flam3_frame f;

   if (1 != argc) {
     puts(docstring);
     exit(0);
   }

   srandom(seed ? seed : (time(0) + getpid()));

   f.temporal_filter_radius = 0.0;
   f.bits = bits;
   f.verbose = verbose;
   f.genomes = &cp_orig;
   f.ngenomes = 1;
   f.pixel_aspect_ratio = 1.0;
   f.progress = 0;
   test_cp(&cp_orig);  // just for the width & height
   image = (unsigned char *) malloc(3 * cp_orig.width * cp_orig.height);

   if (1 < (!!mutate + !!(cross0 || cross1) + !!inter + !!rotate + !!clone)) {
     fprintf(stderr,
	     "can only specify one of mutate, clone, cross, rotate, or inter.\n");
     exit(1);
   }
   if ( (!cross0) ^ (!cross1) ) {
     fprintf(stderr, "must specify both crossover arguments.\n");
     exit(1);
   }

   if (method && (!cross0)) {
     fprintf(stderr, "cannot specify method unless doing crossover.\n");
     exit(1);
   }

   if (cross0) {
     if (NULL == method) {
       if (flam3_random01() < 0.1)
	 method = "union";
       else if (flam3_random01() < 0.2)
	 method = "interpolate";
       else
	 method = "alternate";
     }
     if (strcmp(method, "alternate") &&
	 strcmp(method, "interpolate") &&
	 strcmp(method, "union")) {
       fprintf(stderr,
	       "method must be either alternate, interpolate, "
	       "or union, not %s.\n", method);
       exit(1);
     } else if (verbose) {
       fprintf(stderr, "crossover method is %s.\n", method);
     }
   }
     
   if (clone) parent0 = string_to_cp(clone, &parent0_n);
   if (mutate) parent0 = string_to_cp(mutate, &parent0_n);
   if (cross0) parent0 = string_to_cp(cross0, &parent0_n);
   if (cross1) parent1 = string_to_cp(cross1, &parent1_n);

   if (sequence) {
     flam3_genome *cp;
     double blend, spread;
     int framecount;
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
     cp = flam3_parse_from_file(fp, &ncp);
     if (NULL == cp) exit(1);
     if (enclosed) printf("<sequence>\n");
     spread = 1.0/nframes;
     framecount = 0;
     for (i = 0; i < ncp; i++) {
       if (loops) {
	 for (frame = 0; frame < nframes; frame++) {
	   blend = frame/(double)nframes;
	   spin(framecount++, blend, &cp[i]);
	 }
       }
       if (i < ncp-1)
	 for (frame = 0; frame < nframes; frame++) {
	   blend = frame/(double)nframes;
	   spin_inter(framecount++, blend, &cp[i]);
	 }
     }
     spin(framecount, 0.0, &cp[ncp-1]);
     if (enclosed) printf("</sequence>\n");
     exit(0);
   }

   if (inter || rotate) {
     flam3_genome *cp;
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
     cp = flam3_parse_from_file(fp, &ncp);
     if (NULL == cp) exit(1);
     if (enclosed) printf("<pick>\n");
     if (rotate) {
       if (1 != ncp) {
	 fprintf(stderr,
		 "rotation requires one control point, not %d.\n",
		 ncp);
	 exit(1);
       }
       spin(frame - 1, blend - spread, cp);
       spin(frame    , blend         , cp);
       spin(frame + 1, blend + spread, cp);
     } else {
       if (2 != ncp) {
	 fprintf(stderr,
		 "interpolation requires two control points, not %d.\n",
		 ncp);
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
     templ = flam3_parse_from_file(template, &ncp);
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
     count = 0;
     do {
       f.time = (double) 0.0;
       notes[0] = 0;

       if (clone) {
	   note("clone");
	   cp_orig = parent0[note_int(random()%parent0_n)];
       } else if (mutate) {
	 flam3_genome mutation;
	 double r = flam3_random01();
	 note("mutate");
	 cp_orig = parent0[note_int(random()%parent0_n)];
	 if (r < 0.1) {
	   int done = 0;
	   if (debug) fprintf(stderr, "mutating variation\n");
	   note("variation");
	   // randomize the variations, usually a large visual effect
	   do {
	     flam3_random(&mutation, flam3_variation_random, sym);
	     for (i = 0; i < flam3_nxforms; i++) {
	       if (cp_orig.xform[i].density > 0.0) {
		 for (j = 0; j < flam3_nvariations; j++) {
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
	   note("xform");
	   flam3_random(&mutation, flam3_variation_random, sym);
	   for (i = 0; i < flam3_nxforms; i++) {
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
	   if (debug) fprintf(stderr, "adding symmetry\n");
	   note("symmetry");
	   flam3_add_symmetry(&cp_orig, 0);
	 } else if (r < 0.6) {
	   int b = 1 + random()%6;
	   int same = random()&3;
	   if (debug) fprintf(stderr, "setting post xform\n");
	   note("post");
	   for (i = 0; i < flam3_nxforms; i++) {
	     int copy = (i > 0) && same;
	     if (cp_orig.xform[i].density == 0.0) continue;
	     if (copy) {
	       for (j = 0; j < 3; j++) {
		 cp_orig.xform[i].post[j][0] = cp_orig.xform[0].post[j][0];
		 cp_orig.xform[i].post[j][1] = cp_orig.xform[0].post[j][1];
	       }
	     } else {
	       if (b&1) {
		 double f = M_PI * flam3_random11();
		 double t[2][2];
		 t[0][0] = (cp_orig.xform[i].c[0][0] * cos(f) +
			    cp_orig.xform[i].c[0][1] * -sin(f));
		 t[0][1] = (cp_orig.xform[i].c[0][0] * sin(f) +
			    cp_orig.xform[i].c[0][1] * cos(f));
		 t[1][0] = (cp_orig.xform[i].c[1][0] * cos(f) +
			    cp_orig.xform[i].c[1][1] * -sin(f));
		 t[1][1] = (cp_orig.xform[i].c[1][0] * sin(f) +
			    cp_orig.xform[i].c[1][1] * cos(f));

		 cp_orig.xform[i].c[0][0] = t[0][0];
		 cp_orig.xform[i].c[0][1] = t[0][1];
		 cp_orig.xform[i].c[1][0] = t[1][0];
		 cp_orig.xform[i].c[1][1] = t[1][1];

		 f *= -1.0;

		 t[0][0] = (cp_orig.xform[i].post[0][0] * cos(f) +
			    cp_orig.xform[i].post[0][1] * -sin(f));
		 t[0][1] = (cp_orig.xform[i].post[0][0] * sin(f) +
			    cp_orig.xform[i].post[0][1] * cos(f));
		 t[1][0] = (cp_orig.xform[i].post[1][0] * cos(f) +
			    cp_orig.xform[i].post[1][1] * -sin(f));
		 t[1][1] = (cp_orig.xform[i].post[1][0] * sin(f) +
			    cp_orig.xform[i].post[1][1] * cos(f));

		 cp_orig.xform[i].post[0][0] = t[0][0];
		 cp_orig.xform[i].post[0][1] = t[0][1];
		 cp_orig.xform[i].post[1][0] = t[1][0];
		 cp_orig.xform[i].post[1][1] = t[1][1];

	       }
	       if (b&2) {
		 double f = 0.2 + flam3_random01();
		 double g;
		 if (random()&1) f = 1.0 / f;
		 if (random()&1) {
		   g = 0.2 + flam3_random01();
		   if (random()&1) g = 1.0 / g;
		 } else
		   g = f;
		 cp_orig.xform[i].c[0][0] /= f;
		 cp_orig.xform[i].c[0][1] /= f;
		 cp_orig.xform[i].c[1][1] /= g;
		 cp_orig.xform[i].c[1][0] /= g;
		 cp_orig.xform[i].post[0][0] *= f;
		 cp_orig.xform[i].post[1][0] *= f;
		 cp_orig.xform[i].post[0][1] *= g;
		 cp_orig.xform[i].post[1][1] *= g;
	       }
	       if (b&4) {
		 double f = flam3_random11();
		 double g = flam3_random11();
		 cp_orig.xform[i].c[2][0] -= f;
		 cp_orig.xform[i].c[2][1] -= g;
		 cp_orig.xform[i].post[2][0] += f;
		 cp_orig.xform[i].post[2][1] += g;
	       }
	     }
	   }
	 } else if (r < 0.7) {
	   // change just the color
	   if (debug) fprintf(stderr, "mutating color\n");
	   note("color");
	   cp_orig.hue_rotation = flam3_random01();
	   cp_orig.palette_index =
	     flam3_get_palette(flam3_palette_random,
			       cp_orig.palette,
			       cp_orig.hue_rotation);
	 } else if (r < 0.8) {
	   int nx = 0;
	   int ny = 0;
	   if (debug) fprintf(stderr, "deleting an xform\n");
	   note("delete");
	   for (i = 0; i < flam3_nxforms; i++) {
	     if (cp_orig.xform[i].density > 0.0)
	       nx++;
	   }
	   if (nx > 1) {
	     nx = random()%nx;
	     for (i = 0; i < flam3_nxforms; i++) {
	       if (nx == ny) {
		 cp_orig.xform[i].density = 0;
		 break;
	       }
	       if (cp_orig.xform[i].density > 0.0)
		 ny++;
	     }
	   } else {
	     if (verbose)
	       fprintf(stderr, "not enough xforms to delete one.\n");
	   }
	 } else {
	   int x;
	   if (debug) fprintf(stderr, "mutating all coefs\n");
	   note("all");
	   flam3_random(&mutation, flam3_variation_random, sym);

	   // change all the coefs by a little bit
	   for (x = 0; x < flam3_nxforms; x++) {
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
	 int i0, i1;
	 note("cross");
	 i0 = note_int(random()%parent0_n);
	 i1 = note_int(random()%parent1_n);
	 note(method);
	 if (!strcmp(method, "alternate")) {
	   /* each xform from a random parent,
	      possible for one to be excluded */
	   cp_orig = note_int(rbit()) ? (parent0[i0]) : (parent1[i1]);
	   for (i = 0; i < flam3_nxforms; i++) {
	     cp_orig.xform[i] =
	       note_int(rbit()) ? (parent0[i0].xform[i]) : (parent1[i1].xform[i]);
	   }
	 } else if (!strcmp(method, "interpolate")) {
	   /* linearly interpolate somewhere between the two */
	   flam3_genome parents[2];
	   double t = note_float(flam3_random01());
	   if (verbose) {
	     fprintf(stderr, "interpolating to %g\n", t);
	   }
	   parents[0] = parent0[i0];
	   parents[1] = parent1[i1];
	   parents[0].time = 0.0;
	   parents[1].time = 1.0;
	   flam3_interpolate(parents, 2, t, &cp_orig);
	   /* except pick a simple palette */
	   cp_orig.palette_index = note_int(rbit()) ?
	     parent0[i0].palette_index : parent1[i1].palette_index;
	 } else {
	   /* union */
	   cp_orig = parent0[i0];

	   i = 0;
	   for (j = 0; j < flam3_nxforms; j++) {
	     if (parent1[i1].xform[j].density != 0.0) {
	       while (cp_orig.xform[i].density != 0.0) {
		 i++;
	         if (i == flam3_nxforms) {
		   fprintf(stderr, "warning: union xform overflow\n");
		   goto done;
		 }
	       }
	       cp_orig.xform[i] = parent1[i1].xform[j];
	     }
	   }
	 done:;
	 }
	 /* find the last xform */
	 nxf = 0;
	 for (i = 0; i < flam3_nxforms; i++) {
	   if (cp_orig.xform[i].density > 0.0) {
	     nxf = i;
	   }
	 }
	 /* then reset color coords */
	 if (nxf > 0) {
	   for (i = 0; i < flam3_nxforms; i++) {
	     cp_orig.xform[i].color[0] = i&1;
	     cp_orig.xform[i].color[1] = (i&2)>>1;
	   }
	 }
       } else {
	 note("random");
	 flam3_random(&cp_orig, flam3_variation_random, sym);
       }
       truncate_variations(&cp_orig, 5);
       test_cp(&cp_orig);
       flam3_render(&f, image, cp_orig.width, flam3_field_both, 3);

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
	 int n, tot, totb, totw;
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

       count++;
     } while ((avg_pix < avg_thresh ||
	       fraction_black < black_thresh ||
	       fraction_white > white_limit) &&
	      count < ntries);

     if (ntries == count) {
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
       cp_orig.width = templ->width;
       cp_orig.height = templ->height;
       cp_orig.pixels_per_unit = templ->pixels_per_unit;
     }

     cp_orig.time = (double)rep;
     
     flam3_print(stdout, &cp_orig, get_extras());
   }
   if (enclosed) printf("</pick>\n");

   return 0;
}
