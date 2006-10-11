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





/* for batch
 *   interpolate
 *   compute colormap
 *   for subbatch
 *     compute samples
 *     buckets += cmap[samples]
 *   accum += time_filter[batch] * log(buckets)
 * image = filter(accum)
 */



/* allow this many iterations for settling into attractor */
#define FUSE 15

/* clamp spatial filter to zero at this std dev (2.5 ~= 0.0125) */
#define FILTER_CUTOFF 1.8

#define PREFILTER_WHITE 255
#define WHITE_LEVEL 255
#define SUB_BATCH_SIZE 10000



static void render_rectangle(spec, out, out_width, field, nchan)
   flam3_frame *spec;
   unsigned char *out;
   int out_width;
   int field;
   int nchan;
{
   int i, j, k, nbuckets, batch_num;
   double nsamples, batch_size, sub_batch;
   bucket  *buckets;
   abucket *accumulate;
   double *points;
   double *filter, *temporal_filter, *temporal_deltas;
   double bounds[4], size[2], ppux, ppuy;
   double rot[2][2];
   int image_width, image_height;    /* size of the image to produce */
   int width, height;               /* size of histogram */
   int filter_width;
   int oversample = spec->genomes[0].spatial_oversample;
   int nbatches = spec->genomes[0].nbatches;
   bucket cmap[CMAP_SIZE];
   unsigned char *cmap2;
   int gutter_width;
   int sbc;
   double vibrancy = 0.0;
   double gamma = 0.0;
   double background[3];
   int vib_gam_n = 0;
   time_t progress_timer = 0, progress_began,
     progress_timer_history[64] = {0};
   double progress_history[64] = {0};
   int progress_history_mark = 0;
   int verbose = spec->verbose;
   char *fname = getenv("image");

   if (nbatches < 1) {
       fprintf(stderr, "nbatches must be positive," " not %d.\n", nbatches);
       exit(1);
   }

   if (oversample < 1) {
       fprintf(stderr, "oversample must be positive," " not %d.\n", oversample);
       exit(1);
   }

   image_width = spec->genomes[0].width;
   if (field) {
      image_height = spec->genomes[0].height / 2;
      if (field == flam3_field_odd)
	 out += nchan * out_width;
      out_width *= 2;
   } else
      image_height = spec->genomes[0].height;

   if (1) {
       double fw =  (2.0 * FILTER_CUTOFF * oversample *
		     spec->genomes[0].spatial_filter_radius /
		     spec->pixel_aspect_ratio);
       double adjust;
       filter_width = ((int) fw) + 1;
      /* make sure it has same parity as oversample */
      if ((filter_width ^ oversample) & 1)
	 filter_width++;
      if (fw > 0.0)
	adjust = FILTER_CUTOFF * filter_width / fw;
      else
	adjust = 1.0;

#if 0
      fprintf(stderr, "fw = %g filter_width = %d adjust=%g\n",
	      fw, filter_width, adjust);
#endif

      filter = (double *) malloc(sizeof(double) * filter_width * filter_width);
      /* fill in the coefs */
      for (i = 0; i < filter_width; i++)
	 for (j = 0; j < filter_width; j++) {
	    double ii = ((2.0 * i + 1.0) / filter_width - 1.0) * adjust;
	    double jj = ((2.0 * j + 1.0) / filter_width - 1.0) * adjust;
	    if (field) jj *= 2.0;
	    jj /= spec->pixel_aspect_ratio;
	    filter[i + j * filter_width] =
	       exp(-2.0 * (ii * ii + jj * jj));
	 }

      if (normalize_vector(filter, filter_width * filter_width)) {
	  fprintf(stderr, "spatial filter value is too small: %g.\n",
		  spec->genomes[0].spatial_filter_radius);
	  exit(1);
      }
#if 0
      printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
      for (j = 0; j < filter_width; j++) {
	 for (i = 0; i < filter_width; i++) {
	   printf(" %5d", (int)(10000 * filter[i + j * filter_width]));
	 }
	 printf("\n");
      }
      printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
      fflush(stdout);
#endif
   }
   temporal_filter = (double *) malloc(sizeof(double) * nbatches);
   temporal_deltas = (double *) malloc(sizeof(double) * nbatches);
   if (nbatches > 1) {
      double t;
      /* fill in the coefs */
      for (i = 0; i < nbatches; i++) {
	 t = temporal_deltas[i] = (2.0 * ((double) i / (nbatches - 1)) - 1.0)
	    * spec->temporal_filter_radius;
	 temporal_filter[i] = exp(-2.0 * t * t);
      }

      if (normalize_vector(temporal_filter, nbatches)) {
	  fprintf(stderr, "temporal filter value is too small: %g.\n",
		  spec->temporal_filter_radius);
	  exit(1);
      }
   } else {
      temporal_filter[0] = 1.0;
      temporal_deltas[0] = 0.0;
   }

#if 0
   for (j = 0; j < nbatches; j++)
      fprintf(stderr, "%4f %4f\n", temporal_deltas[j], temporal_filter[j]);
   fprintf(stderr, "\n");
#endif
   
   /* the number of additional rows of buckets we put at the edge so
      that the filter doesn't go off the edge */

   gutter_width = (filter_width - oversample) / 2;
   height = oversample * image_height + 2 * gutter_width;
   width  = oversample * image_width  + 2 * gutter_width;

   nbuckets = width * height;
   if (1) {
     char *last_block = NULL;
     int memory_rqd = (sizeof(bucket) * nbuckets +
		       sizeof(abucket) * nbuckets +
		       4 * sizeof(double) * SUB_BATCH_SIZE);
     last_block = (char *) malloc(memory_rqd);
     if (NULL == last_block) {
       fprintf(stderr, "render_rectangle: cannot malloc %d bytes.\n", memory_rqd);
       fprintf(stderr, "render_rectangle: h=%d w=%d nb=%d.\n", width, height, nbuckets);
       exit(1);
     }
     /* else fprintf(stderr, "render_rectangle: mallocked %dMb.\n", Mb); */

     buckets = (bucket *) last_block;
     accumulate = (abucket *) (last_block + sizeof(bucket) * nbuckets);
     points = (double *)  (last_block + (sizeof(bucket) + sizeof(abucket)) * nbuckets);
   }

   if (verbose) {
     fprintf(stderr, "chaos: ");
     progress_began = time(NULL);
   }

   if (fname) {
       int len = strlen(fname);
       FILE *fin = fopen(fname, "rb");
       int w, h;
       cmap2 = NULL;
       if (len > 4) {
	   char *ending = fname + len - 4;
	   if (!strcmp(ending, ".png")) {
	       cmap2 = read_png(fin, &w, &h);
	   } else if  (!strcmp(ending, ".jpg")) {
	       cmap2 = read_jpeg(fin, &w, &h);
	   }
       }
       if (NULL == cmap2) {
	   perror(fname);
	   exit(1);
       }
       if (256 != w || 256 != h) {
	   fprintf(stderr, "image must be 256 by 256, not %d by %d.\n", w, h);
	   exit(1);
       }
   }

   background[0] = background[1] = background[2] = 0.0;
   memset((char *) accumulate, 0, sizeof(abucket) * nbuckets);
   for (batch_num = 0; batch_num < nbatches; batch_num++) {
      double batch_time;
      double sample_density;
      flam3_genome cp;
      memset((char *) buckets, 0, sizeof(bucket) * nbuckets);
      batch_time = spec->time + temporal_deltas[batch_num];

      /* interpolate and get a control point */
      flam3_interpolate(spec->genomes, spec->ngenomes, batch_time, &cp);

      /* compute the colormap entries.  the input colormap is 256 long with
	 entries from 0 to 1.0 */
      if (!fname) {
	  for (j = 0; j < CMAP_SIZE; j++) {
	      for (k = 0; k < 3; k++) {
#if 1
		  cmap[j][k] = (int) (cp.palette[(j * 256) / CMAP_SIZE][k] * WHITE_LEVEL);
#else
		  /* monochrome if you don't have any cmaps */
		  cmap[j][k] = WHITE_LEVEL;
#endif	    
	      }
	      cmap[j][3] = WHITE_LEVEL;
	  }
      }

      /* compute camera */
      if (1) {
	double t0, t1, shift, corner0, corner1;
	double scale;

	if (cp.sample_density <= 0.0) {
	  fprintf(stderr,
		  "sample density (quality) must be greater than zero,"
		  " not %g.\n", cp.sample_density);
	  exit(1);
	}

	scale = pow(2.0, cp.zoom);
	sample_density = cp.sample_density * scale * scale;
	
	
	ppux = cp.pixels_per_unit * scale;
	ppuy = field ? (ppux / 2.0) : ppux;
	ppux /=  spec->pixel_aspect_ratio;
	switch (field) {
	case flam3_field_both: shift =  0.0; break;
	case flam3_field_even: shift = -0.5; break;
	case flam3_field_odd:  shift =  0.5; break;
	}
	shift = shift / ppux;
	t0 = (double) gutter_width / (oversample * ppux);
	t1 = (double) gutter_width / (oversample * ppuy);
	corner0 = cp.center[0] - image_width / ppux / 2.0;
	corner1 = cp.center[1] - image_height / ppuy / 2.0;
	bounds[0] = corner0 - t0;
	bounds[1] = corner1 - t1 + shift;
	bounds[2] = corner0 + image_width  / ppux + t0;
	bounds[3] = corner1 + image_height / ppuy + t1 + shift;
	size[0] = 1.0 / (bounds[2] - bounds[0]);
	size[1] = 1.0 / (bounds[3] - bounds[1]);
	rot[0][0] = cos(cp.rotate * 2 * M_PI / 360.0);
	rot[0][1] = -sin(cp.rotate * 2 * M_PI / 360.0);
	rot[1][0] = -rot[0][1];
	rot[1][1] = rot[0][0];
      }

      nsamples = (sample_density * (double) nbuckets /
			(oversample * oversample));
#if 0
      fprintf(stderr, "sample_density=%g nsamples=%g nbuckets=%d\n",
	      sample_density, nsamples, nbuckets);
#endif
      
      batch_size = nsamples / cp.nbatches;

      sbc = 0;
      for (sub_batch = 0;
	   sub_batch < batch_size;
	   sub_batch += SUB_BATCH_SIZE) {

	if (spec->progress&&!(sbc++&7))
	  if ((*spec->progress)(spec->progress_parameter, 0.5*sub_batch/(double)batch_size))
	    return;

	if (verbose && time(NULL) != progress_timer) {
	  double percent = 100.0 * ((sub_batch / (double) batch_size) + batch_num) / nbatches;
	  double eta;
	  int old_mark = 0;
				
	  fprintf(stderr, "\rchaos: %5.1f%%", percent);
	  progress_timer = time(NULL);
	  if (progress_timer_history[progress_history_mark] &&
	      progress_history[progress_history_mark] < percent)
	    old_mark = progress_history_mark;
				
	  if (percent > 0) {
	    eta = (100 - percent) * (progress_timer - progress_timer_history[old_mark])
	      / (percent - progress_history[old_mark]);
	    
	    if (eta > 100)
	      fprintf(stderr, "  ETA: %.1f minutes", eta / 60);
	    else
	      fprintf(stderr, "  ETA: %ld seconds ", (long) ceil(eta));
	    fprintf(stderr, "              \r");
	  }

	  progress_timer_history[progress_history_mark] = progress_timer;
	  progress_history[progress_history_mark] = percent;
	  progress_history_mark = (progress_history_mark + 1) % 64;
	}


	 /* generate a sub_batch_size worth of samples */
	 points[0] = flam3_random11();
	 points[1] = flam3_random11();
	 points[2] = flam3_random01();
	 points[3] = flam3_random01();
	 flam3_iterate(&cp, SUB_BATCH_SIZE, FUSE, points);
	 
	 
	 /* merge them into buckets, looking up colors */
	 for (j = 0; j < SUB_BATCH_SIZE; j++) {
	    double p0, p1, p00, p11;
	    int k, color_index0, color_index1;
	    double *p = &points[4*j];
	    bucket *b;

	    if (cp.rotate != 0.0) {
		p00 = p[0] - cp.rot_center[0];
		p11 = p[1] - cp.rot_center[1];
		p0 = p00 * rot[0][0] + p11 * rot[0][1] + cp.rot_center[0];
		p1 = p00 * rot[1][0] + p11 * rot[1][1] + cp.rot_center[1];
	    } else {
		p0 = p[0];
		p1 = p[1];
	    }

	    if (p0 < bounds[0] || p1 < bounds[1] ||
		p0 > bounds[2] || p1 > bounds[3])
	       continue;
	    if (fname) {
		int ci;
		color_index0 = (int) (p[2] * CMAP_SIZE);
		color_index1 = (int) (p[3] * CMAP_SIZE);
		if (color_index0 < 0) color_index0 = 0;
		else if (color_index0 > (CMAP_SIZE-1))
		    color_index0 = CMAP_SIZE-1;
		if (color_index1 < 0) color_index1 = 0;
		else if (color_index1 > (CMAP_SIZE-1))
		    color_index1 = CMAP_SIZE-1;
		b = buckets +
		    (int) (width * (p0 - bounds[0]) * size[0]) +
		    width * (int) (height * (p1 - bounds[1]) * size[1]);
		ci = 4 * (CMAP_SIZE * color_index1 + color_index0);
		for (k = 0; k < 4; k++)
		    // b[0][k] += cmap2[ci+k];
		    bump_no_overflow(b[0][k], cmap2[ci+k], ACCUM_T);
	    } else {
		color_index0 = (int) (p[2] * CMAP_SIZE);
		if (color_index0 < 0) color_index0 = 0;
		else if (color_index0 > (CMAP_SIZE-1))
		    color_index0 = CMAP_SIZE-1;

		b = buckets +
		    (int) (width * (p0 - bounds[0]) * size[0]) +
		    width * (int) (height * (p1 - bounds[1]) * size[1]);
		for (k = 0; k < 4; k++)
		    // b[0][k] += cmap[color_index0][k];
		    bump_no_overflow(b[0][k], cmap[color_index0][k], ACCUM_T);
	    }
	 }
      }
      
      if (1) {
	 double k1 =(cp.contrast * cp.brightness *
		     PREFILTER_WHITE * 268.0 *
		     temporal_filter[batch_num]) / 256;
	 double area = image_width * image_height / (ppux * ppuy);
	 double k2 = (oversample * oversample * nbatches) /
	    (cp.contrast * area * WHITE_LEVEL * sample_density);

	 vibrancy += cp.vibrancy;
	 gamma += cp.gamma;
	 background[0] += cp.background[0];
	 background[1] += cp.background[1];
	 background[2] += cp.background[2];
	 vib_gam_n++;

	 /* log intensity */
	 for (j = 0; j < height; j++) {
	   for (i = 0; i < width; i++) {
	     abucket *a = accumulate + i + j * width;
	     bucket *b = buckets + i + j * width;
	     double c[4], ls;
	     
	     c[0] = (double) b[0][0];
	     c[1] = (double) b[0][1];
	     c[2] = (double) b[0][2];
	     c[3] = (double) b[0][3];
	     if (0.0 == c[3])
	       continue;
	       
	     ls = (k1 * log(1.0 + c[3] * k2))/c[3];
	     c[0] *= ls;
	     c[1] *= ls;
	     c[2] *= ls;
	     c[3] *= ls;

	       bump_no_overflow(a[0][0], c[0], ACCUM_T);
	       bump_no_overflow(a[0][1], c[1], ACCUM_T);
	       bump_no_overflow(a[0][2], c[2], ACCUM_T);
	       bump_no_overflow(a[0][3], c[3], ACCUM_T);
	   }
	 }
      }
   }

   if (verbose) {
     fprintf(stderr, "\rchaos: 100.0%%  took: %ld seconds   \n",
	     time(NULL) - progress_began);
     fprintf(stderr, "filtering...");
   }
 
   /*
    * filter the accumulation buffer down into the image
    */
   if (1) {
      int x, y;
      double t[4];
      double g = 1.0 / (gamma / vib_gam_n);
      vibrancy /= vib_gam_n;
      background[0] /= vib_gam_n/256.0;
      background[1] /= vib_gam_n/256.0;
      background[2] /= vib_gam_n/256.0;
      y = 0;

      for (j = 0; j < image_height; j++) {
	 if (spec->progress && !(j&15))
	   if ((*spec->progress)(spec->progress_parameter, 0.5+0.5*j/(double)image_height))
	     return;
	 x = 0;
	 for (i = 0; i < image_width; i++) {
	    int ii, jj;
	    unsigned char *p;
	    double ls, a;
	    double alpha;
	    t[0] = t[1] = t[2] = t[3] = 0.0;
	    for (ii = 0; ii < filter_width; ii++)
	       for (jj = 0; jj < filter_width; jj++) {
		  double k = filter[ii + jj * filter_width];
		  abucket *a = accumulate + x + ii + (y + jj) * width;
		  
		  t[0] += k * a[0][0];
		  t[1] += k * a[0][1];
		  t[2] += k * a[0][2];
		  t[3] += k * a[0][3];
	       }
	    p = out + nchan * (i + j * out_width);

	    alpha = t[3];
	    if (alpha > 0.0) {
		ls = vibrancy * 256.0 * pow(alpha / PREFILTER_WHITE, g)
		    / (alpha / PREFILTER_WHITE);
		alpha = pow(alpha / PREFILTER_WHITE, g);
		if (alpha < 0.0) alpha = 0.0;
		else if (alpha > 1.0) alpha = 1.0;
	    } else
		ls = 0.0;

	    /* apply to rgb channels the relative scale from gamma of alpha channel */
	    
	    a = ls * ((double) t[0] / PREFILTER_WHITE);
	    a += (1.0-vibrancy) * 256.0 * pow((double) t[0] / PREFILTER_WHITE, g);
	    a += ((1.0 - alpha) * background[0]);
	    if (a < 0) a = 0; else if (a > 255) a = 255;
	    p[0] = (unsigned char) a;

	    a = ls * ((double) t[1] / PREFILTER_WHITE);
	    a += (1.0-vibrancy) * 256.0 * pow((double) t[1] / PREFILTER_WHITE, g);
	    a += ((1.0 - alpha) * background[1]);
	    if (a < 0) a = 0; else if (a > 255) a = 255;
	    p[1] = (unsigned char) a;
	    
	    a = ls * ((double) t[2] / PREFILTER_WHITE);
	    a += (1.0-vibrancy) * 256.0 * pow((double) t[2] / PREFILTER_WHITE, g);
	    a += ((1.0 - alpha) * background[2]);
	    if (a < 0) a = 0; else if (a > 255) a = 255;
	    p[2] = (unsigned char) a;

	    if (nchan > 3)
	      p[3] = (unsigned char) (alpha * 255.999);

	    x += oversample;
	 }
	 y += oversample;
      }
   }
   
   free(filter);
   free(buckets);
   if (fname) free(cmap2);
}
