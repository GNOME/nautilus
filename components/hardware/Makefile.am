NULL=

SUBDIRS=icons

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Hardware\"		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)					\
	$(NAUTILUS_PRIVATE_CFLAGS) \
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
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

EXTRA_DIST = $(oaf_DATA) $(oaf_in_files)

