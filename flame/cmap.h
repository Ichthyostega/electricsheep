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


#ifndef cmap_included
#define cmap_included

static char *cmap_h_id =
"@(#) $Id: cmap.h,v 1.8 2004/03/04 06:35:17 spotspot Exp $";

#define cmap_random (-1)
#define cmap_interpolated (-2)



#define vlen(x) (sizeof(x)/sizeof(*x))

typedef double clrmap[256][3];

extern void rgb2hsv(double *rgb, double *hsv);
extern void hsv2rgb(double *hsv, double *rgb);
extern int get_cmap(int n, clrmap c, int cmap_len, double hue_rotation);

#endif
