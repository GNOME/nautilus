PRINT_LIBS = -lbonobo-print

OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)

bin_PROGRAMS = nautilus-image-view

nautilus_image_view_SOURCES =	\
	nautilus-image-view.c	\
	io-png.c		\
	io-png.h

INCLUDES =	\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/libnautilus	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)	\
	$(GCONF_CFLAGS)		\
	$(BONOBO_CFLAGS)	\
	$(VFS_CFLAGS)

nautilus_image_view_LDADD = 			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

oafdir = $(datadir)/oaf
oaf_DATA = nautilus-image-view.oafinfo

EXTRA_DIST = $(oaf_DATA)
