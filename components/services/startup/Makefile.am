CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I/usr/include/rpm 			\
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

gnorbadir = $(sysconfdir)/CORBA/servers

gnorba_DATA =					\
	nautilus-service-startup-view.goad


bin_PROGRAMS =					\
	nautilus-service-startup-view

nautilus_service_startup_view_SOURCES =		\
	nautilus-service-startup-view.c         \
	eazel-register.c			\
	main.c

nautilus_service_startup_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(VFS_LIBS)					\
	-lrpm						\
	-lghttp						\
	$(GNORBA_LIBS)

EXTRA_DIST = nautilus-service-startup-view.goad
