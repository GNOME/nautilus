NULL=

INCLUDES = -I. -I$(srcdir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)					\
	$(APPLETS_CFLAGS)				\
	$(NULL)

bin_PROGRAMS = nautilus-icon-applet

nautilus_icon_applet_SOURCES = 	\
	nautilus-icon-applet.c	\
	nautilus-icon-applet.h	\
	$(NULL)

nautilus_icon_applet_LDADD =	\
	$(APPLETS_LIBS) 	\
	$(NULL)

EXTRA_DIST = 				\
	nautilus-icon-applet.desktop 	\
	nautilus-icon-applet.gnorba 	\
	nautilus-icon-applet.png	\
	$(NULL)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = nautilus-icon-applet.png
