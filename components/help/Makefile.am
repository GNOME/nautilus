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
	-DPREFIX=\""$(prefix)"\" \
	-DHYPERBOLA_DATADIR=\""$(datadir)/nautilus/components/hyperbola"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DINFODIR=\"$(infodir)\" \
	-DENABLE_SCROLLKEEPER_SUPPORT
	

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

mapdatadir=$(datadir)/nautilus/components/hyperbola/maps
mapdata_DATA=pages.map

module_flags = -export_dynamic -avoid-version
modulesdir = $(libdir)/vfs/modules

libvfs_help_la_SOURCES = help-method.c help-method.h
libvfs_help_la_LDFLAGS = $(module_flags)
libvfs_help_la_LIBADD  = $(GLIB_LIBS)

modulesconfdir=$(sysconfdir)/vfs/modules
modulesconf_DATA = help-methods.conf

hyperboladir=$(datadir)/nautilus/components/hyperbola
hyperbola_DATA=topleveldocs.xml

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_help.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@


EXTRA_DIST=pages.map $(oaf_in_files) $(oaf_DATA) help-methods.conf topleveldocs.xml
