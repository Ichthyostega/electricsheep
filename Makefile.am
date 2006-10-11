AUTOMAKE_OPTIONS = foreign no-dependencies

SUBDIRS = flam3 mpeg2dec

AM_CFLAGS = -DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"

bin_PROGRAMS = electricsheep electricsheep-voter

electricsheep_voter_CFLAGS = @X_CFLAGS@ $(AM_CFLAGS)
electricsheep_voter_LDADD = @X_LIBS@ -lX11 @X_EXTRA_LIBS@

pkgdata_DATA = electricsheep-smile.png electricsheep-frown.png

electricsheep_SOURCES = electricsheep.c getdate.c
man1_MANS = electricsheep.man
EXTRA_DIST = $(man1_MANS) $(pkgdata_DATA) electricsheep.man.in \
	electricsheep.xml.in electricsheep.spec.in mkinstalldirs getdate.h

SCREENSAVER_DATADIR = $(DESTDIR)/usr/share/xscreensaver/config

install-data-local:
	$(INSTALL) -m 0755 -d $(SCREENSAVER_DATADIR)
	$(INSTALL) -m 0644    electricsheep.xml $(SCREENSAVER_DATADIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(pkgdatadir)
