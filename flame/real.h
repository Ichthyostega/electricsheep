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


#ifndef real_included
#define real_included


#include <stdio.h>
#include <math.h>

#ifndef M_PI
#  define M_PI 3.1415926
#endif

   /* use type real instead of double,
      and use F(cos)() instead of cos(). */
#if 0
#  define real float
#  ifdef __STDC__
#    define F(x) x##f
#  else
#    ifdef BSD
#      define F(x) f\\
x
#    else
#      define F(x) f/**/x
#    endif
#  endif
#else
#  define real double
#  define F(x) x
#endif

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define EPS (1e-10)

#if __STDC__
# define P(x)x
#else
# define P(x)()
#endif

#define vlen(x) (sizeof(x)/sizeof(*x))

#endif

