SUBDIRS=icons

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Hardware\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GCONF_CFLAGS) 			\
	$(VFS_CFLAGS) 				\
	$(BONOBO_CFLAGS)			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-hardware-view.oafinfo


bin_PROGRAMS =					\
	nautilus-hardware-view

nautilus_hardware_view_SOURCES =		        \
	nautilus-hardware-view.c			\
	nautilus-hardware-view.h			\
	main.c

nautilus_hardware_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

EXTRA_DIST = $(oaf_DATA)
