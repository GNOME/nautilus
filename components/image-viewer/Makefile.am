NULL =

bin_PROGRAMS = nautilus-image-view

nautilus_image_view_SOURCES =	\
	nautilus-image-view.c	\
	io-png.c		\
	io-png.h

INCLUDES =	\
	-I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

nautilus_image_view_LDADD = 			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_image.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files)
