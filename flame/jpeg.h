/*
    flame - cosmic recursive fractal flames
    Copyright (C) 2002-2003  Scott Draves <source@flam3.com>

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


static char *jpeg_h_id =
"@(#) $Id: jpeg.h,v 1.6 2004/03/04 06:35:17 spotspot Exp $";

#include <stdio.h>

void write_jpeg(FILE *file, char *image, int width, int height);
void write_png(FILE *file, char *image, int width, int height);
