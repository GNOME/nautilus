NULL =

CPPFLAGS =						\
	-DPREFIX=\"$(prefix)\"				\
	-DG_LOG_DOMAIN=\"Nautilus-service-install\"

INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/components/services/install/lib	\
	-I$(top_srcdir)/components/services/trilobite	\
	-I$(top_builddir)/components/services/install/lib \
	-I$(top_builddir)/components/services/trilobite	\
	-I$(top_srcdir)/components/services/nautilus-dependant-shared	\
	-I$(top_builddir)/components/services/install/lib \
	-I$(top_builddir)				\
	-I/usr/include/rpm				\
	$(GCONF_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(GNOMEUI_CFLAGS)				\
	$(VFS_CFLAGS)					\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA = nautilus-service-install-view.oafinfo

bin_PROGRAMS = nautilus-service-install-view

nautilus_service_install_view_SOURCES =		        \
	nautilus-service-install-view.h			\
	nautilus-service-install-view.c			\
	nautilus-service-install.h			\
	nautilus-service-install.c			\
	main.c						\
	$(NULL)

nautilus_service_install_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/components/services/install/lib/libeazelinstall.la \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/nautilus-dependant-shared/libnautilusdepshared.a \
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(OAF_LIBS)					\
	$(VFS_LIBS)					\
	$(RPM_LIBS)					\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
