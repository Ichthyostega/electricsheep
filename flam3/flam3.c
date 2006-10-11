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

static char *libifs_c_id =
"@(#) $Id: flam3.c,v 1.9 2005/07/20 06:06:16 spotspot Exp $";


#include "private.h"
#include "img.h"


char *flam3_version() {
    return VERSION;
}


#define SUB_BATCH_SIZE     10000
#define CHOOSE_XFORM_GRAIN 10000

#define random_distrib(v) ((v)[random()%vlen(v)])


#define coef   cp->xform[fn].c
#define vari   cp->xform[fn].var
#define pcoef  cp->xform[fn].post

#define badvalue(x) (((x)!=(x))||((x)>1e10)||((x)<-1e10))

/*
 * run the function system described by CP forward N generations.  store
 * the N resulting 4-vectors in SAMPLES.  the initial point is passed in
 * SAMPLES[0..3].  ignore the first FUSE iterations.
 */
void flam3_iterate(flam3_genome *cp, int n, int fuse,  double *samples) {
   int i, j, count_large = 0, count_nan = 0;
   char xform_distrib[CHOOSE_XFORM_GRAIN];
   double p[4], t, r, dr;

   p[0] = samples[0];
   p[1] = samples[1];
   p[2] = samples[2];
   p[3] = samples[3];

   /*
    * first, set up xform, which is an array that converts a uniform random
    * variable into one with the distribution dictated by the density
    * fields 
    */
   dr = 0.0;
   for (i = 0; i < flam3_nxforms; i++) {
       double d = cp->xform[i].density;
       if (d < 0.0) {
	   fprintf(stderr, "xform weight must be non-negative, not %g.\n", d);
	   exit(1);
       }
      dr += d;
   }
   if (dr == 0.0) {
       fprintf(stderr, "cannot iterate empty flame.\n");
       exit(1);
   }
   dr = dr / CHOOSE_XFORM_GRAIN;

   j = 0;
   t = cp->xform[0].density;
   r = 0.0;
   for (i = 0; i < CHOOSE_XFORM_GRAIN; i++) {
      while (r >= t) {
	 j++;
	 t += cp->xform[j].density;
      }
      xform_distrib[i] = j;
      r += dr;
   }

   for (i = -fuse; i < n; i++) {
      int fn = xform_distrib[random() % CHOOSE_XFORM_GRAIN];
      double tx, ty, v;

      /* first compute the color coord */
      {
	double s = cp->xform[fn].symmetry;
	double s1 = 1.0 - s;
	p[2] = (p[2] + cp->xform[fn].color[0]) * 0.5 * s1 + s * p[2];
	p[3] = (p[3] + cp->xform[fn].color[1]) * 0.5 * s1 + s * p[3];
      }

      /* then apply the affine part of the function */
      tx = coef[0][0] * p[0] + coef[1][0] * p[1] + coef[2][0];
      ty = coef[0][1] * p[0] + coef[1][1] * p[1] + coef[2][1];

      p[0] = p[1] = 0.0;
      /* then add in proportional amounts of each of the variations */
      v = vari[0];
      if (v != 0.0) {
	 /* linear */
	 double nx, ny;
	 nx = tx;
	 ny = ty;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }
      
      v = vari[1];
      if (v != 0.0) {
	 /* sinusoidal */
	 double nx, ny;
	 nx = sin(tx);
	 ny = sin(ty);
	 p[0] += v * nx;
	 p[1] += v * ny;
      }
      
      v = vari[2];
      if (v != 0.0) {
	 /* spherical */
	 double nx, ny;
	 double r2 = tx * tx + ty * ty + 1e-6;
	 nx = tx / r2;
	 ny = ty / r2;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[3];
      if (v != 0.0) {
	 /* swirl */
	 double r2 = tx * tx + ty * ty;  /* /k here is fun */
#if 1
	 double c1 = sin(r2);
	 double c2 = cos(r2);
	 double nx = c1 * tx - c2 * ty;
	 double ny = c2 * tx + c1 * ty;
#else
	 double c1, c2, nx, ny;
	 sincos(r2, &c1, &c2);
	 nx = c1 * tx - c2 * ty;
	 ny = c2 * tx + c1 * ty;
#endif
	 p[0] += v * nx;
	 p[1] += v * ny;
      }
      
      v = vari[4];
      if (v != 0.0) {
	 /* horseshoe */
	 double a, c1, c2, nx, ny;
	 a = atan2(tx, ty);
	 c1 = sin(a);
	 c2 = cos(a);
	 nx = c1 * tx - c2 * ty;
	 ny = c2 * tx + c1 * ty;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[5];
      if (v != 0.0) {
	/* polar */
	 double nx, ny;
	 nx = atan2(tx, ty) / M_PI;
	 ny = sqrt(tx * tx + ty * ty) - 1.0;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[6];
      if (v != 0.0) {
	/* folded handkerchief */
	 double a, r;
	 a = atan2(tx, ty);
	 r = sqrt(tx*tx + ty*ty);
	 p[0] += v * sin(a+r) * r;
	 p[1] += v * cos(a-r) * r;
      }

      v = vari[7];
      if (v != 0.0) {
	/* heart */
	 double a, r;
	 a = atan2(tx, ty);
	 r = sqrt(tx*tx + ty*ty);
	 a *= r;
	 p[0] += v * sin(a) * r;
	 p[1] += v * cos(a) * -r;
      }

      v = vari[8];
      if (v != 0.0) {
	/* disc */
	 double a, r;
	 double nx, ny;
	 nx = tx * M_PI;
	 ny = ty * M_PI;
	 a = atan2(nx, ny);
	 r = sqrt(nx*nx + ny*ny);
	 p[0] += v * sin(r) * a / M_PI;
	 p[1] += v * cos(r) * a / M_PI;
      }

      v = vari[9];
      if (v != 0.0) {
	/* spiral */
	 double a, r;
	 a = atan2(tx, ty);
	 r = sqrt(tx*tx + ty*ty) + 1e-6;
	 p[0] += v * (cos(a) + sin(r)) / r;
	 p[1] += v * (sin(a) - cos(r)) / r;
      }

      v = vari[10];
      if (v != 0.0) {
	/* hyperbolic */
	 double a, r;
	 a = atan2(tx, ty);
	 r = sqrt(tx*tx + ty*ty) + 1e-6;
	 p[0] += v * sin(a) / r;
	 p[1] += v * cos(a) * r;
      }

      v = vari[11];
      if (v != 0.0) {
	/* diamond */
	 double a, r;
	 a = atan2(tx, ty);
	 r = sqrt(tx*tx + ty*ty);
	 p[0] += v * sin(a) * cos(r);
	 p[1] += v * cos(a) * sin(r);
      }

      v = vari[12];
      if (v != 0.0) {
	/* ex */
	 double a, r;
	 double n0, n1, m0, m1;
	 a = atan2(tx, ty);
	 r = sqrt(tx*tx + ty*ty);
	 n0 = sin(a+r);
	 n1 = cos(a-r);
	 m0 = n0 * n0 * n0 * r;
	 m1 = n1 * n1 * n1 * r;
	 p[0] += v * (m0 + m1);
	 p[1] += v * (m0 - m1);
      }

      v = vari[13];
      if (v != 0.0) {
	 double a, r, nx, ny;
	 /* julia */
	 a = atan2(tx, ty)/2.0;
	 if (flam3_random_bit()) a += M_PI;
	 r = pow(tx*tx + ty*ty, 0.25);
	 nx = r * cos(a);
	 ny = r * sin(a);
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[14];
      if (v != 0.0) {
	 double nx, ny;
	 /* bent */
	 nx = tx;
	 ny = ty;
	 if (nx < 0.0) nx = nx * 2.0;
	 if (ny < 0.0) ny = ny / 2.0;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[15];
      if (v != 0.0) {
	 double nx, ny;
	 double dx, dy;
	 /* waves */
	 dx = coef[2][0];
	 dy = coef[2][1];
	 nx = tx + coef[1][0]*sin(ty/((dx*dx)+EPS));
	 ny = ty + coef[1][1]*sin(tx/((dy*dy)+EPS));
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[16];
      if (v != 0.0) {
	 double nx, ny;
	 double a, r;
	 /* fisheye */
	 r = sqrt(tx*tx + ty*ty);
	 a = atan2(tx, ty);
	 r = 2 * r / (r + 1);
	 nx = r * cos(a);
	 ny = r * sin(a);
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[17];
      if (v != 0.0) {
	 double nx, ny;
	 double dx, dy;
	 /* popcorn */
	 dx = tan(3*ty);
	 dy = tan(3*tx);
	 nx = tx + coef[2][0] * sin(dx);
	 ny = ty + coef[2][1] * sin(dy);
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[18];
      if (v != 0.0) {
	 double nx, ny;
	 double dx, dy;
	 /* exponential */
	 dx = exp(tx) / 2.718281828459045;
	 dy = M_PI * ty;
	 nx = cos(dy) * dx;
	 ny = sin(dy) * dx;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[19];
      if (v != 0.0) {
	 double nx, ny;
	 /* power */
	 double a, r, sa;
	 r = sqrt(tx*tx + ty*ty);
	 a = atan2(tx, ty);
	 sa = sin(a);
	 r = pow(r, sa);
	 nx = r * cos(a);
	 ny = r * sa;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[20];
      if (v != 0.0) {
	  double nx, ny;
	  /* cosine */
	  nx = cos(tx * M_PI) * cosh(ty);
	  ny = -sin(tx * M_PI) * sinh(ty);
	  p[0] += v * nx;
	  p[1] += v * ny;
      }

      v = vari[21];
      if (v != 0.0) {
	  double nx, ny, dx;
	  double a, r;
	  /* rings */
	  dx = coef[2][0];
	  dx = dx * dx + EPS;
	  r = sqrt(tx*tx + ty*ty);
	  r = fmod(r + dx, 2*dx) - dx + r*(1-dx);
	  a = atan2(tx, ty);
	  nx = cos(a) * r;
	  ny = sin(a) * r;
	  p[0] += v * nx;
	  p[1] += v * ny;
      }

      v = vari[22];
      if (v != 0.0) {
	  double nx, ny, dx, dx2, dy;
	  double a, r;
	  /* fan */
	  dx = coef[2][0];
	  dy = coef[2][1];
	  dx = M_PI * (dx * dx + EPS);
	  dx2 = dx/2;
	  r = sqrt(tx*tx + ty*ty);
	  a = atan2(tx, ty);
	  a += (fmod(a+dy, dx) > dx2) ? -dx2 : dx2;
	  nx = cos(a) * r;
	  ny = sin(a) * r;
	  p[0] += v * nx;
	  p[1] += v * ny;
      }
      /* apply the post transform */
      tx = pcoef[0][0] * p[0] + pcoef[1][0] * p[1] + pcoef[2][0];
      ty = pcoef[0][1] * p[0] + pcoef[1][1] * p[1] + pcoef[2][1];
      if (badvalue(tx) || badvalue(ty)) {
	  tx = flam3_random11();
	  ty = flam3_random11();
      }
      p[0] = tx;
      p[1] = ty;

      /* if fuse over, store it */
      if (i >= 0) {
	int i4 = 4*i;
	 samples[i4] = p[0];
	 samples[i4+1] = p[1];
	 samples[i4+2] = p[2];
	 samples[i4+3] = p[3];
      }
   }
}


/* correlation dimension.  after clint sprott. */
double flam3_dimension(flam3_genome *cp, int ntries, int clip_to_camera) {
  double fd;
  double *hist;
  double bmin[2];
  double bmax[2];
  double d2max;
  int i, n1=0, n2=0, got, nclipped;

  if (ntries < 2) ntries = 3000*1000;

  if (clip_to_camera) {
    double scale, ppux, corner0, corner1;
    scale = pow(2.0, cp->zoom);
    ppux = cp->pixels_per_unit * scale;
    corner0 = cp->center[0] - cp->width / ppux / 2.0;
    corner1 = cp->center[1] - cp->height / ppux / 2.0;
    bmin[0] = corner0;
    bmin[1] = corner1;
    bmax[0] = corner0 + cp->width  / ppux;
    bmax[1] = corner1 + cp->height / ppux;
  } else {
    flam3_estimate_bounding_box(cp, 0.0, bmin, bmax);
  }

  d2max =
    (bmax[0] - bmin[0]) * (bmax[0] - bmin[0]) +
    (bmax[1] - bmin[1]) * (bmax[1] - bmin[1]);

  //  fprintf(stderr, "d2max=%g %g %g %g %g\n", d2max,
  //  bmin[0], bmin[1], bmax[0], bmax[1]);

  hist = malloc(2 * ntries * sizeof(double));

  got = 0;
  nclipped = 0;
  while (got < 2*ntries) {
    double subb[4*SUB_BATCH_SIZE];
    int i4, clipped;
    subb[0] = flam3_random11();
    subb[1] = flam3_random11();
    subb[2] = 0.0;
    subb[3] = 0.0;
    flam3_iterate(cp, SUB_BATCH_SIZE, 20, subb);
    i4 = 0;
    for (i = 0; i < SUB_BATCH_SIZE; i++) {
      if (got == 2*ntries) break;
      clipped = clip_to_camera &&
	((subb[i4] < bmin[0]) ||
	 (subb[i4+1] < bmin[1]) ||
	 (subb[i4] > bmax[0]) ||
	 (subb[i4+1] > bmax[1]));
      if (!clipped) {
	hist[got] = subb[i4];
	hist[got+1] = subb[i4+1];
	got += 2;
      } else {
	nclipped++;
	if (nclipped > 10 * ntries) {
	    fprintf(stderr, "warning: too much clipping, "
		    "flam3_dimension giving up.\n");
	    return sqrt(-1.0);
	}
      }
      i4 += 4;
    }
  }
  if (0)
    fprintf(stderr, "cliprate=%g\n", nclipped/(ntries+(double)nclipped));

  for (i = 0; i < ntries; i++) {
    int ri;
    double dx, dy, d2;
    double tx, ty;
    
    tx = hist[2*i];
    ty = hist[2*i+1];
    
    do {
      ri = 2 * (random() % ntries);
    } while (ri == i);

    dx = hist[ri] - tx;
    dy = hist[ri+1] - ty;
    d2 = dx*dx + dy*dy;
    if (d2 < 0.004 * d2max) n2++;
    if (d2 < 0.00004 * d2max) n1++;
  }

  fd = 0.434294 * log(n2 / (n1 - 0.5));

  if (0)
    fprintf(stderr, "n1=%d n2=%d\n", n1, n2);

  free(hist);
  return fd;
}

double flam3_lyapunov(flam3_genome *cp, int ntries) {
  double p[4];
  double x, y;
  double xn, yn;
  double xn2, yn2;
  double dx, dy, r;
  double eps = 1e-5;
  int i;
  double sum = 0.0;

  if (ntries < 1) ntries = 10000;

  for (i = 0; i < ntries; i++) {
    x = flam3_random11();
    y = flam3_random11();

    p[0] = x;
    p[1] = y;
    p[2] = 0.0;
    p[3] = 0.0;

    // get into the attractor
    flam3_iterate(cp, 1, 20+(random()%10), p);

    x = p[0];
    y = p[1];

    // take one deterministic step
    srandom(i);
    flam3_iterate(cp, 1, 0, p);

    xn = p[0];
    yn = p[1];

    do {
      dx = flam3_random11();
      dy = flam3_random11();
      r = sqrt(dx * dx + dy * dy);
    } while (r == 0.0);
    dx /= r;
    dy /= r;

    dx *= eps;
    dy *= eps;

    p[0] = x + dx;
    p[1] = y + dy;
    p[2] = 0.0;

    // take the same step but with eps
    srandom(i);
    flam3_iterate(cp, 1, 0, p);

    xn2 = p[0];
    yn2 = p[1];

    r = sqrt((xn-xn2)*(xn-xn2) + (yn-yn2)*(yn-yn2));

    sum += log(r/eps);
  }
  return sum/(log(2.0)*ntries);
}

/* args must be non-overlapping */
static void mult_matrix(double s1[2][2], double s2[2][2], double d[2][2]) {
   d[0][0] = s1[0][0] * s2[0][0] + s1[1][0] * s2[0][1];
   d[1][0] = s1[0][0] * s2[1][0] + s1[1][0] * s2[1][1];
   d[0][1] = s1[0][1] * s2[0][0] + s1[1][1] * s2[0][1];
   d[1][1] = s1[0][1] * s2[1][0] + s1[1][1] * s2[1][1];
}

/* BY is angle in degrees */
void flam3_rotate(flam3_genome *cp, double by) {
   int i;
   for (i = 0; i < flam3_nxforms; i++) {
      double r[2][2];
      double T[2][2];
      double U[2][2];
      double dtheta = by * 2.0 * M_PI / 360.0;

      /* hmm */
      if (cp->xform[i].symmetry > 0.0) continue;

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

static double det_matrix(double s[2][2]) {
   return s[0][0] * s[1][1] - s[0][1] * s[1][0];
}

static int id_matrix(double s[3][2]) {
  return
    (s[0][0] == 1.0) &&
    (s[0][1] == 0.0) &&
    (s[1][0] == 0.0) &&
    (s[1][1] == 1.0) &&
    (s[2][0] == 0.0) &&
    (s[2][1] == 0.0);
}

/* element-wise linear */
static void interpolate_matrix(double t, double m1[3][2],
			       double m2[3][2], double m3[3][2]) {
   double s = 1.0 - t;

   m3[0][0] = s * m1[0][0] + t * m2[0][0];
   m3[0][1] = s * m1[0][1] + t * m2[0][1];

   m3[1][0] = s * m1[1][0] + t * m2[1][0];
   m3[1][1] = s * m1[1][1] + t * m2[1][1];

   m3[2][0] = s * m1[2][0] + t * m2[2][0];
   m3[2][1] = s * m1[2][1] + t * m2[2][1];
}

static void interpolate_cmap(double cmap[256][3], double blend,
			     int index0, double hue0, int index1, double hue1) {
  double p0[256][3];
  double p1[256][3];
  int i, j;

  flam3_get_palette(index0, p0, hue0);
  flam3_get_palette(index1, p1, hue1);
  
  for (i = 0; i < 256; i++) {
    double t[3], s[3];
    rgb2hsv(p0[i], s);
    rgb2hsv(p1[i], t);
    for (j = 0; j < 3; j++)
      t[j] = ((1.0-blend) * s[j]) + (blend * t[j]);
    hsv2rgb(t, cmap[i]);
  }
}

#define INTERP(x)  result->x = c0 * cps[i1].x + c1 * cps[i2].x
#define INTERI(x)  \
  result->x = (int)floor(0.5 + c0 * cps[i1].x + c1 * cps[i2].x)

/*
 * create a control point that interpolates between the control points
 * passed in CPS.  for now just do linear.  in the future, add control
 * point types and other things to the cps.  CPS must be sorted by time.
 */
void flam3_interpolate(flam3_genome cps[], int ncps,
		       double time, flam3_genome *result) {
   int i, j, i1, i2;
   double c0, c1;

   if (1 == ncps) {
      *result = cps[0];
      return;
   }
   if (cps[0].time >= time) {
      i1 = 0;
      i2 = 1;
   } else if (cps[ncps - 1].time <= time) {
      i1 = ncps - 2;
      i2 = ncps - 1;
   } else {
      i1 = 0;
      while (cps[i1].time < time)
	 i1++;
      i1--;
      i2 = i1 + 1;
      if (time - cps[i1].time > -1e-7 &&
	  time - cps[i1].time < 1e-7) {
	 *result = cps[i1];
	 return;
      }
   }

   c0 = (cps[i2].time - time) / (cps[i2].time - cps[i1].time);
   c1 = 1.0 - c0;

   result->time = time;

   for (i = 0; i < 256; i++) {
     double t[3], s[3];
     rgb2hsv(cps[i1].palette[i], s);
     rgb2hsv(cps[i2].palette[i], t);
     for (j = 0; j < 3; j++)
       t[j] = c0 * s[j] + c1 * t[j];
     hsv2rgb(t, result->palette[i]);
   }

   result->palette_index = flam3_palette_random;
   result->symmetry = 0;
   INTERP(brightness);
   INTERP(contrast);
   INTERP(gamma);
   INTERP(vibrancy);
   INTERP(hue_rotation);
   INTERI(width);
   INTERI(height);
   INTERI(spatial_oversample);
   INTERP(center[0]);
   INTERP(center[1]);
   result->rot_center[0] = result->center[0];
   result->rot_center[1] = result->center[1];
   INTERP(background[0]);
   INTERP(background[1]);
   INTERP(background[2]);
   INTERP(pixels_per_unit);
   INTERP(spatial_filter_radius);
   INTERP(sample_density);
   INTERP(zoom);
   INTERP(rotate);
   INTERI(nbatches);

   for (i = 0; i < flam3_nxforms; i++) {
      INTERP(xform[i].density);
      INTERP(xform[i].color[0]);
      INTERP(xform[i].color[1]);
      INTERP(xform[i].symmetry);
      for (j = 0; j < flam3_nvariations; j++)
	 INTERP(xform[i].var[j]);
      interpolate_matrix(c1, cps[i1].xform[i].c, cps[i2].xform[i].c,
			 result->xform[i].c);
      interpolate_matrix(c1, cps[i1].xform[i].post,
			 cps[i2].xform[i].post, result->xform[i].post);
   }
}

static int compare_xforms(const void *av, const void *bv) {
    flam3_xform *a = (flam3_xform *) av;
    flam3_xform *b = (flam3_xform *) bv;
   double aa[2][2];
   double bb[2][2];
   double ad, bd;

   aa[0][0] = a->c[0][0];
   aa[0][1] = a->c[0][1];
   aa[1][0] = a->c[1][0];
   aa[1][1] = a->c[1][1];
   bb[0][0] = b->c[0][0];
   bb[0][1] = b->c[0][1];
   bb[1][0] = b->c[1][0];
   bb[1][1] = b->c[1][1];
   ad = det_matrix(aa);
   bd = det_matrix(bb);

   if (a->symmetry < b->symmetry) return 1;
   if (a->symmetry > b->symmetry) return -1;
   if (a->symmetry) {
     if (ad < 0) return -1;
     if (bd < 0) return 1;
     ad = atan2(a->c[0][0], a->c[0][1]);
     bd = atan2(b->c[0][0], b->c[0][1]);
   }

   if (ad < bd) return -1;
   if (ad > bd) return 1;
   return 0;
}



static flam3_genome xml_current_cp;
static flam3_genome *xml_all_cp;
static int xml_all_ncps;
static int xml_current_xform;

static void clear_current_cp() {
    int i, j;
    flam3_genome *cp = &xml_current_cp;

    cp->palette_index = flam3_palette_random;
    cp->background[0] = 0.0;
    cp->background[1] = 0.0;
    cp->background[2] = 0.0;
    cp->center[0] = 0.0;
    cp->center[1] = 0.0;
    cp->rot_center[0] = 0.0;
    cp->rot_center[1] = 0.0;
    cp->pixels_per_unit = 50;
    cp->width = 100;
    cp->height = 100;
    cp->spatial_oversample = 1;
    cp->gamma = 4.0;
    cp->vibrancy = 1.0;
    cp->contrast = 1.0;
    cp->brightness = 1.0;
    cp->spatial_filter_radius = 0.5;
    cp->zoom = 0.0;
    cp->sample_density = 1;
    cp->nbatches = 1;
    cp->symmetry = 0;
    cp->hue_rotation = 0.0;
    cp->rotate = 0.0;
    
    for (i = 0; i < flam3_nxforms; i++) {
	cp->xform[i].density = 0.0;
	cp->xform[i].symmetry = 0;
	cp->xform[i].color[0] = i&1;
	cp->xform[i].color[1] = (i&2)>>1;
	cp->xform[i].var[0] = 1.0;
	for (j = 1; j < flam3_nvariations; j++)
	    cp->xform[i].var[j] = 0.0;
	cp->xform[i].c[0][0] = 1.0;
	cp->xform[i].c[0][1] = 0.0;
	cp->xform[i].c[1][0] = 0.0;
	cp->xform[i].c[1][1] = 1.0;
	cp->xform[i].c[2][0] = 0.0;
	cp->xform[i].c[2][1] = 0.0;
	cp->xform[i].post[0][0] = 1.0;
	cp->xform[i].post[0][1] = 0.0;
	cp->xform[i].post[1][0] = 0.0;
	cp->xform[i].post[1][1] = 1.0;
	cp->xform[i].post[2][0] = 0.0;
	cp->xform[i].post[2][1] = 0.0;
    }
}

char *flam3_variation_names[1+flam3_nvariations] = {
  "linear",
  "sinusoidal",
  "spherical",
  "swirl",
  "horseshoe",
  "polar",
  "handkerchief",
  "heart",
  "disc",
  "spiral",
  "hyperbolic",
  "diamond",
  "ex",
  "julia",
  "bent",
  "waves",
  "fisheye",
  "popcorn",
  "exponential",
  "power",
  "cosine",
  "rings",
  "fan",
  0
};


static int var2n(const char *s) {
  int i;
  for (i = 0; i < flam3_nvariations; i++)
    if (!strcmp(s, flam3_variation_names[i])) return i;
  return flam3_variation_none;
}

static void start_element(void *userData, const char *name, const char **atts) {
    flam3_genome *cp = &xml_current_cp;
    int i = 0, j;
    if (!strcmp(name, "flame")) {
	xml_current_xform = 0;
	while (atts[i]) {
	    const char *a = atts[i+1];
	    if (!strcmp(atts[i], "time")) {
		cp->time = atof(a);
	    } else if (!strcmp(atts[i], "palette")) {
		cp->palette_index = atoi(a);
	    } else if (!strcmp(atts[i], "size")) {
		sscanf(a, "%d %d", &cp->width, &cp->height);
	    } else if (!strcmp(atts[i], "center")) {
		sscanf(a, "%lf %lf", &cp->center[0], &cp->center[1]);
		cp->rot_center[0] = cp->center[0];
		cp->rot_center[1] = cp->center[1];
	    } else if (!strcmp(atts[i], "scale")) {
		cp->pixels_per_unit = atof(a);
	    } else if (!strcmp(atts[i], "rotate")) {
		cp->rotate = atof(a);
	    } else if (!strcmp(atts[i], "zoom")) {
		cp->zoom = atof(a);
	    } else if (!strcmp(atts[i], "oversample")) {
		cp->spatial_oversample = atoi(a);
	    } else if (!strcmp(atts[i], "filter")) {
		cp->spatial_filter_radius = atof(a);
	    } else if (!strcmp(atts[i], "quality")) {
		cp->sample_density = atof(a);
	    } else if (!strcmp(atts[i], "batches")) {
		cp->nbatches = atoi(a);
	    } else if (!strcmp(atts[i], "background")) {
		sscanf(a, "%lf %lf %lf", &cp->background[0],
		       &cp->background[1], &cp->background[2]);
	    } else if (!strcmp(atts[i], "brightness")) {
		cp->brightness = atof(a);
	    } else if (!strcmp(atts[i], "gamma")) {
		cp->gamma = atof(a);
	    } else if (!strcmp(atts[i], "vibrancy")) {
		cp->vibrancy = atof(a);
	    } else if (!strcmp(atts[i], "hue")) {
		cp->hue_rotation = fmod(atof(a), 1.0);
	    }
	    i += 2;
	}
    } else if (!strcmp(name, "color")) {
      int index = -1;
      double r, g, b;
      r = g = b = 0.0;
      while (atts[i]) {
	const char *a = atts[i+1];
	if (!strcmp(atts[i], "index")) {
	  index = atoi(a);
	} else if (!strcmp(atts[i], "rgb")) {
	  sscanf(a, "%lf %lf %lf", &r, &g, &b);
	}
	i += 2;
      }
      if (index >= 0 && index < 256) {
	cp->palette[index][0] = r/255.0;
	cp->palette[index][1] = g/255.0;
	cp->palette[index][2] = b/255.0;
      } else {
	fprintf(stderr,
		"color element with missing or bad index attribute.\n");
      }
    } else if (!strcmp(name, "palette")) {
      int index0, index1;
      double hue0, hue1;
      double blend = 0.5;
      index0 = index1 = flam3_palette_random;
      hue0 = hue1 = 0.0;
      while (atts[i]) {
	const char *a = atts[i+1];
	if (!strcmp(atts[i], "index0")) {
	  index0 = atoi(a);
	} else if (!strcmp(atts[i], "index1")) {
	  index1 = atoi(a);
	} else if (!strcmp(atts[i], "hue0")) {
	  hue0 = atof(a);
	} else if (!strcmp(atts[i], "hue1")) {
	  hue1 = atof(a);
	} else if (!strcmp(atts[i], "blend")) {
	  blend = atof(a);
	}
	i += 2;
      }
      interpolate_cmap(cp->palette, blend, index0, hue0, index1, hue1);
    } else if (!strcmp(name, "symmetry")) {
      int kind = 0;
      while (atts[i]) {
	const char *a = atts[i+1];
	if (!strcmp(atts[i], "kind")) {
	  kind = atoi(a);
	}
	i += 2;
      }
      xml_current_xform += flam3_add_symmetry(cp, kind);
    } else if (!strcmp(name, "xform")) {
	int xf = xml_current_xform;
	if (xf < 0) {
	    fprintf(stderr, "<xform> not inside <flame>.\n");
	    exit(1);
	}
	if (xml_current_xform == flam3_nxforms) {
	    if (0)
		fprintf(stderr, "too many xforms, dropping on the floor.\n");
	    xml_current_xform--;
	    xf--;
	}
	for (j = 0; j < flam3_nvariations; j++)
	  cp->xform[xf].var[j] = 0.0;
	while (atts[i]) {
	    char *a = (char *) atts[i+1];
	    if (!strcmp(atts[i], "weight")) {
		cp->xform[xf].density = atof(a);
	    } else if (!strcmp(atts[i], "symmetry")) {
		cp->xform[xf].symmetry = atof(a);
	    } else if (!strcmp(atts[i], "color")) {
	      cp->xform[xf].color[1] = 0.0;
	      sscanf(a, "%lf %lf", &cp->xform[xf].color[0], &cp->xform[xf].color[1]);
	      sscanf(a, "%lf", &cp->xform[xf].color[0]);
	    } else if (!strcmp(atts[i], "var1")) {
	      int k;
	      for (k = 0; k < flam3_nvariations; k++) {
		cp->xform[xf].var[k] = 0.0;
	      }
	      k = atoi(a);
	      if (k < 0 || k >= flam3_nvariations) {
		fprintf(stderr, "bad variation: %d.\n", k);
		k = 0;
	      }
	      cp->xform[xf].var[k] = 1.0;
	    } else if (!strcmp(atts[i], "var")) {
		int k;
		for (k = 0; k < flam3_nvariations; k++) {
		    cp->xform[xf].var[k] = strtod(a, &a);
		}
	    } else if (!strcmp(atts[i], "coefs")) {
		int k, j;
		for (k = 0; k < 3; k++) {
		    for (j = 0; j < 2; j++) {
			cp->xform[xf].c[k][j] = strtod(a, &a);
		    }
		}
	    } else if (!strcmp(atts[i], "post")) {
		int k, j;
		for (k = 0; k < 3; k++) {
		    for (j = 0; j < 2; j++) {
			cp->xform[xf].post[k][j] = strtod(a, &a);
		    }
		}
	    } else {
	      int v = var2n(atts[i]);
	      if (v != flam3_variation_none)
		cp->xform[xf].var[v] = atof(a);
	    }
	    i += 2;
	}
    }
}

static void end_element(void *userData, const char *name) {
    if (!strcmp("flame", name)) {
	size_t s = (1+xml_all_ncps)*sizeof(flam3_genome);
	xml_current_xform = -1;
	xml_all_cp = realloc(xml_all_cp, s);
	if (xml_current_cp.palette_index != flam3_palette_random)
	  flam3_get_palette(xml_current_cp.palette_index,
			    xml_current_cp.palette, 
			    xml_current_cp.hue_rotation);
	xml_all_cp[xml_all_ncps++] = xml_current_cp;
	clear_current_cp();
    } else if (!strcmp("xform", name)) {
	xml_current_xform++;
    }
}

flam3_genome *flam3_parse(char *s, int *ncps) {
    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, start_element, end_element);
    xml_current_xform = -1;
    xml_all_cp = NULL;
    xml_all_ncps = 0;
    clear_current_cp();
    if (!XML_Parse(parser, s, strlen(s), 1)) {
      fprintf(stderr,
	      "%s at line %d\n",
	      XML_ErrorString(XML_GetErrorCode(parser)),
	      XML_GetCurrentLineNumber(parser));
      return NULL;
    }
    XML_ParserFree(parser);
    *ncps = xml_all_ncps;
    return xml_all_cp;
}

flam3_genome * flam3_parse_from_file(FILE *f, int *ncps) {
  int i, c, slen = 5000;
  char *s;
  
  s = malloc(slen);
  i = 0;
  do {
    c = getc(f);
    if (EOF == c) goto done_reading;
    s[i++] = c;
    if (i == slen-1) {
      slen *= 2;
      s = realloc(s, slen);
    }
  } while (1);
 done_reading:
  s[i] = 0;

  return flam3_parse(s, ncps);
}

void flam3_print(FILE *f, flam3_genome *cp, char *extra_attributes) {
    int i, j;
    char *p = "";
    fprintf(f, "%s<flame time=\"%g\"", p, cp->time);
    if (0 <= cp->palette_index)
	fprintf(f, " palette=\"%d\"", cp->palette_index);
    fprintf(f, " size=\"%d %d\"", cp->width, cp->height);
    if (cp->center[0] != 0.0 ||
	cp->center[1] != 0.0)
      fprintf(f, " center=\"%g %g\"", cp->center[0], cp->center[1]);
    fprintf(f, " scale=\"%g\"", cp->pixels_per_unit);
    if (cp->zoom != 0.0)
      fprintf(f, " zoom=\"%g\"", cp->zoom);
    if (cp->rotate != 0.0)
      fprintf(f, " rotate=\"%g\"", cp->rotate);
    if (cp->spatial_oversample != 1)
      fprintf(f, " oversample=\"%d\"", cp->spatial_oversample);
    fprintf(f, " filter=\"%g\"", cp->spatial_filter_radius);
    fprintf(f, " quality=\"%g\"", cp->sample_density);
    if (1 != cp->nbatches)
      fprintf(f, " batches=\"%d\"", cp->nbatches);
    if (cp->background[0] != 0.0 ||
	cp->background[1] != 0.0 ||
	cp->background[2] != 0.0)
      fprintf(f, " background=\"%g %g %g\"",
	      cp->background[0], cp->background[1], cp->background[2]);
    fprintf(f, " brightness=\"%g\"", cp->brightness);
    fprintf(f, " gamma=\"%g\"", cp->gamma);
    if (1.0 != cp->vibrancy)
      fprintf(f, " vibrancy=\"%g\"", cp->vibrancy);
    if (0 <= cp->palette_index &&
	cp->hue_rotation != 0.0)
      fprintf(f, " hue=\"%g\"", cp->hue_rotation);
    if (extra_attributes)
	fprintf(f, " %s", extra_attributes);
    fprintf(f, ">\n");
    if (flam3_palette_interpolated == cp->palette_index) {
      fprintf(f, "%s   <palette blend=\"%g\" index0=\"%d\" hue0=\"%g\" ",
	      p, cp->palette_blend, cp->palette_index0, cp->hue_rotation0);
      fprintf(f, "index1=\"%d\" hue1=\"%g\"/>\n",
	      cp->palette_index1, cp->hue_rotation1);
    } else if (flam3_palette_random == cp->palette_index) {
      for (i = 0; i < 256; i++) {
	int r, g, b;
	r = (int) (cp->palette[i][0] * 255.0);
	g = (int) (cp->palette[i][1] * 255.0);
	b = (int) (cp->palette[i][2] * 255.0);
	printf("%s   <color index=\"%d\" rgb=\"%d %d %d\"/>\n",
	       p, i, r, g, b);
      }
    }
    for (i = 0; i < flam3_nxforms; i++)
      if (cp->xform[i].density > 0.0
	  && !(cp->symmetry &&  cp->xform[i].symmetry == 1.0)) {
	int nones = 0;
	int nzeroes = 0;
	int lastnonzero = 0;
	fprintf(f, "%s   <xform weight=\"%g\" color=\"%g",
		p, cp->xform[i].density, cp->xform[i].color[0]);
	if (0.0 != cp->xform[i].color[1]) {
	    fprintf(f, " %g\" ", cp->xform[i].color[1]);
	} else {
	    fprintf(f, "\" ");
	}
	if (cp->xform[i].symmetry != 0.0) {
	  fprintf(f, "symmetry=\"%g\" ", cp->xform[i].symmetry);
	}
#if 1
	for (j = 0; j < flam3_nvariations; j++) {
	  double v = cp->xform[i].var[j];
	  if (0.0 != v) {
	    fprintf(f, "%s=\"%g\" ", flam3_variation_names[j], v);
	  }
	}
#else
	for (j = 0; j < flam3_nvariations; j++) {
	  double v = cp->xform[i].var[j];
	  if (1.0 == v) {
	    nones++;
	  } else if (0.0 == v) {
	    nzeroes++;
	  }
	  if (0.0 != v)
	    lastnonzero = j;
	}
	if (1 == nones &&
	    flam3_nvariations-1 == nzeroes) {
	  fprintf(f, "var1=\"%d\"", lastnonzero);
	} else {
	  fprintf(f, "var=\"");
	  for (j = 0; j <= lastnonzero; j++) {
	    if (j) fprintf(f, " ");
	    fprintf(f, "%g", cp->xform[i].var[j]);
	  }
	  fprintf(f, "\"");
	}
#endif
	fprintf(f, "coefs=\"");
	for (j = 0; j < 3; j++) {
	    if (j) fprintf(f, " ");
	    fprintf(f, "%g %g", cp->xform[i].c[j][0], cp->xform[i].c[j][1]);
	}
	fprintf(f, "\"");
	if (!id_matrix(cp->xform[i].post)) {
	  fprintf(f, " post=\"");
	  for (j = 0; j < 3; j++) {
	    if (j) fprintf(f, " ");
	    fprintf(f, "%g %g", cp->xform[i].post[j][0], cp->xform[i].post[j][1]);
	  }
	  fprintf(f, "\"");
	}
	fprintf(f, "/>\n");

    }
    if (cp->symmetry)
      fprintf(f, "%s   <symmetry kind=\"%d\"/>\n", p, cp->symmetry);
    fprintf(f, "%s</flame>\n", p);
}

/* returns a uniform variable from 0 to 1 */
double flam3_random01() {
   return (random() & 0xfffffff) / (double) 0xfffffff;
}

double flam3_random11() {
   return ((random() & 0xfffffff) - 0x7ffffff) / (double) 0x7ffffff;
}

int flam3_random_bit() {
  static int n = 0;
  static int l;
  if (0 == n) {
    l = random();
    n = 20;
  } else {
    l = l >> 1;
    n--;
  }
  return l & 1;
}


/* sum of entries of vector to 1 */
static int normalize_vector(double *v, int n) {
    double t = 0.0;
    int i;
    for (i = 0; i < n; i++)
	t += v[i];
    if (0.0 == t) return 1;
    t = 1.0 / t;
    for (i = 0; i < n; i++)
	v[i] *= t;
    return 0;
}



static double round6(double x) {
  x *= 1e6;
  if (x < 0) x -= 1.0;
  return 1e-6*(int)(x+0.5);
}

/* sym=2 or more means rotational
   sym=1 means identity, ie no symmetry
   sym=0 means pick a random symmetry (maybe none)
   sym=-1 means bilateral (reflection)
   sym=-2 or less means rotational and reflective
*/
int flam3_add_symmetry(flam3_genome *cp, int sym) {
  int i, j, k;
  double a;
  int result = 0;

  if (0 == sym) {
    static int sym_distrib[] = {
      -4, -3,
      -2, -2, -2,
      -1, -1, -1,
      2, 2, 2,
      3, 3,
      4, 4,
    };
    if (random()&1) {
      sym = random_distrib(sym_distrib);
    } else if (random()&31) {
      sym = (random()%13)-6;
    } else {
      sym = (random()%51)-25;
    }
  }

  if (1 == sym || 0 == sym) return 0;

  for (i = 0; i < flam3_nxforms; i++)
    if (cp->xform[i].density == 0.0)
      break;

  if (i == flam3_nxforms) return 0;

  cp->symmetry = sym;

  if (sym < 0) {
    cp->xform[i].density = 1.0;
    cp->xform[i].symmetry = 1.0;
    cp->xform[i].var[0] = 1.0;
    for (j = 1; j < flam3_nvariations; j++)
      cp->xform[i].var[j] = 0;
    cp->xform[i].color[0] = 1.0;
    cp->xform[i].color[1] = 1.0;
    cp->xform[i].c[0][0] = -1.0;
    cp->xform[i].c[0][1] = 0.0;
    cp->xform[i].c[1][0] = 0.0;
    cp->xform[i].c[1][1] = 1.0;
    cp->xform[i].c[2][0] = 0.0;
    cp->xform[i].c[2][1] = 0.0;

    i++;
    result++;
    sym = -sym;
  }

  a = 2*M_PI/sym;

  for (k = 1; (k < sym) && (i < flam3_nxforms); k++) {
    cp->xform[i].density = 1.0;
    cp->xform[i].var[0] = 1.0;
    cp->xform[i].symmetry = 1.0;
    for (j = 1; j < flam3_nvariations; j++)
      cp->xform[i].var[j] = 0;
    cp->xform[i].color[1] = /* XXX */
    cp->xform[i].color[0] = (sym<3) ? 0.0 : ((k-1.0)/(sym-2.0));
    cp->xform[i].c[0][0] = round6(cos(k*a));
    cp->xform[i].c[0][1] = round6(sin(k*a));
    cp->xform[i].c[1][0] = round6(-cp->xform[i].c[0][1]);
    cp->xform[i].c[1][1] = cp->xform[i].c[0][0];
    cp->xform[i].c[2][0] = 0.0;
    cp->xform[i].c[2][1] = 0.0;

    i++;
    result++;
  }

  qsort((char *) &cp->xform[i-result], result,
	sizeof(flam3_xform), compare_xforms);

  return result;
}

static int random_var() {
  return random() % flam3_nvariations;
}

void flam3_random(flam3_genome *cp, int ivar, int sym) {
   int i, nxforms, var, samed, multid, samepost, postid;
   static int xform_distrib[] = {
     2, 2, 2, 2,
     3, 3, 3, 3,
     4, 4, 4,
     5, 5,
     6
   };

   cp->hue_rotation = (random()&7) ? 0.0 : flam3_random01();
   cp->palette_index =
       flam3_get_palette(flam3_palette_random, cp->palette, cp->hue_rotation);
   cp->time = 0.0;
   nxforms = random_distrib(xform_distrib);
   if (flam3_variation_random == ivar) {
     if (flam3_random_bit()) {
       var = random_var();
     } else {
       var = flam3_variation_random;
     }
   } else {
     var = ivar;
   }

   samed = flam3_random_bit();
   multid = flam3_random_bit();
   postid = flam3_random01() < 0.6;
   samepost = flam3_random_bit();
   for (i = 0; i < nxforms; i++) {
      int j, k;
      cp->xform[i].density = 1.0 / nxforms;
      cp->xform[i].color[0] = i&1;
      cp->xform[i].color[1] = (i&2)>>1;
      cp->xform[i].symmetry = 0.0;
      for (j = 0; j < 3; j++)
	  for (k = 0; k < 2; k++) {
	      cp->xform[i].c[j][k] = flam3_random11();
	      cp->xform[i].post[j][k] = (double)(k==j);
	  }
      if (!postid) {
	  for (j = 0; j < 3; j++)
	      for (k = 0; k < 2; k++) {
		  if (samepost || (i==0))
		      cp->xform[i].post[j][k] = flam3_random11();
		  else
		      cp->xform[i].post[j][k] = cp->xform[0].post[j][k];
	      }
      }

      for (j = 0; j < flam3_nvariations; j++)
	 cp->xform[i].var[j] = 0.0;
      if (flam3_variation_random != var)
	 cp->xform[i].var[var] = 1.0;
      else if (multid) {
	 cp->xform[i].var[random_var()] = 1.0;
      } else {
	int n;
	double sum;
	if (samed && i > 0) {
	  for (j = 0; j < flam3_nvariations; j++)
	    cp->xform[i].var[j] = cp->xform[i-1].var[j];
	} else {
	  n = 2;
	  while ((flam3_random_bit()) && (n<flam3_nvariations))
	    n++;
	  for (j = 0; j < n; j++)
	    cp->xform[i].var[random_var()] = flam3_random01();
	  sum = 0.0;
	  for (j = 0; j < flam3_nvariations; j++)
	    sum += cp->xform[i].var[j];
	  if (sum == 0.0)
	    cp->xform[i].var[random_var()] = 1.0;
	  else {
	    for (j = 0; j < flam3_nvariations; j++)
	      cp->xform[i].var[j] /= sum;
	  }
	}
      }
   }
   for (; i < flam3_nxforms; i++)
      cp->xform[i].density = 0.0;

   if (sym || !(random()%4)) {
     flam3_add_symmetry(cp, sym);
   } else
     cp->symmetry = 0;

   qsort((char *) cp->xform, flam3_nxforms,
	 sizeof(flam3_xform), compare_xforms);
}


/*
 * find a 2d bounding box that does not enclose eps of the fractal density
 * in each compass direction.  works by binary search.
 * this is stupid, it shouldjust use the find nth smallest algorithm.
 */
void flam3_estimate_bounding_box(flam3_genome *cp, double eps,
				 double *bmin, double *bmax) {
   int i, j, batch = (int)((eps == 0.0) ? 10000 : 10.0/eps);
   int low_target = (int)(batch * eps);
   int high_target = batch - low_target;
   double min[3], max[3], delta[3];
   double *points;

   points = (double *)  malloc(sizeof(double) * 4 * batch);
   points[0] = flam3_random11();
   points[1] = flam3_random11();
   points[2] = 0.0;
   points[3] = 0.0;

   flam3_iterate(cp, batch, 20, points);

   min[0] = min[1] =  1e10;
   max[0] = max[1] = -1e10;
   
   for (i = 0; i < batch; i++) {
      double *p = &points[3*i];
      if (p[0] < min[0]) min[0] = p[0];
      if (p[1] < min[1]) min[1] = p[1];
      if (p[0] > max[0]) max[0] = p[0];
      if (p[1] > max[1]) max[1] = p[1];
   }

   if (low_target == 0) {
      bmin[0] = min[0];
      bmin[1] = min[1];
      bmax[0] = max[0];
      bmax[1] = max[1];
      return;
   }
   
   delta[0] = (max[0] - min[0]) * 0.25;
   delta[1] = (max[1] - min[1]) * 0.25;

   bmax[0] = bmin[0] = min[0] + 2.0 * delta[0];
   bmax[1] = bmin[1] = min[1] + 2.0 * delta[1];

   for (i = 0; i < 14; i++) {
      int n, s, e, w;
      n = s = e = w = 0;
      for (j = 0; j < batch; j++) {
	 double *p = &points[3*j];
	 if (p[0] < bmin[0]) n++;
	 if (p[0] > bmax[0]) s++;
	 if (p[1] < bmin[1]) w++;
	 if (p[1] > bmax[1]) e++;
      }
      bmin[0] += (n <  low_target) ? delta[0] : -delta[0];
      bmax[0] += (s < high_target) ? delta[0] : -delta[0];
      bmin[1] += (w <  low_target) ? delta[1] : -delta[1];
      bmax[1] += (e < high_target) ? delta[1] : -delta[1];
      delta[0] = delta[0] / 2.0;
      delta[1] = delta[1] / 2.0;
      /*
      fprintf(stderr, "%g %g %g %g\n", bmin[0], bmin[1], bmax[0], bmax[1]);
      */
   }
   /*
   fprintf(stderr, "%g %g %g %g\n", min[0], min[1], max[0], max[1]);
   */
}





typedef double bucket_double[4];
typedef double abucket_double[4];
typedef int bucket_int[4];
typedef int abucket_int[4];
typedef short bucket_short[4];
typedef short abucket_short[4];




#define ACCUM_T double
#define bucket bucket_double
#define abucket abucket_double
#define MAXBUCKET (1<<14)
#define bump_no_overflow(dest, delta, type) {dest += delta;}
#define render_rectangle render_rectangle_double
#include "rect.c"
#undef render_rectangle
#undef ACCUM_T
#undef bucket
#undef abucket
#undef MAXBUCKET
#undef bump_no_overflow

#define ACCUM_T int
#define bucket bucket_int
#define abucket abucket_int
#define MAXBUCKET (1<<30)
#define bump_no_overflow(dest, delta, type) { \
   type tt_ = (type) (dest + delta + 0.5);           \
   if (tt_ > dest) dest = tt_;                  \
}
#define render_rectangle render_rectangle_int
#include "rect.c"
#undef render_rectangle
#undef ACCUM_T
#undef bucket
#undef abucket
#undef MAXBUCKET
#undef bump_no_overflow

#define ACCUM_T short
#define bucket bucket_short
#define abucket abucket_short
#define MAXBUCKET (1<<14)
#define bump_no_overflow(dest, delta, type) { \
   type tt_ = (type) (dest + delta + 0.5);           \
   if (tt_ > dest) dest = tt_;                  \
}
#define render_rectangle render_rectangle_short
#include "rect.c"
#undef render_rectangle
#undef ACCUM_T
#undef bucket
#undef abucket
#undef MAXBUCKET
#undef bump_no_overflow

double flam3_render_memory_required(flam3_frame *spec)
{
  flam3_genome *cps = spec->genomes;

  /* note 4 channels * 2 buffers cancels out 8 bits per byte */

  return
    (double) cps[0].spatial_oversample * cps[0].spatial_oversample *
    (double) cps[0].width * cps[0].height * spec->bits;
}

void flam3_render(flam3_frame *spec, unsigned char *out,
		  int out_width, int field, int nchan) {
  switch (spec->bits) {
  case 16:
    render_rectangle_short(spec, out, out_width, field, nchan);
    break;
  case 32:
    render_rectangle_int(spec, out, out_width, field, nchan);
    break;
  case 64:
    render_rectangle_double(spec, out, out_width, field, nchan);
    break;
  default:
    fprintf(stderr, "bad bits, must be 16, 32, or 64 not %d.\n", spec->bits);
    exit(1);
    break;
  }
}
