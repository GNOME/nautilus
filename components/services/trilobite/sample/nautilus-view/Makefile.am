NULL =

SUBDIRS=icons

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/components/services/trilobite/lib	\
	-I$(top_builddir)					\
	$(GNOMEUI_CFLAGS)                       		\
	$(GCONF_CFLAGS) 					\
	$(WERROR)						\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =		\
	nautilus-eazel-sample-service-view.oafinfo


bin_PROGRAMS =		\
	nautilus-eazel-sample-service-view

nautilus_eazel_sample_service_view_SOURCES =		\
	nautilus-eazel-sample-service-view.h		\
	nautilus-eazel-sample-service-view.c    	\
	main.c						\
	$(NULL)

nautilus_eazel_sample_service_view_LDFLAGS =			\
	$(top_builddir)/components/services/trilobite/lib/libtrilobite.la \
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/librsvg/librsvg.la		\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS)					\
	$(VFS_LIBS)					\
	-lghttp						\
	$(OAF_LIBS)					\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
