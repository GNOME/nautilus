NULL=

INCLUDES = -I. -I$(srcdir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)					\
	$(APPLETS_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DBINDIR=\""$(bindir)"\" 			\
	$(NULL)

bin_PROGRAMS = nautilus-icon-applet

nautilus_icon_applet_SOURCES = 	\
	nautilus-icon-applet.c	\
	nautilus-icon-applet.h	\
	$(NULL)

nautilus_icon_applet_LDADD =	\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(APPLETS_LIBS) 	\
	$(NULL)

foo =\
	$(top_builddir)/libnautilus/libnautilus.la 				\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS) 			\
	$(OAF_LIBS) 			\
	$(GCONF_LIBS) 			\
	$(GNOMEUI_LIBS) 		\
	$(VFS_LIBS) 			\
	$(XML_LIBS) 			\
	$(NULL)

EXTRA_DIST = 				\
	nautilus-icon-applet.desktop 	\
	nautilus-icon-applet.gnorba 	\
	nautilus-icon-applet.png	\
	$(NULL)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = nautilus-icon-applet.png
