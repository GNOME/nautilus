NULL =

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
	$(EEL_CFLAGS) \
	$(LIBRSVG_CFLAGS) \
	$(GNOMEUI_CFLAGS)	\
	$(GCONF_CFLAGS)		\
	$(BONOBO_CFLAGS)	\
	$(VFS_CFLAGS)

nautilus_image_view_LDADD = 			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)


oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_image.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files)
