CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Sample\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

gnorbadir = $(sysconfdir)/CORBA/servers

gnorba_DATA =					\
	nautilus-sample-content-view.goad


bin_PROGRAMS =					\
	nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =		\
	nautilus-sample-content-view.c          \
	main.c

nautilus_sample_content_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

EXTRA_DIST = nautilus-sample-content-view.goad
