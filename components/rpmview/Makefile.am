CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-RPM\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I/usr/include/rpm 			\
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-rpm-view.oafinfo


bin_PROGRAMS =					\
	nautilus-rpm-view

nautilus_rpm_view_SOURCES =		        \
	nautilus-rpm-view.c			\
	nautilus-rpm-view.h			\
	main.c

nautilus_rpm_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/librsvg/librsvg.la		\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	-lrpm						\
	$(VFS_LIBS)					\
	$(OAF_LIBS)

EXTRA_DIST = $(oaf_DATA)
