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
"@(#) $Id: png.c,v 1.2 2004/03/04 06:35:17 spotspot Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

void write_png(FILE *file, char *image, int width, int height) {
  png_structp  png_ptr;
  png_infop    info_ptr;
  int          i;
  char         **rows = malloc(sizeof(char *) * height);

  for (i = 0; i < height; i++)
    rows[i] = image + i * width * 4;
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
				    NULL, NULL, NULL);
  info_ptr = png_create_info_struct(png_ptr);

  if (setjmp(png_jmpbuf(png_ptr))) {
     fclose(file);
     png_destroy_write_struct(&png_ptr, &info_ptr);
     perror("writing file");
     return;
  }
  png_init_io(png_ptr, file);

  png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA,
	       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_write_info(png_ptr, info_ptr);
  png_write_image(png_ptr, (png_bytepp) rows);
  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  free(rows);
}
