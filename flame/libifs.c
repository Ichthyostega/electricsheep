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
"@(#) $Id: libifs.c,v 1.30 2004/03/29 01:02:01 spotspot Exp $";

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>
#include "libifs.h"

#ifdef WIN32
#define M_PI 3.1415926539
#define random()  (rand() ^ (rand()<<15))
#define srandom(x)  (srand(x))
#endif

void sort_control_points(control_point *cps, int ncps, double (*metric)());
double standard_metric(control_point *cp1, control_point *cp2);
double random_uniform01();
double random_uniform11();
double random_gaussian();
void mult_matrix(double s1[2][2], double s2[2][2], double d[2][2]);


#define CHOOSE_XFORM_GRAIN 10000

#define random_distrib(v) ((v)[random()%vlen(v)])


/*
 * run the function system described by CP forward N generations.
 * store the n resulting 3 vectors in POINTS.  the initial point is passed
 * in POINTS[0].  ignore the first FUSE iterations.
 */

void iterate(control_point *cp, int n, int fuse, point *points) {
   int i, j, count_large = 0, count_nan = 0;
   char xform_distrib[CHOOSE_XFORM_GRAIN];
   double p[3], t, r, dr;
   p[0] = points[0][0];
   p[1] = points[0][1];
   p[2] = points[0][2];

   /*
    * first, set up xform, which is an array that converts a uniform random
    * variable into one with the distribution dictated by the density
    * fields 
    */
   dr = 0.0;
   for (i = 0; i < NXFORMS; i++) {
      dr += cp->xform[i].density;
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

      if (p[0] > 100.0 || p[0] < -100.0 ||
	  p[1] > 100.0 || p[1] < -100.0)
	 count_large++;
      if (p[0] != p[0])
	 count_nan++;

#define coef   cp->xform[fn].c
#define vari   cp->xform[fn].var

      /* first compute the color coord */
      {
	double s = cp->xform[fn].symmetry;
	p[2] = (p[2] + cp->xform[fn].color) * 0.5 * (1.0 - s) + s * p[2];
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
	 double c1 = sin(r2);
	 double c2 = cos(r2);
	 double nx = c1 * tx - c2 * ty;
	 double ny = c2 * tx + c1 * ty;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }
      
      v = vari[4];
      if (v != 0.0) {
	 /* horseshoe */
	 double a, c1, c2, nx, ny;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);  /* times k here is fun */
	 else
	    a = 0.0;
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
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    nx = atan2(tx, ty) / M_PI;
	 else
	    nx = 0.0;

	 ny = sqrt(tx * tx + ty * ty) - 1.0;
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      v = vari[6];
      if (v != 0.0) {
	/* folded handkerchief */
	 double a, r;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);
	 else
	    a = 0.0;
	 r = sqrt(tx*tx + ty*ty);
	 p[0] += v * sin(a+r) * r;
	 p[1] += v * cos(a-r) * r;
      }

      v = vari[7];
      if (v != 0.0) {
	/* heart */
	 double a, r;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);
	 else
	    a = 0.0;
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
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(nx, ny);
	 else
	    a = 0.0;
	 r = sqrt(nx*nx + ny*ny);
	 p[0] += v * sin(r) * a / M_PI;
	 p[1] += v * cos(r) * a / M_PI;
      }

      v = vari[9];
      if (v != 0.0) {
	/* spiral */
	 double a, r;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);
	 else
	    a = 0.0;
	 r = sqrt(tx*tx + ty*ty) + 1e-6;
	 p[0] += v * (cos(a) + sin(r)) / r;
	 p[1] += v * (sin(a) - cos(r)) / r;
      }

      v = vari[10];
      if (v != 0.0) {
	/* hyperbolic */
	 double a, r;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);
	 else
	    a = 0.0;
	 r = sqrt(tx*tx + ty*ty) + 1e-6;
	 p[0] += v * sin(a) / r;
	 p[1] += v * cos(a) * r;
      }

      v = vari[11];
      if (v != 0.0) {
	/* diamond */
	 double a, r;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);
	 else
	    a = 0.0;
	 r = sqrt(tx*tx + ty*ty);
	 p[0] += v * sin(a) * cos(r);
	 p[1] += v * cos(a) * sin(r);
      }

      v = vari[12];
      if (v != 0.0) {
	/* ex */
	 double a, r;
	 double n0, n1, m0, m1;
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty);
	 else
	    a = 0.0;
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
	 if (tx < -EPS || tx > EPS ||
	     ty < -EPS || ty > EPS)
	    a = atan2(tx, ty)/2.0;
	 else
	    a = 0.0;
	 if (random()&1) a += M_PI;
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
	 if (dx != dx) dx = 0.0;
	 dy = tan(3*tx);
	 if (dy != dy) dy = 0.0;
	 nx = tx + coef[2][0] * sin(dx);
	 ny = ty + coef[2][1] * sin(dy);
	 p[0] += v * nx;
	 p[1] += v * ny;
      }

      /* if fuse over, store it */
      if (i >= 0) {
	 points[i][0] = p[0];
	 points[i][1] = p[1];
	 points[i][2] = p[2];
      }
   }
#if 0
   if ((count_large > 10 || count_nan > 10)
       && !getenv("PVM_ARCH"))
      fprintf(stderr, "large = %d nan = %d\n", count_large, count_nan);
#endif
}

double
lyapunov(control_point *cp, int ntries)
{
  point p[10];
  double x, y;
  double xn, yn;
  double xn2, yn2;
  double dx, dy, r;
  double eps = 1e-5;
  int i;
  double sum = 0.0;

  for (i = 0; i < ntries; i++) {
    x = random_uniform11();
    y = random_uniform11();

    p[0][0] = x;
    p[0][1] = y;
    p[0][2] = 0.0;

    // get into the attractor
    iterate(cp, 1, 20+(random()%10), p);

    x = p[0][0];
    y = p[0][1];

    // take one deterministic step
    srandom(i);
    iterate(cp, 1, 0, p);

    xn = p[0][0];
    yn = p[0][1];

    do {
      dx = random_uniform11();
      dy = random_uniform11();
      r = sqrt(dx * dx + dy * dy);
    } while (r == 0.0);
    dx /= r;
    dy /= r;

    dx *= eps;
    dy *= eps;

    p[0][0] = x + dx;
    p[0][1] = y + dy;
    p[0][2] = 0.0;

    // take the same step but with eps
    srandom(i);
    iterate(cp, 1, 0, p);

    xn2 = p[0][0];
    yn2 = p[0][1];

    r = sqrt((xn-xn2)*(xn-xn2) + (yn-yn2)*(yn-yn2));

    sum += log(r/eps);
  }
  return sum/(log(2.0)*ntries);
}

/* BY is angle in degrees */
void
rotate_control_point(control_point *cp, double by)
{
   int i;
   for (i = 0; i < NXFORMS; i++) {
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

/* args must be non-overlapping */
void mult_matrix(s1, s2, d)
   double s1[2][2];
   double s2[2][2];
   double d[2][2];
{
   d[0][0] = s1[0][0] * s2[0][0] + s1[1][0] * s2[0][1];
   d[1][0] = s1[0][0] * s2[1][0] + s1[1][0] * s2[1][1];
   d[0][1] = s1[0][1] * s2[0][0] + s1[1][1] * s2[0][1];
   d[1][1] = s1[0][1] * s2[1][0] + s1[1][1] * s2[1][1];
}

double det_matrix(s)
   double s[2][2];
{
   return s[0][0] * s[1][1] - s[0][1] * s[1][0];
}

void flip_matrix(m, h)
   double m[2][2];
   int h;
{
   double s, t;
   if (h) {
      /* flip on horizontal axis */
      s = m[0][0];
      t = m[0][1];
      m[0][0] = m[1][0];
      m[0][1] = m[1][1];
      m[1][0] = s;
      m[1][1] = t;
   } else {
      /* flip on vertical axis */
      s = m[0][0];
      t = m[1][0];
      m[0][0] = m[0][1];
      m[1][0] = m[1][1];
      m[0][1] = s;
      m[1][1] = t;
   }
}

void transpose_matrix(m)
   double m[2][2];
{
   double t;
   t = m[0][1];
   m[0][1] = m[1][0];
   m[1][0] = t;
}

void choose_evector(m, r, v)
   double m[3][2], r;
   double v[2];
{
   double b = m[0][1];
   double d = m[1][1];
   double x = r - d;
   if (b > EPS) {
      v[0] = x;
      v[1] = b;
   } else if (b < -EPS) {
      v[0] = -x;
      v[1] = -b;
   } else {
      /* XXX */
      v[0] = 1.0;
      v[1] = 0.0;
   }
}


/* diagonalize the linear part of a 3x2 matrix.  the evalues are returned 
   in r as either reals on the diagonal, or a complex pair.  the evectors
   are returned as a change of coords matrix.  does not handle shearing
   transforms.
   */

void diagonalize_matrix(m, r, v)
   double m[3][2];
   double r[2][2];
   double v[2][2];
{
   double b, c, d;
   double m00, m10, m01, m11;
   m00 = m[0][0];
   m10 = m[1][0];
   m01 = m[0][1];
   m11 = m[1][1];
   b = -m00 - m11;
   c = (m00 * m11) - (m01 * m10);
   d = b * b - 4 * c;
   /* should use better formula, see numerical recipes */
   if (d > EPS) {
      double r0 = (-b + sqrt(d)) / 2.0;
      double r1 = (-b - sqrt(d)) / 2.0;
      r[0][0] = r0;
      r[1][1] = r1;
      r[0][1] = 0.0;
      r[1][0] = 0.0;

      choose_evector(m, r0, v + 0);
      choose_evector(m, r1, v + 1);
   } else if (d < -EPS) {
      double uu = -b / 2.0;
      double vv = sqrt(-d) / 2.0;
      double w1r, w1i, w2r, w2i;
      r[0][0] = uu;
      r[0][1] = vv;
      r[1][0] = -vv;
      r[1][1] = uu;

      if (m01 > EPS) {
	 w1r = uu - m11;
	 w1i = vv;
	 w2r = m01;
	 w2i = 0.0;
      } else if (m01 < -EPS) {
	 w1r = m11 - uu;
	 w1i = -vv;
	 w2r = -m01;
	 w2i = 0.0;
      } else {
	 /* XXX */
	 w1r = 0.0;
	 w1i = 1.0;
	 w2r = 1.0;
	 w2i = 0.0;
      }
      v[0][0] = w1i;
      v[0][1] = w2i;
      v[1][0] = w1r;
      v[1][1] = w2r;

   } else {
      double rr = -b / 2.0;
      r[0][0] = rr;
      r[1][1] = rr;
      r[0][1] = 0.0;
      r[1][0] = 0.0;

      v[0][0] = 1.0;
      v[0][1] = 0.0;
      v[1][0] = 0.0;
      v[1][1] = 1.0;
   }
   /* order the values so that the evector matrix has is positively
      oriented.  this is so that evectors never have to cross when we
      interpolate them. it might mean that the values cross zero when they
      wouldn't have otherwise (if they had different signs) but this is the
      lesser of two evils */
   if (det_matrix(v) < 0.0) {
      flip_matrix(v, 1);
      flip_matrix(r, 0);
      flip_matrix(r, 1);
   }
}


void undiagonalize_matrix(r, v, m)
   double r[2][2];
   double v[2][2];
   double m[3][2];
{
   double v_inv[2][2];
   double t1[2][2];
   double t2[2][2];
   double t;
   /* the unfortunate truth is that given we are using row vectors
      the evectors should be stacked horizontally, but the complex
      interpolation functions only work on rows, so we fix things here */
   transpose_matrix(v);
   mult_matrix(r, v, t1);

   t = 1.0 / det_matrix(v);
   v_inv[0][0] = t * v[1][1];
   v_inv[1][1] = t * v[0][0];
   v_inv[1][0] = t * -v[1][0];
   v_inv[0][1] = t * -v[0][1];

   mult_matrix(v_inv, t1, t2);

   /* the unforunate truth is that i have no idea why this is needed. sigh. */
   transpose_matrix(t2);

   /* switch v back to how it was */
   transpose_matrix(v);

   m[0][0] = t2[0][0];
   m[0][1] = t2[0][1];
   m[1][0] = t2[1][0];
   m[1][1] = t2[1][1];
}

void interpolate_angle(t, s, v1, v2, v3, tie)
   double t, s;
   double *v1, *v2, *v3;
   int tie;
{
   double x = *v1;
   double y = *v2;
   double d;

   /* take the shorter way around the circle */
   if (x > y) {
      d = x - y;
      if (d > M_PI + EPS ||
	  (d > M_PI - EPS && tie)) {
	 y += 2*M_PI;
      }
   } else {
      d = y - x;
      if (d > M_PI + EPS ||
	  (d > M_PI - EPS && tie)) {
	 x += 2*M_PI;
      }
   }

   *v3 = s * x + t * y;
}

void interpolate_complex(t, s, r1, r2, r3, flip, tie)
   double t, s;
   double r1[2], r2[2], r3[2];
   int flip, tie;
{
   double c1[2], c2[2], c3[2];
   double a1, a2, a3, d1, d2, d3;

   c1[0] = r1[0];
   c1[1] = r1[1];
   c2[0] = r2[0];
   c2[1] = r2[1];
   if (flip) {
      double t = c1[0];
      c1[0] = c1[1];
      c1[1] = t;
      t = c2[0];
      c2[0] = c2[1];
      c2[1] = t;
   }

   /* convert to log space */
   a1 = atan2(c1[1], c1[0]);
   a2 = atan2(c2[1], c2[0]);
   d1 = 0.5 * log(c1[0] * c1[0] + c1[1] * c1[1]);
   d2 = 0.5 * log(c2[0] * c2[0] + c2[1] * c2[1]);

   /* interpolate linearly */
   interpolate_angle(t, s, &a1, &a2, &a3, tie);
   d3 = s * d1 + t * d2;

   /* convert back */
   d3 = exp(d3);
   c3[0] = cos(a3) * d3;
   c3[1] = sin(a3) * d3;

   if (flip) {
      r3[1] = c3[0];
      r3[0] = c3[1];
   } else {
      r3[0] = c3[0];
      r3[1] = c3[1];
   }
}


void interpolate_matrix(double t, double m1[3][2], double m2[3][2], double m3[3][2]) {
   double s = 1.0 - t;

#if 1
   m3[0][0] = s * m1[0][0] + t * m2[0][0];
   m3[0][1] = s * m1[0][1] + t * m2[0][1];

   m3[1][0] = s * m1[1][0] + t * m2[1][0];
   m3[1][1] = s * m1[1][1] + t * m2[1][1];
#else
   interpolate_complex(t, s, m1 + 0, m2 + 0, m3 + 0, 0, 0);
   interpolate_complex(t, s, m1 + 1, m2 + 1, m3 + 1, 1, 1);
#endif

   /* handle the translation part of the xform linearly */
   m3[2][0] = s * m1[2][0] + t * m2[2][0];
   m3[2][1] = s * m1[2][1] + t * m2[2][1];
}

void interpolate_cmap(double cmap[256][3], double blend,
		      int index0, double hue0, int index1, double hue1) {
  double p0[256][3];
  double p1[256][3];
  int i, j;

  get_cmap(index0, p0, 256, hue0);
  get_cmap(index1, p1, 256, hue1);
  
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
#define INTERI(x)  result->x = (int)(c0 * cps[i1].x + c1 * cps[i2].x)

/*
 * create a control point that interpolates between the control points
 * passed in CPS.  for now just do linear.  in the future, add control
 * point types and other things to the cps.  CPS must be sorted by time.
 */
void interpolate(control_point cps[], int ncps, double time, control_point *result) {
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
     rgb2hsv(cps[i1].cmap[i], s);
     rgb2hsv(cps[i2].cmap[i], t);
     for (j = 0; j < 3; j++)
       t[j] = c0 * s[j] + c1 * t[j];
     hsv2rgb(t, result->cmap[i]);
   }

   result->cmap_index = -1;
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
   INTERP(background[0]);
   INTERP(background[1]);
   INTERP(background[2]);
   INTERP(pixels_per_unit);
   INTERP(spatial_filter_radius);
   INTERP(sample_density);
   INTERP(zoom);
   INTERI(nbatches);
   INTERI(white_level);

   for (i = 0; i < NXFORMS; i++) {
      INTERP(xform[i].density);
      INTERP(xform[i].color);
      INTERP(xform[i].symmetry);
      for (j = 0; j < NVARS; j++)
	 INTERP(xform[i].var[j]);
      interpolate_matrix(c1, cps[i1].xform[i].c, cps[i2].xform[i].c,
			 result->xform[i].c);
   }
}

int compare_xforms(const xform *a, const xform *b) {
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



control_point xml_current_cp;
control_point *xml_all_cp;
int xml_all_ncps;
int xml_current_xform;

void clear_current_cp() {
    int i, j;
    control_point *cp = &xml_current_cp;

    cp->cmap_index = -1;
    cp->background[0] = 0.0;
    cp->background[1] = 0.0;
    cp->background[2] = 0.0;
    cp->center[0] = 0.0;
    cp->center[1] = 0.0;
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
    cp->white_level = 200;
    cp->symmetry = 0;

    for (i = 0; i < NXFORMS; i++) {
	cp->xform[i].density = 0.0;
	cp->xform[i].symmetry = 0;
	cp->xform[i].color = (i == 0);
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
}

static char *var_names[NVARS] = {
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
  "popcorn"
};

int var2n(const char *s) {
  int i;
  for (i = 0; i < NVARS; i++)
    if (!strcmp(s, var_names[i])) return i;
  return variation_none;
}

void start_element(void *userData, const char *name, const char **atts) {
    control_point *cp = &xml_current_cp;
    int i = 0, j;
    if (!strcmp(name, "flame")) {
	xml_current_xform = 0;
	while (atts[i]) {
	    const char *a = atts[i+1];
	    if (!strcmp(atts[i], "time")) {
		cp->time = atof(a);
	    } else if (!strcmp(atts[i], "palette")) {
		cp->cmap_index = atoi(a);
	    } else if (!strcmp(atts[i], "size")) {
		sscanf(a, "%d %d", &cp->width, &cp->height);
	    } else if (!strcmp(atts[i], "center")) {
		sscanf(a, "%lf %lf", &cp->center[0], &cp->center[1]);
	    } else if (!strcmp(atts[i], "scale")) {
		cp->pixels_per_unit = atof(a);
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
		cp->hue_rotation = atof(a);
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
      if (index >= 0) {
	cp->cmap[index][0] = r/255.0;
	cp->cmap[index][1] = g/255.0;
	cp->cmap[index][2] = b/255.0;
      } else {
	fprintf(stderr, "color element without index attribute.\n");
      }
    } else if (!strcmp(name, "palette")) {
      int index0, index1;
      double hue0, hue1;
      double blend = 0.5;
      index0 = index1 = cmap_random;
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
      interpolate_cmap(cp->cmap, blend, index0, hue0, index1, hue1);
    } else if (!strcmp(name, "symmetry")) {
      int kind = 0;
      while (atts[i]) {
	const char *a = atts[i+1];
	if (!strcmp(atts[i], "kind")) {
	  kind = atoi(a);
	}
	i += 2;
      }
      xml_current_xform += add_symmetry_to_control_point(cp, kind);
    } else if (!strcmp(name, "xform")) {
	int xf = xml_current_xform;
	if (xf < 0) {
	    fprintf(stderr, "<xform> not inside <flame>.\n");
	    exit(1);
	}
	if (xml_current_xform == NXFORMS) {
	    fprintf(stderr, "too many xforms, dropping on the floor.\n");
	    xml_current_xform--;
	}
	for (j = 0; j < NVARS; j++)
	  cp->xform[xf].var[j] = 0.0;
	while (atts[i]) {
	    char *a = (char *) atts[i+1];
	    if (!strcmp(atts[i], "weight")) {
		cp->xform[xf].density = atof(a);
	    } else if (!strcmp(atts[i], "symmetry")) {
		cp->xform[xf].symmetry = atof(a);
	    } else if (!strcmp(atts[i], "color")) {
		cp->xform[xf].color = atof(a);
	    } else if (!strcmp(atts[i], "var1")) {
	      int k;
	      for (k = 0; k < NVARS; k++) {
		cp->xform[xf].var[k] = 0.0;
	      }
	      k = atoi(a);
	      if (k < 0 || k >= NVARS) {
		fprintf(stderr, "bad variation: %d.\n", k);
		k = 0;
	      }
	      cp->xform[xf].var[k] = 1.0;
	    } else if (!strcmp(atts[i], "var")) {
		int k;
		for (k = 0; k < NVARS; k++) {
		    cp->xform[xf].var[k] = strtod(a, &a);
		}
	    } else if (!strcmp(atts[i], "coefs")) {
		int k, j;
		for (k = 0; k < 3; k++) {
		    for (j = 0; j < 2; j++) {
			cp->xform[xf].c[k][j] = strtod(a, &a);
		    }
		}
	    } else {
	      int v = var2n(atts[i]);
	      if (v != variation_none)
		cp->xform[xf].var[v] = atof(a);
	    }
	    i += 2;
	}
    }
}

void end_element(void *userData, const char *name) {
    if (!strcmp("flame", name)) {
	size_t s = (1+xml_all_ncps)*sizeof(control_point);
	xml_current_xform = -1;
	xml_all_cp = realloc(xml_all_cp, s);
	if (xml_current_cp.cmap_index != -1)
	  get_cmap(xml_current_cp.cmap_index,
		   xml_current_cp.cmap, 256,
		   xml_current_cp.hue_rotation);
	xml_all_cp[xml_all_ncps++] = xml_current_cp;
	clear_current_cp();
    } else if (!strcmp("xform", name)) {
	xml_current_xform++;
    }
}

control_point * parse_control_points(char *s, int *ncps) {
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

control_point * parse_control_points_from_file(FILE *f, int *ncps) {
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

  return parse_control_points(s, ncps);
}

void print_control_point(FILE *f, control_point *cp,
			 char *extra_attributes) {
    int i, j;
    char *p = "";
    fprintf(f, "%s<flame time=\"%g\"", p, cp->time);
    if (0 <= cp->cmap_index)
	fprintf(f, " palette=\"%d\"", cp->cmap_index);
    fprintf(f, " size=\"%d %d\"", cp->width, cp->height);
    if (cp->center[0] != 0.0 ||
	cp->center[1] != 0.0)
      fprintf(f, " center=\"%g %g\"", cp->center[0], cp->center[1]);
    fprintf(f, " scale=\"%g\"", cp->pixels_per_unit);
    if (cp->zoom != 0.0)
      fprintf(f, " zoom=\"%g\"", cp->zoom);
    if (cp->spatial_oversample != 1)
      fprintf(f, " oversample=\"%d\"", cp->spatial_oversample);
    fprintf(f, " filter=\"%g\"", cp->spatial_filter_radius);
    fprintf(f, " quality=\"%g\"", cp->sample_density);
    fprintf(f, " batches=\"%d\"", cp->nbatches);
    if (cp->background[0] != 0.0 ||
	cp->background[1] != 0.0 ||
	cp->background[2] != 0.0)
      fprintf(f, " background=\"%g %g %g\"", cp->background[0], cp->background[1], cp->background[2]);
    fprintf(f, " brightness=\"%g\"", cp->brightness);
    fprintf(f, " gamma=\"%g\"", cp->gamma);
    fprintf(f, " vibrancy=\"%g\"", cp->vibrancy);
    if (0 <= cp->cmap_index &&
	cp->hue_rotation != 0.0)
      fprintf(f, " hue=\"%g\"", cp->hue_rotation);
    if (extra_attributes)
	fprintf(f, " %s", extra_attributes);
    fprintf(f, ">\n");
    if (cmap_interpolated == cp->cmap_index) {
      fprintf(f, "%s   <palette blend=\"%g\" index0=\"%d\" hue0=\"%g\" ",
	      p, cp->palette_blend, cp->cmap_index0, cp->hue_rotation0);
      fprintf(f, "index1=\"%d\" hue1=\"%g\"/>\n",
	      cp->cmap_index1, cp->hue_rotation1);
    } else if (cmap_random == cp->cmap_index) {
      for (i = 0; i < 256; i++) {
	int r, g, b;
	r = (int) (cp->cmap[i][0] * 255.0);
	g = (int) (cp->cmap[i][1] * 255.0);
	b = (int) (cp->cmap[i][2] * 255.0);
	printf("%s   <color index=\"%d\" rgb=\"%d %d %d\"/>\n",
	       p, i, r, g, b);
      }
    }
    if (cp->symmetry)
      fprintf(f, "%s   <symmetry kind=\"%d\"/>\n", p, cp->symmetry);
    for (i = 0; i < NXFORMS; i++)
      if (cp->xform[i].density > 0.0
	  && !(cp->symmetry &&  cp->xform[i].symmetry == 1.0)) {
	int nones = 0;
	int nzeroes = 0;
	int lastnonzero = 0;
	fprintf(f, "%s   <xform weight=\"%g\" color=\"%g\" ",
		p, cp->xform[i].density, cp->xform[i].color);
	if (cp->xform[i].symmetry != 0.0) {
	  fprintf(f, "symmetry=\"%g\" ", cp->xform[i].symmetry);
	}
#if 1
	for (j = 0; j < NVARS; j++) {
	  double v = cp->xform[i].var[j];
	  if (0.0 != v) {
	    fprintf(f, "%s=\"%g\" ", var_names[j], v);
	  }
	}
#else
	for (j = 0; j < NVARS; j++) {
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
	    NVARS-1 == nzeroes) {
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
	fprintf(f, "\"/>\n");
    }
    fprintf(f, "%s</flame>\n", p);
}

/* returns a uniform variable from 0 to 1 */
double random_uniform01() {
   return (random() & 0xfffffff) / (double) 0xfffffff;
}

double random_uniform11() {
   return ((random() & 0xfffffff) - 0x7ffffff) / (double) 0x7ffffff;
}

/* returns a mean 0 variance 1 random variable
   see numerical recipies p 217 */
double random_gaussian() {
   static int iset = 0;
   static double gset;
   double fac, r, v1, v2;

   if (0 == iset) {
      do {
	 v1 = random_uniform11();
	 v2 = random_uniform11();
	 r = v1 * v1 + v2 * v2;
      } while (r >= 1.0 || r == 0.0);
      fac = sqrt(-2.0 * log(r)/r);
      gset = v1 * fac;
      iset = 1;
      return v2 * fac;
   }
   iset = 0;
   return gset;
}

double round6(double x) {
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
int add_symmetry_to_control_point(control_point *cp, int sym) {
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

  for (i = 0; i < NXFORMS; i++)
    if (cp->xform[i].density == 0.0)
      break;

  if (i == NXFORMS) return 0;

  cp->symmetry = sym;

  if (sym < 0) {
    cp->xform[i].density = 1.0;
    cp->xform[i].symmetry = 1.0;
    cp->xform[i].var[0] = 1.0;
    for (j = 1; j < NVARS; j++)
      cp->xform[i].var[j] = 0;
    cp->xform[i].color = 1.0;
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

  for (k = 1; (k < sym) && (i < NXFORMS); k++) {
    cp->xform[i].density = 1.0;
    cp->xform[i].var[0] = 1.0;
    cp->xform[i].symmetry = 1.0;
    for (j = 1; j < NVARS; j++)
      cp->xform[i].var[j] = 0;
    cp->xform[i].color = (sym<3) ? 0.0 : ((k-1.0)/(sym-2.0));

    cp->xform[i].c[0][0] = round6(cos(k*a));
    cp->xform[i].c[0][1] = round6(sin(k*a));
    cp->xform[i].c[1][0] = round6(-cp->xform[i].c[0][1]);
    cp->xform[i].c[1][1] = cp->xform[i].c[0][0];
    cp->xform[i].c[2][0] = 0.0;
    cp->xform[i].c[2][1] = 0.0;

    i++;
    result++;
  }

  qsort((char *) &cp->xform[i-result], result, sizeof(xform), compare_xforms);

  return result;
}

int random_var() {
  return random()%NVARS;
}

void random_control_point(control_point *cp, int ivar, int sym) {
   int i, nxforms, var;
   static int xform_distrib[] = {
      2, 2, 2,
      3, 3, 3,
      4, 4,
      5};

   cp->hue_rotation = random_uniform01();
   cp->cmap_index = get_cmap(cmap_random, cp->cmap, 256, cp->hue_rotation);
   cp->time = 0.0;
   nxforms = random_distrib(xform_distrib);
   if (variation_random == ivar) {
     if (random()%4) {
       var = random_var();
     } else {
       var = variation_random;
     }
   } else {
     var = ivar;
   }

   for (i = 0; i < nxforms; i++) {
      int j, k;
      cp->xform[i].density = 1.0 / nxforms;
      cp->xform[i].color = (nxforms==1)?1.0:(i/(double)(nxforms-1));
      cp->xform[i].symmetry = 0.0;
      for (j = 0; j < 3; j++)
	 for (k = 0; k < 2; k++)
	    cp->xform[i].c[j][k] = random_uniform11();
      for (j = 0; j < NVARS; j++)
	 cp->xform[i].var[j] = 0.0;
      if (variation_random != var)
	 cp->xform[i].var[var] = 1.0;
      else
	 cp->xform[i].var[random_var()] = 1.0;
   }
   for (; i < NXFORMS; i++)
      cp->xform[i].density = 0.0;

   if (sym || !(random()%4)) {
     add_symmetry_to_control_point(cp, sym);
   } else
     cp->symmetry = 0;

   qsort((char *) cp->xform, NXFORMS, sizeof(xform), compare_xforms);
}

/*
 * find a 2d bounding box that does not enclose eps of the fractal density
 * in each compass direction.  works by binary search.
 * this is stupid, it shouldjust use the find nth smallest algorithm.
 */
void estimate_bounding_box(cp, eps, bmin, bmax)
   control_point *cp;
   double eps;
   double *bmin;
   double *bmax;
{
   int i, j, batch = (int)((eps == 0.0) ? 10000 : 10.0/eps);
   int low_target = (int)(batch * eps);
   int high_target = batch - low_target;
   point min, max, delta;
   point *points = (point *)  malloc(sizeof(point) * batch);
   iterate(cp, batch, 20, points);

   min[0] = min[1] =  1e10;
   max[0] = max[1] = -1e10;
   
   for (i = 0; i < batch; i++) {
      if (points[i][0] < min[0]) min[0] = points[i][0];
      if (points[i][1] < min[1]) min[1] = points[i][1];
      if (points[i][0] > max[0]) max[0] = points[i][0];
      if (points[i][1] > max[1]) max[1] = points[i][1];
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
	 if (points[j][0] < bmin[0]) n++;
	 if (points[j][0] > bmax[0]) s++;
	 if (points[j][1] < bmin[1]) w++;
	 if (points[j][1] > bmax[1]) e++;
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

/* use hill climberer to find smooth ordering of control points
   this is untested */
   
void sort_control_points(cps, ncps, metric)
   control_point *cps;
   int ncps;
   double (*metric)();
{
   int niter = ncps * 1000;
   int i, n, m;
   double same, swap;
   for (i = 0; i < niter; i++) {
      /* consider switching points with indexes n and m */
      n = random() % ncps;
      m = random() % ncps;

      same = (metric(cps + n, cps + (n - 1) % ncps) +
	      metric(cps + n, cps + (n + 1) % ncps) +
	      metric(cps + m, cps + (m - 1) % ncps) +
	      metric(cps + m, cps + (m + 1) % ncps));

      swap = (metric(cps + n, cps + (m - 1) % ncps) +
	      metric(cps + n, cps + (m + 1) % ncps) +
	      metric(cps + m, cps + (n - 1) % ncps) +
	      metric(cps + m, cps + (n + 1) % ncps));

      if (swap < same) {
	 control_point t;
	 t = cps[n];
	 cps[n] = cps[m];
	 cps[m] = t;
      }
   }
}

/* this has serious flaws in it */

double standard_metric(cp1, cp2)
   control_point *cp1, *cp2;
{
   int i, j, k;
   double t;
   
   double dist = 0.0;
   for (i = 0; i < NXFORMS; i++) {
      double var_dist = 0.0;
      double coef_dist = 0.0;
      for (j = 0; j < NVARS; j++) {
	 t = cp1->xform[i].var[j] - cp2->xform[i].var[j];
	 var_dist += t * t;
      }
      for (j = 0; j < 3; j++)
	 for (k = 0; k < 2; k++) {
	    t = cp1->xform[i].c[j][k] - cp2->xform[i].c[j][k];
	    coef_dist += t *t;
	 }

      /* weight them equally for now. */
      dist += var_dist + coef_dist;
   }
   return dist;
}

void
stat_matrix(f, m)
   FILE *f;
   double m[3][2];
{
   double r[2][2];
   double v[2][2];
   double a;

   diagonalize_matrix(m, r, v);
   fprintf(f, "entries = % 10f % 10f % 10f % 10f\n",
	   m[0][0], m[0][1], m[1][0], m[1][1]);
   fprintf(f, "evalues  = % 10f % 10f % 10f % 10f\n",
	   r[0][0], r[0][1], r[1][0], r[1][1]);
   fprintf(f, "evectors = % 10f % 10f % 10f % 10f\n",
	   v[0][0], v[0][1], v[1][0], v[1][1]);
   a = (v[0][0] * v[1][0] + v[0][1] * v[1][1]) /
      sqrt((v[0][0] * v[0][0] + v[0][1] * v[0][1]) *
	   (v[1][0] * v[1][0] + v[1][1] * v[1][1]));
   fprintf(f, "theta = %g det = %g\n", a,
	   m[0][0] * m[1][1] - m[0][1] * m[1][0]);
}


#if 0
main()
{
#if 0
   double m1[3][2] = {-0.633344, -0.269064, 0.0676171, 0.590923, 0, 0};
   double m2[3][2] = {-0.844863, 0.0270297, -0.905294, 0.413218, 0, 0};
#endif

#if 0
   double m1[3][2] = {-0.347001, -0.15219, 0.927161, 0.908305, 0, 0};
   double m2[3][2] = {-0.577884, 0.653803, 0.664982, -0.734136, 0, 0};
#endif

#if 0
   double m1[3][2] = {1, 0, 0, 1, 0, 0};
   double m2[3][2] = {0, -1, 1, 0, 0, 0};
#endif

#if 1
   double m1[3][2] = {1, 0, 0, 1, 0, 0};
   double m2[3][2] = {-1, 0, 0, -1, 0, 0};
#endif

   double m3[3][2];
   double t;
   int i = 0;

   for (t = 0.0; t <= 1.0; t += 1.0/15.0) {
      int x, y;
      fprintf(stderr, "%g--\n", t);
      interpolate_matrix(t, m1, m2, m3);
/*       stat_matrix(stderr, m3); */
      x = (i % 4) * 100 + 100;
      y = (i / 4) * 100 + 100;
      printf("newpath ");
      printf("%d %d %d %d %d arc ", x, y, 30, 0, 360);
      printf("%d %d moveto ", x, y);
      printf("%g %g rlineto ", m3[0][0] * 30, m3[0][1] * 30);
      printf("%d %d moveto ", x, y);
      printf("%g %g rlineto ", m3[1][0] * 30, m3[1][1] * 30);
      printf("stroke \n");
      printf("newpath ");
      printf("%g %g %d %d %d arc ", x + m3[0][0] * 30, y + m3[0][1] * 30, 3, 0, 360);
      printf("stroke \n");
      i++;
   }
}
#endif
