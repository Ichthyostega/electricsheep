AUTOMAKE_OPTIONS = foreign no-dependencies

SUBDIRS = video_out flame

bin_PROGRAMS = electricsheep

data_DATA = electricsheep-splash-0.tif electricsheep-splash-1.tif electricsheep-smile.tif electricsheep-frown.tif

electricsheep_SOURCES = electricsheep.c
install-data-local:
	${INSTALL} Electricsheep.desktop /usr/share/control-center/.data
	${INSTALL} electricsheep.png /usr/share/control-center/.data
