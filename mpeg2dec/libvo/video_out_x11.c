/*
 * video_out_x11.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#include "config.h"

#ifdef LIBVO_X11

extern int window_id;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <inttypes.h>
#include <sys/time.h>
#include <png.h>
#include <setjmp.h>

#include "vroot.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase (Display *);

#ifdef LIBVO_XV
#include <string.h>	/* strcmp */
#include <X11/extensions/Xvlib.h>
#define FOURCC_YV12 0x32315659
#endif

#include "video_out.h"
#include "video_out_internal.h"

typedef struct x11_frame_s {
    vo_frame_t vo;
    uint8_t * rgb_ptr;
    int rgb_stride;
    int yuv_stride;
    XImage * ximage;
    int wait_completion;
#ifdef LIBVO_XV
    XvImage * xvimage;	/* FIXME have only one ximage/xvimage pointer ? */
#endif
} x11_frame_t;

typedef struct x11_instance_s {
    vo_instance_t vo;
    int prediction_index;
    vo_frame_t * frame_ptr[3];
    x11_frame_t frame[3];

    /* local data */
    int width;
    int height;

    int displaywidth;
    int displayheight;

    /* X11 related variables */
    Display * display;
    Window window;
    GC gc;
    XVisualInfo vinfo;
    XShmSegmentInfo shminfo;
    int corner_x, corner_y;
    int completion_type;
#ifdef LIBVO_XV
    XvPortID port;
#endif
} x11_instance_t;

static uint32_t frame_counter = 0;

static void frame_rate_delay()
{
    static struct timeval tv_rate;
    struct timeval tv_end;
    extern int target_fps;
    int d;
    double e;

    gettimeofday (&tv_end, NULL);

    if (!frame_counter++) {
	tv_rate = tv_end;
	return;
    }

    e = (tv_end.tv_sec - tv_rate.tv_sec)  +
      (tv_end.tv_usec - tv_rate.tv_usec) * 1e-6;
    d = 1000 * 1000 * (1.0/target_fps - e);
    if (d > 0) {
      usleep(d);
      gettimeofday (&tv_end, NULL);
    }

    tv_rate = tv_end;
}

static char *overlay_luma = NULL;
static char *overlay_alpha = NULL;
static int overlay_width;
static int overlay_height;
static int overlay_start_fade_in_frame;
static int overlay_end_fade_out_frame;
static int overlay_end_fade_in_frame;
static int overlay_end_hold_frame;

static int fifo_fd;
static void fifo_init()
{
    char *hom = getenv("HOME");
    char fname[MAXPATHLEN];


    /* assuming the cache dir is ~/.sheep is wrong xxx */
    sprintf(fname, "%s/.sheep/overlay_fifo", hom);

    unlink(fname);

    if (-1 == mkfifo(fname, S_IRWXU)) {
	perror(fname);
	fifo_fd = -1;
    } else if (-1 == (fifo_fd = open(fname, O_NONBLOCK))) {
	perror(fname);
	fifo_fd = -1;
    }


}

#define SIG_CHECK_SIZE 8

static void overlay_read_png(FILE *ifp)
{
  unsigned char sig_buf [SIG_CHECK_SIZE];
  png_struct *png_ptr;
  png_info *info_ptr;
  png_byte **png_image = NULL;
  int linesize, x, y;
  unsigned char *p, *q;

  if (fread (sig_buf, 1, SIG_CHECK_SIZE, ifp) != SIG_CHECK_SIZE) {
    fprintf (stderr, "input file empty or too short\n");
    return;
  }
  if (png_sig_cmp (sig_buf, (png_size_t) 0, (png_size_t) SIG_CHECK_SIZE) != 0) {
    fprintf (stderr, "input file not a PNG file\n");
    return;
  }

  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL) {
    fprintf (stderr, "cannot allocate LIBPNG structure\n");
    return;
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
     if (png_image) {
	 for (y = 0 ; y < info_ptr->height ; y++)
	     free (png_image[y]);
	 free (png_image);
     }
     png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
     perror("reading file");
     return;
  }
  info_ptr = png_create_info_struct (png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct (&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    fprintf (stderr, "cannot allocate LIBPNG structures\n");
    return;
  }

  png_init_io (png_ptr, ifp);
  png_set_sig_bytes (png_ptr, SIG_CHECK_SIZE);
  png_read_info (png_ptr, info_ptr);

  if (8 != info_ptr->bit_depth) {
    fprintf(stderr, "bit depth type must be 8, not %d.\n",
	    info_ptr->bit_depth);
    return;
  }
  overlay_width = info_ptr->width;
  overlay_height = info_ptr->height;
  if (overlay_luma) {
    free(overlay_luma);
    free(overlay_alpha);
  }

  overlay_luma = malloc(overlay_width * overlay_height);
  overlay_alpha = malloc(overlay_width * overlay_height);
  png_image = (png_byte **)malloc (info_ptr->height * sizeof (png_byte*));
  linesize = info_ptr->width;
  switch (info_ptr->color_type) {
    case PNG_COLOR_TYPE_RGB:
      linesize *= 3;
      break;
    case PNG_COLOR_TYPE_RGBA:
      linesize *= 4;
      break;
    case PNG_COLOR_TYPE_GRAY:
      break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
      linesize *= 2;
      break;

  default:
    fprintf(stderr, "color type must be RGB, RGBA, GRAY, "
	    "or GRAY_ALPHA not %d.\n",
	    info_ptr->color_type);
    return;
  }


  for (y = 0 ; y < info_ptr->height ; y++) {
    png_image[y] = malloc (linesize);
  }
  png_read_image (png_ptr, png_image);
  png_read_end (png_ptr, info_ptr);

  p = (unsigned char *) overlay_luma;
  q = (unsigned char *) overlay_alpha;
  for (y = 0 ; y < info_ptr->height ; y++) {
    unsigned char *s = png_image[y];
    for (x = 0 ; x < info_ptr->width ; x++) {

      switch (info_ptr->color_type) {
      case PNG_COLOR_TYPE_RGB:
	*p++ = 0.587*s[1] + 0.299*s[0] + 0.114*s[2];
	*q++ = (s[0]+s[1]+s[2])?255:0;
	s += 3;
	break;
      case PNG_COLOR_TYPE_RGBA:
	*p++ = 0.587*s[1] + 0.299*s[0] + 0.114*s[2];
	*q++ = s[3];
	s += 4;
	break;
    case PNG_COLOR_TYPE_GRAY:
	*p++ = s[0];
	*q++ = s[0]?255:0;
	s += 1;
	break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
	*p++ = s[0];
	*q++ = s[1];
	s += 2;
	break;
      }
    }
  }

  for (y = 0 ; y < info_ptr->height ; y++)
    free (png_image[y]);
  free (png_image);
  png_destroy_read_struct (&png_ptr, &info_ptr, (png_infopp)NULL);  
}

static void overlay_read_ppm(FILE *fin)
{
    int c, i, j;
#if 0
    short *p;
#else
    unsigned char *p, *q;
#endif

    fscanf(fin, "P6\n");
    c = fgetc(fin);
    if ('#' == c) {
	while ('\n' != (c = fgetc(fin)))
	    ;
    } else
	ungetc(c, fin);
    fscanf(fin, "%d %d\n", &overlay_width, &overlay_height);
    fscanf(fin, "%d\n", &c);
    if (overlay_luma) {
	free(overlay_luma);
	free(overlay_alpha);
    }
#if 0
    overlay_luma = malloc(overlay_width * overlay_height * 2);
    p = (short *) overlay_luma;
#else
    overlay_luma = malloc(overlay_width * overlay_height);
    p = (unsigned char *) overlay_luma;
    overlay_alpha = malloc(overlay_width * overlay_height);
    q = (unsigned char *) overlay_alpha;
#endif
    for (i = 0; i < overlay_height; i++)
	for (j = 0; j < overlay_width; j++) {
	    int r = fgetc(fin);
	    int g = fgetc(fin);
	    int b = fgetc(fin);
#if 0
	    *p++ = ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
#else
	    *p++ = 0.587*g + 0.299*r + 0.114*b;
	    *q++ = (r+g+b)?255:0;
#endif
	}
}

static void overlay_read_image(char *name)
{
    char fname[MAXPATHLEN];
    int fade_in_frames;
    int fade_out_frames;
    int hold_frames;
    FILE *fin;

    sscanf(name, "%s %d %d %d", fname, &fade_in_frames, &hold_frames, &fade_out_frames);

    fin = fopen(fname, "r");
    if (NULL == fin) {
	perror(fname);
	return;
    }

    if (strlen(fname) > 4 && !strcmp(fname+strlen(fname)-4, ".ppm"))
      overlay_read_ppm(fin);
    else
      overlay_read_png(fin);

    fclose(fin);

    overlay_start_fade_in_frame = frame_counter;
    overlay_end_fade_in_frame =  frame_counter + fade_in_frames;
    overlay_end_hold_frame = overlay_end_fade_in_frame + hold_frames;
    overlay_end_fade_out_frame = overlay_end_hold_frame + fade_out_frames;
}

#define bufsize 1000

static void
overlay_draw(x11_instance_t *instance, x11_frame_t *frame)
{

  if (fifo_fd > 0) {
    int nbytesread;
    char buf[bufsize];
    if (-1 == (nbytesread = read(fifo_fd, buf, bufsize))) {
      perror("fifo read");
      close(fifo_fd);
      fifo_fd = -1;
    }
    if (nbytesread > 0) {
      int i;
      int cmd_start = -1, cmd_end = -1;
      for (i = 0; i < bufsize; i++) {
	if ('!' == buf[i]) {
	  cmd_start = i;
	  for (; i < bufsize; i++) {
	    if ('\n' == buf[i]) {
	      cmd_end = i;
	      break;
	    }
	  }
	  break;
	}
      }
      if (cmd_start >= 0 && cmd_end >= 0) {
	buf[cmd_end] = 0;
	overlay_read_image(buf + cmd_start + 1);
      }
    }
  }

  if (overlay_luma && frame_counter > overlay_end_fade_out_frame) {
    free(overlay_luma);
    free(overlay_alpha);
    overlay_luma = 0;
    overlay_alpha = 0;
  }

  if (overlay_luma) {
    int i, j;
    int voffset, hoffset;
    int vsize, hsize;
    double fraction;
#if 0
    short *ximg_base = (short *) frame->ximage->data;
#else
    unsigned char *ximg_base =
      (unsigned char *) frame->xvimage->data;
#endif

    if (frame_counter < overlay_end_fade_in_frame) {
      fraction = 1.0 -
	((overlay_end_fade_in_frame - frame_counter) / 
	 (double)(overlay_end_fade_in_frame - overlay_start_fade_in_frame));
    } else if (frame_counter < overlay_end_hold_frame) {
      fraction = 1.0;
    } else {
      fraction = 
	((overlay_end_fade_out_frame - frame_counter) / 
	 (double)(overlay_end_fade_out_frame - overlay_end_hold_frame));
    }

    if (overlay_width >= instance->width) {
      hoffset = 0;
      hsize = instance->width;
    } else {
      hoffset = (instance->width - overlay_width)/2;
      hsize = overlay_width;
    }
    if (overlay_height >= instance->height) {
      voffset = 0;
      vsize = instance->height;
    } else {
      voffset = (instance->height - overlay_height)/2;
      vsize = overlay_height;
    }
    for (i = 0; i < vsize; i++) {
      unsigned char *ximg_row = ximg_base + hoffset +
	(i + voffset) * instance->width;
      unsigned char *ovrl_row =
	((unsigned char *)overlay_luma) + i * overlay_width;
      unsigned char *ovrl_alpha_row =
	((unsigned char *)overlay_alpha) + i * overlay_width;
      for (j = 0; j < hsize; j++) {
	int src = *ovrl_row++;
	int dst = *ximg_row;
	double ff = fraction * *ovrl_alpha_row++ * 1.0/255.0;
	*ximg_row++ = src * ff + dst * (1.0-ff);
      }
    }
  }
		
#if 0
  // evil 565=16bpp format
  0xffff;  // white
  0x001f;  // blue
  0x07e0;  // green
  0xf800;  // red
#endif

}


static int open_display (x11_instance_t * instance)
{
    int major;
    int minor;
    Bool pixmaps;
    XVisualInfo visualTemplate;
    XVisualInfo * XvisualInfoTable;
    XVisualInfo * XvisualInfo;
    int number;
    int i;
    XSetWindowAttributes attr;
    XGCValues gcValues;

    instance->display = XOpenDisplay (NULL);
    if (! (instance->display)) {
	fprintf (stderr, "Can not open display\n");
	return 1;
    }

    if ((XShmQueryVersion (instance->display, &major, &minor,
			   &pixmaps) == 0) ||
	(major < 1) || ((major == 1) && (minor < 1))) {
	fprintf (stderr, "No xshm extension\n");
	return 1;
    }

    instance->completion_type =
	XShmGetEventBase (instance->display) + ShmCompletion;

    /* list truecolor visuals for the default screen */
    visualTemplate.class = TrueColor;
    visualTemplate.screen = DefaultScreen (instance->display);
    XvisualInfoTable = XGetVisualInfo (instance->display,
				       VisualScreenMask | VisualClassMask,
				       &visualTemplate, &number);
    if (XvisualInfoTable == NULL) {
	fprintf (stderr, "No truecolor visual\n");
	return 1;
    }

    /* find the visual with the highest depth */
    XvisualInfo = XvisualInfoTable;
    for (i = 1; i < number; i++)
	if (XvisualInfoTable[i].depth > XvisualInfo->depth)
	    XvisualInfo = XvisualInfoTable + i;

    instance->vinfo = *XvisualInfo;
    XFree (XvisualInfoTable);

    attr.background_pixmap = None;
    attr.backing_store = NotUseful;
    attr.border_pixel = 0;
    attr.event_mask = 0;
    /* fucking sun blows me - you have to create a colormap there... */
    attr.colormap = XCreateColormap (instance->display,
				     RootWindow (instance->display,
						 instance->vinfo.screen),
				     instance->vinfo.visual, AllocNone);
    instance->corner_x = instance->corner_y = 0;
    instance->displaywidth = instance->width;
    instance->displayheight = instance->height;

    if (window_id == -3) {
      /* display zoomed on the (virtual) root window */
      instance->window = DefaultRootWindow (instance->display);
      instance->displaywidth = DisplayWidth(instance->display, DefaultScreen (instance->display));
      instance->displayheight = DisplayHeight(instance->display, DefaultScreen (instance->display));
    } else if (window_id == -2) {
      /* display non-zoomed on the (virtual) root window */
      int w, h;
      w = DisplayWidth(instance->display, DefaultScreen (instance->display));
      h = DisplayHeight(instance->display, DefaultScreen (instance->display));
      instance->window = DefaultRootWindow (instance->display);
      instance->corner_x = (w - instance->width)/2;
      instance->corner_y = (h - instance->height)/2;
    } else if (window_id == -1) {
      XTextProperty xname;
      char *nm = "Electric Sheep";
      /* create a window the same size as the video */
      instance->window =
	XCreateWindow (instance->display,
		       DefaultRootWindow (instance->display),
		       0 /* x */, 0 /* y */, instance->width, instance->height,
		       0 /* border_width */, instance->vinfo.depth,
		       InputOutput, instance->vinfo.visual,
		       (CWBackPixmap | CWBackingStore | CWBorderPixel |
			CWEventMask | CWColormap), &attr);
      XStringListToTextProperty(&nm, 1, &xname);
      XSetWMName(instance->display, instance->window, &xname);
    } else {
	/* zoomed to fit the window specified on the command line */
	XWindowAttributes xgwa;
	XGetWindowAttributes(instance->display, window_id, &xgwa);
	instance->window = window_id;
	instance->displaywidth = xgwa.width;
	instance->displayheight = xgwa.height;
    }

    instance->gc = XCreateGC (instance->display, instance->window, 0,
			      &gcValues);

    return 0;
}

static int shmerror = 0;

static int handle_error (Display * display, XErrorEvent * error)
{
    shmerror = 1;
    return 0;
}

static void * create_shm (x11_instance_t * instance, int size)
{
    instance->shminfo.shmid = shmget (IPC_PRIVATE, size, IPC_CREAT | 0777);
    if (instance->shminfo.shmid == -1)
	goto error;

    instance->shminfo.shmaddr = shmat (instance->shminfo.shmid, 0, 0);
    if (instance->shminfo.shmaddr == (char *)-1)
	goto error;

    /* on linux the IPC_RMID only kicks off once everyone detaches the shm */
    /* doing this early avoids shm leaks when we are interrupted. */
    /* this would break the solaris port though :-/ */

    /* fuck solaris, plug the leak! */
    shmctl(instance->shminfo.shmid, IPC_RMID, 0);

    /* XShmAttach fails on remote displays, so we have to catch this event */

    XSync (instance->display, False);
    XSetErrorHandler (handle_error);

    instance->shminfo.readOnly = True;
    if (! (XShmAttach (instance->display, &(instance->shminfo))))
	shmerror = 1;

    XSync (instance->display, False);
    XSetErrorHandler (NULL);
    if (shmerror) {
    error:
	fprintf (stderr, "cannot create shared memory\n");
	return NULL;
    }

    return instance->shminfo.shmaddr;
}

static void destroy_shm (x11_instance_t * instance)
{
    XShmDetach (instance->display, &(instance->shminfo));
    shmdt (instance->shminfo.shmaddr);
    shmctl (instance->shminfo.shmid, IPC_RMID, 0);
}

static void x11_event (x11_instance_t * instance)
{
    XEvent event;
    char * addr;
    int i;

    XNextEvent (instance->display, &event);
    if (event.type == instance->completion_type) {
	addr = (instance->shminfo.shmaddr +
		((XShmCompletionEvent *)&event)->offset);
	for (i = 0; i < 3; i++)
	    if (addr == instance->frame[i].ximage->data)
		instance->frame[i].wait_completion = 0;
    }
}

static vo_frame_t * x11_get_frame (vo_instance_t * _instance, int flags)
{
    x11_instance_t * instance;
    x11_frame_t * frame;

    instance = (x11_instance_t *) _instance;
    frame = (x11_frame_t *) libvo_common_get_frame ((vo_instance_t *) instance,
						    flags);

    while (frame->wait_completion)
	x11_event (instance);

    frame->rgb_ptr = (uint8_t *) frame->ximage->data;
    frame->rgb_stride = frame->ximage->bytes_per_line;
    frame->yuv_stride = instance->width;
    if ((flags & VO_TOP_FIELD) == 0)
	frame->rgb_ptr += frame->rgb_stride;
    if ((flags & VO_BOTH_FIELDS) != VO_BOTH_FIELDS) {
	frame->rgb_stride <<= 1;
	frame->yuv_stride <<= 1;
    }

    return (vo_frame_t *) frame;
}

static void x11_copy_slice (vo_frame_t * _frame, uint8_t ** src)
{
    x11_frame_t * frame;
    x11_instance_t * instance;

    frame = (x11_frame_t *) _frame;
    instance = (x11_instance_t *) frame->vo.instance;

    yuv2rgb (frame->rgb_ptr, src[0], src[1], src[2], instance->width, 16,
	     frame->rgb_stride, frame->yuv_stride, frame->yuv_stride >> 1);
    frame->rgb_ptr += frame->rgb_stride << 4;
}

static void x11_field (vo_frame_t * _frame, int flags)
{
    x11_frame_t * frame;

    frame = (x11_frame_t *) _frame;
    frame->rgb_ptr = (uint8_t *) frame->ximage->data;
    if ((flags & VO_TOP_FIELD) == 0)
	frame->rgb_ptr += frame->ximage->bytes_per_line;
}



static void x11_draw_frame (vo_frame_t * _frame)
{
    x11_frame_t * frame;
    x11_instance_t * instance;

    frame = (x11_frame_t *) _frame;
    instance = (x11_instance_t *) frame->vo.instance;

    overlay_draw(instance, frame);

    XShmPutImage (instance->display, instance->window, instance->gc,
		  frame->ximage, 0, 0, instance->corner_x, instance->corner_y,
		  instance->width, instance->height,
		  True);
    XFlush (instance->display);
    frame->wait_completion = 1;
    frame_rate_delay();
}

static int x11_alloc_frames (x11_instance_t * instance)
{
    int size;
    uint8_t * alloc;
    int i;

    size = 0;
    alloc = NULL;
    for (i = 0; i < 3; i++) {
	instance->frame[i].wait_completion = 0;
	instance->frame[i].ximage =
	    XShmCreateImage (instance->display, instance->vinfo.visual,
			     instance->vinfo.depth, ZPixmap, NULL /* data */,
			     &(instance->shminfo),
			     instance->width, instance->height);
	if (instance->frame[i].ximage == NULL) {
	    fprintf (stderr, "Cannot create ximage\n");
	    return 1;
	} else if (i == 0) {
	    size = (instance->frame[0].ximage->bytes_per_line * 
		    instance->frame[0].ximage->height);
	    alloc = create_shm (instance, 3 * size);
	    if (alloc == NULL)
		return 1;
	} else if (size != (instance->frame[0].ximage->bytes_per_line *
			    instance->frame[0].ximage->height)) {
	    fprintf (stderr, "unexpected ximage data size\n");
	    return 1;
	}

	instance->frame[i].ximage->data = (char *) alloc;
	alloc += size;
    }

#ifdef WORDS_BIGENDIAN 
    if (instance->frame[0].ximage->byte_order != MSBFirst) {
	fprintf (stderr, "No support for non-native byte order\n");
	return 1;
    }
#else
    if (instance->frame[0].ximage->byte_order != LSBFirst) {
	fprintf (stderr, "No support for non-native byte order\n");
	return 1;
    }
#endif

    /*
     * depth in X11 terminology land is the number of bits used to
     * actually represent the colour.
     *
     * bpp in X11 land means how many bits in the frame buffer per
     * pixel. 
     *
     * ex. 15 bit color is 15 bit depth and 16 bpp. Also 24 bit
     *     color is 24 bit depth, but can be 24 bpp or 32 bpp.
     *
     * If we have blue in the lowest bit then "obviously" RGB
     * (the guy who wrote this convention never heard of endianness ?)
     */

    yuv2rgb_init (((instance->vinfo.depth == 24) ?
		   instance->frame[0].ximage->bits_per_pixel :
		   instance->vinfo.depth),
		  ((instance->frame[0].ximage->blue_mask & 0x01) ?
		   MODE_RGB : MODE_BGR));

    if (libvo_common_alloc_frames ((vo_instance_t *) instance,
				   instance->width, instance->height,
				   sizeof (x11_frame_t), x11_copy_slice,
				   x11_field, x11_draw_frame)) {
	fprintf (stderr, "Can not allocate yuv backing buffers\n");
	return 1;
    }

    return 0;
}

static void x11_close (vo_instance_t * _instance)
{
    x11_instance_t * instance = (x11_instance_t *) _instance;
    int i;

    libvo_common_free_frames ((vo_instance_t *) instance);
    for (i = 0; i < 3; i++) {
	while (instance->frame[i].wait_completion)
	    x11_event (instance);
	XDestroyImage (instance->frame[i].ximage);
    }
    destroy_shm (instance);
    XFreeGC (instance->display, instance->gc);
    if (-1 == window_id)
      XDestroyWindow (instance->display, instance->window);
    XCloseDisplay (instance->display);
}

#ifdef LIBVO_XV
static void xv_event (x11_instance_t * instance)
{
    XEvent event;
    char * addr;
    int i;

    XNextEvent (instance->display, &event);
    if (event.type == instance->completion_type) {
	addr = (instance->shminfo.shmaddr +
		((XShmCompletionEvent *)&event)->offset);
	for (i = 0; i < 3; i++)
	    if (addr == instance->frame[i].xvimage->data)
		instance->frame[i].wait_completion = 0;
    }
}

static vo_frame_t * xv_get_frame (vo_instance_t * _instance, int flags)
{
    x11_instance_t * instance;
    x11_frame_t * frame;

    instance = (x11_instance_t *) _instance;
    frame = (x11_frame_t *) libvo_common_get_frame ((vo_instance_t *) instance,
						    flags);

    while (frame->wait_completion)
	xv_event (instance);

    return (vo_frame_t *) frame;
}

static void xv_draw_frame (vo_frame_t * _frame)
{
    x11_frame_t * frame;
    x11_instance_t * instance;

    frame = (x11_frame_t *) _frame;
    instance = (x11_instance_t *) frame->vo.instance;

    overlay_draw(instance, frame);

    XvShmPutImage (instance->display, instance->port, instance->window,
		   instance->gc, frame->xvimage, 0, 0,
		   instance->width, instance->height,
		   instance->corner_x, instance->corner_y,
		   instance->displaywidth, instance->displayheight, True);
    XFlush (instance->display);
    frame->wait_completion = 1;
    frame_rate_delay();
}

static int xv_check_yv12 (x11_instance_t * instance, XvPortID port)
{
    XvImageFormatValues * formatValues;
    int formats;
    int i;

    formatValues = XvListImageFormats (instance->display, port, &formats);
    for (i = 0; i < formats; i++)
	if ((formatValues[i].id == FOURCC_YV12) &&
	    (! (strcmp (formatValues[i].guid, "YV12")))) {
	    XFree (formatValues);
	    return 0;
	}
    XFree (formatValues);
    return 1;
}

static int xv_check_extension (x11_instance_t * instance)
{
    unsigned int version;
    unsigned int release;
    unsigned int dummy;
    int adaptors;
    int i;
    unsigned long j;
    XvAdaptorInfo * adaptorInfo;

    if ((XvQueryExtension (instance->display, &version, &release,
			   &dummy, &dummy, &dummy) != Success) ||
	(version < 2) || ((version == 2) && (release < 2))) {
	fprintf (stderr, "No xv extension\n");
	return 1;
    }

    XvQueryAdaptors (instance->display, instance->window, (unsigned int *) &adaptors,
		     &adaptorInfo);

    for (i = 0; i < adaptors; i++)
	if (adaptorInfo[i].type & XvImageMask)
	    for (j = 0; j < adaptorInfo[i].num_ports; j++)
		if ((! (xv_check_yv12 (instance,
				       adaptorInfo[i].base_id + j))) &&
		    (XvGrabPort (instance->display, adaptorInfo[i].base_id + j,
				 0) == Success)) {
		    instance->port = adaptorInfo[i].base_id + j;
		    XvFreeAdaptorInfo (adaptorInfo);
		    return 0;
		}

    XvFreeAdaptorInfo (adaptorInfo);
    fprintf (stderr, "Cannot find xv port\n");
    return 1;
}

static int xv_alloc_frames (x11_instance_t * instance)
{
    int size;
    uint8_t * alloc;
    int i;

    size = instance->width * instance->height / 4;
    alloc = create_shm (instance, 18 * size);
    if (alloc == NULL)
	return 1;

    for (i = 0; i < 3; i++) {
	instance->frame_ptr[i] = (vo_frame_t *) (instance->frame + i);
	instance->frame[i].vo.base[0] = alloc;
	instance->frame[i].vo.base[1] = alloc + 5 * size;
	instance->frame[i].vo.base[2] = alloc + 4 * size;
	instance->frame[i].vo.copy = NULL;
	instance->frame[i].vo.field = NULL;
	instance->frame[i].vo.draw = xv_draw_frame;
	instance->frame[i].vo.instance = (vo_instance_t *) instance;
	instance->frame[i].wait_completion = 0;
	instance->frame[i].xvimage =
	    XvShmCreateImage (instance->display, instance->port, FOURCC_YV12,
			      (char *) alloc, instance->width, instance->height,
			      &(instance->shminfo));
	if ((instance->frame[i].xvimage == NULL) ||
	    (instance->frame[i].xvimage->data_size != 6 * size)) { /* FIXME */
	    fprintf (stderr, "Cannot create xvimage\n");
	    return 1;
	}
	alloc += 6 * size;
    }

    return 0;
}

static void xv_close (vo_instance_t * _instance)
{
    x11_instance_t * instance = (x11_instance_t *) _instance;
    int i;

    for (i = 0; i < 3; i++) {
	while (instance->frame[i].wait_completion)
	    xv_event (instance);
	XFree (instance->frame[i].xvimage);
    }
    destroy_shm (instance);
    XvUngrabPort (instance->display, instance->port, 0);
    XFreeGC (instance->display, instance->gc);
    XDestroyWindow (instance->display, instance->window);
    XCloseDisplay (instance->display);
}
#endif

static int common_setup (x11_instance_t * instance, int width, int height,
			 int xv)
{
    instance->width = width;
    instance->height = height;

    if (open_display (instance))
	return 1;

    fifo_init();

#ifdef LIBVO_XV
    if (xv && (! (xv_check_extension (instance)))) {
	if (xv_alloc_frames (instance))
	    return 1;
	instance->vo.close = xv_close;
	instance->vo.get_frame = xv_get_frame;
    } else
#endif
    {
	if (x11_alloc_frames (instance))
	    return 1;
	instance->vo.close = x11_close;
	instance->vo.get_frame = x11_get_frame;
    }

    XMapWindow (instance->display, instance->window);

    return 0;
}

static int x11_setup (vo_instance_t * instance, int width, int height)
{
    return common_setup ((x11_instance_t *) instance, width, height, 0);
}

vo_instance_t * vo_x11_open (void)
{
    x11_instance_t * instance;

    instance = malloc (sizeof (x11_instance_t));
    if (instance == NULL)
	return NULL;

    instance->vo.setup = x11_setup;
    return (vo_instance_t *) instance;
}

#ifdef LIBVO_XV
static int xv_setup (vo_instance_t * instance, int width, int height)
{
    return common_setup ((x11_instance_t *) instance, width, height, 1);
}

vo_instance_t * vo_xv_open (void)
{
    x11_instance_t * instance;

    instance = malloc (sizeof (x11_instance_t));
    if (instance == NULL)
	return NULL;

    instance->vo.setup = xv_setup;
    return (vo_instance_t *) instance;
}
#endif
#endif
