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


#include "flam3.h"

#include <stdlib.h>

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <expat.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "config.h"

#define EPS (1e-10)
#define CMAP_SIZE 256
#define rbit() (flam3_random_bit())
#define flam3_variation_none   (-1)
#define vlen(x) (sizeof(x)/sizeof(*x))

extern void rgb2hsv(double *rgb, double *hsv);
extern void hsv2rgb(double *hsv, double *rgb);


#ifdef WIN32
#define M_PI 3.1415926539
#define random()  (rand() ^ (rand()<<15))
#define srandom(x)  (srand(x))
extern int getpid();
#endif

#define argi(s,d)   ((ai = getenv(s)) ? atoi(ai) : (d))
#define argf(s,d)   ((ai = getenv(s)) ? atof(ai) : (d))
#define args(s,d)   ((ai = getenv(s)) ? ai : (d))

extern char *docstring;
