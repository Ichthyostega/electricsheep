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


static char *jpeg_c_id =
"@(#) $Id: jpeg.c,v 1.9 2004/03/04 06:35:17 spotspot Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

#include "jpeg.h"

void
write_jpeg(FILE *file, char *image, int width, int height) {
    struct jpeg_compress_struct info;
    struct jpeg_error_mgr jerr;
    int i;

    info.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, file);
    info.in_color_space = JCS_RGB;
    info.input_components = 3;
    info.image_width = width;
    info.image_height = height;
    jpeg_set_defaults(&info);
    if (getenv("quality")) {
	int quality = atoi(getenv("quality"));
	jpeg_set_quality(&info, quality, TRUE);
    }
    jpeg_start_compress(&info, TRUE);
    for (i = 0; i < height; i++) {
	JSAMPROW row_pointer[1];
	row_pointer[0] = image + (3 * width * i);
	jpeg_write_scanlines(&info, row_pointer, 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
}
