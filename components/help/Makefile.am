NULL=

SUBDIRS=converters

bin_PROGRAMS=hyperbola
modules_LTLIBRARIES=libvfs-help.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GNOMEUI_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DHYPERBOLA_DATADIR=\""$(datadir)/hyperbola"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DINFODIR=\"$(infodir)\"

LDADD =\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS)

hyperbola_SOURCES=		\
	hyperbola-nav.h		\
	hyperbola-types.h	\
	hyperbola-nav-tree.c 	\
	hyperbola-main.c	\
	hyperbola-filefmt.c	\
	hyperbola-filefmt.h	\
	hyperbola-nav-index.c	\
	hyperbola-nav-search.c

oafdir=$(datadir)/oaf

# These are broken:
#
#	help-search.oafinfo
#	help-index.oafinfo
#

oaf_DATA=\
	help-factory.oafinfo \
	help-contents.oafinfo \
	$(NULL)

mapdatadir=$(datadir)/hyperbola/maps
mapdata_DATA=pages.map

module_flags = -export_dynamic -avoid-version
modulesdir = $(libdir)/vfs/modules

libvfs_help_la_SOURCES = help-method.c help-method.h
libvfs_help_la_LDFLAGS = $(module_flags)
libvfs_help_la_LIBADD  = $(GLIB_LIBS)

modulesconfdir=$(sysconfdir)/vfs/modules
modulesconf_DATA = help-methods.conf

EXTRA_DIST=pages.map $(oaf_DATA) help-methods.conf

