SUBDIRS=converters

bin_PROGRAMS=hyperbola
lib_LTLIBRARIES=libvfs-help.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOMEUI_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(XML_CFLAGS) \
	-DINFODIR=\"$(infodir)\"

LDADD =\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS)

hyperbola_SOURCES=		\
	hyperbola-nav-tree.c 	\
	hyperbola-main.c	\
	hyperbola-filefmt.c	\
	hyperbola-filefmt.h	\
	hyperbola-nav-index.c	\
	hyperbola-nav-search.c

EXTRA_DIST=pages.map hyperbola.goad help-methods.conf

goaddir=$(sysconfdir)/CORBA/servers
goad_DATA=hyperbola.goad

mapdatadir=$(datadir)/hyperbola/maps
mapdata_DATA=pages.map

module_flags = -export_dynamic -avoid-version
modulesdir = $(libdir)/vfs/modules

libvfs_help_la_SOURCES = help-method.c
libvfs_help_la_LDFLAGS = $(module_flags)
libvfs_help_la_LIBADD  = $(GLIB_LIBS)

modulesconfdir=$(sysconfdir)/vfs/modules
modulesconf_DATA = help-methods.conf
