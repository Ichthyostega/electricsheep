AUTOMAKE_OPTIONS = foreign no-dependencies

SUBDIRS = flame mpeg2dec

bin_PROGRAMS = electricsheep

data_DATA = electricsheep-splash-0.tif electricsheep-splash-1.tif electricsheep-smile.tif electricsheep-frown.tif

electricsheep_SOURCES = electricsheep.c
man1_MANS = electricsheep.man
EXTRA_DIST = $(man1_MANS) $(data_DATA) electricsheep.xml electricsheep.spec timers.c.patch mkinstalldirs

# first one is for redhat 8, then for redhat 9
install-data-local:
	${INSTALL} electricsheep.xml /usr/X11R6/lib/xscreensaver/config
	${INSTALL} electricsheep.xml /usr/share/control-center/screensavers/electricsheep.xml
