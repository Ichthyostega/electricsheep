AUTOMAKE_OPTIONS = no-dependencies

CFLAGS = @CFLAGS@ @MPEG2DEC_CFLAGS@

bin_PROGRAMS = mpeg2dec_onroot
mpeg2dec_onroot_SOURCES = mpeg2dec_onroot.c getopt.c
mpeg2dec_onroot_LDADD = $(top_builddir)/libmpeg2/libmpeg2.a \
		 $(top_builddir)/libvo/libvo.a @LIBVO_LIBS@ \
		 libcpuaccel.a

noinst_LIBRARIES = libcpuaccel.a
libcpuaccel_a_SOURCES = cpu_accel.c


EXTRA_DIST = configure.incl getopt.h
