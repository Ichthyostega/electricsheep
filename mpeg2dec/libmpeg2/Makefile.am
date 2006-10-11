AUTOMAKE_OPTIONS = no-dependencies

AM_CFLAGS = @LIBMPEG2_CFLAGS@

noinst_LIBRARIES = libmpeg2.a
libmpeg2_a_SOURCES = decode.c header.c stats.c cpu_state.c slice.c \
		      motion_comp.c motion_comp_mlib.c motion_comp_mmx.c \
		      motion_comp_altivec.c \
		      idct.c idct_mlib.c idct_mmx.c idct_altivec.c

libmpeg2_a_LIBADD = @LIBMPEG2_LIBS@

EXTRA_DIST = configure.incl vlc.h mpeg2_internal.h
