CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Sample\"	\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(BONOBO_CFLAGS)			\
	$(OAF_CFLAGS)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-sample-content-view.oafinfo

bin_PROGRAMS =					\
	nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =		\
	nautilus-sample-content-view.h          \
	nautilus-sample-content-view.c          \
	main.c

nautilus_sample_content_view_LDADD =		\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(GNOMEUI_LIBS)				\
	$(BONOBO_LIBS)				\
	$(OAF_LIBS)

EXTRA_DIST = $(oaf_DATA)
