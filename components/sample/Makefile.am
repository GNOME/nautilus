CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Sample\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-sample-content-view.oafinfo


bin_PROGRAMS =					\
	nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =		\
	nautilus-sample-content-view.c          \
	main.c

nautilus_sample_content_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/librsvg/librsvg.la		\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)

EXTRA_DIST = $(oaf_DATA)
