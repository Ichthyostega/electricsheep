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

#ifndef rect_included
#define rect_included

static char *rect_h_id =
"@(#) $Id: rect.h,v 1.8 2004/03/04 06:35:17 spotspot Exp $";

#include "libifs.h"


/* size of the cmap actually used. may be smaller than input cmap size */
#define CMAP_SIZE 256

typedef struct {
   double         temporal_filter_radius;
   double         pixel_aspect_ratio;    /* width over height of each pixel */
   control_point *cps;
   int            ncps;
   double         time;
} frame_spec;


#define field_both  0
#define field_even  1
#define field_odd   2


extern void render_rectangle(frame_spec *spec, unsigned char *out,
			     int out_width, int field, int nchan,
			     void progress(double));

#endif
