NULL=

SUBDIRS=icons

INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DG_LOG_DOMAIN=\"Nautilus-Hardware\"		\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(EEL_CFLAGS)					\
	$(LIBRSVG_CFLAGS)				\
	$(GCONF_CFLAGS)					\
	$(VFS_CFLAGS)					\
	$(BONOBO_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(GNOMEUI_CFLAGS)				\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_hardware.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

bin_PROGRAMS =					\
	nautilus-hardware-view

nautilus_hardware_view_SOURCES =		        \
	nautilus-hardware-view.c			\
	nautilus-hardware-view.h			\
	main.c

nautilus_hardware_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(EEL_LIBS)		\
	$(LIBRSVG_LIBS)		\
	$(BONOBO_LIBS)		\
	$(GNOMEUI_LIBS)		\
	$(GCONF_LIBS)		\
	$(VFS_LIBS)		\
	$(GNORBA_LIBS)

EXTRA_DIST = $(oaf_DATA) $(oaf_in_files)

