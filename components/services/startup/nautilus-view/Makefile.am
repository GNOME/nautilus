NULL =

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"trilobite-eazel-startup\"

INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite \
	-I$(top_srcdir)/components/services/nautilus-dependant-shared \
	-I$(top_builddir)					\
	$(GNOMEUI_CFLAGS)                       		\
	$(GCONF_CFLAGS) 					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"		\
	$(GHTTP_CFLAGS) 					\
	$(VFS_CFLAGS)	 					\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-service-startup-view.oafinfo


bin_PROGRAMS =					\
	nautilus-service-startup-view

nautilus_service_startup_view_SOURCES =		\
	nautilus-service-startup-view.h		\
	nautilus-service-startup-view.c         \
	main.c					\
	$(NULL)

nautilus_service_startup_view_LDADD =                                     \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/nautilus-dependant-shared/libnautilusdepshared.a \
	$(top_builddir)/libnautilus/libnautilus.la                        \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la   \
	$(BONOBO_LIBS)                                                    \
	$(GNOMEUI_LIBS)                                                   \
	$(GCONF_LIBS)                                                     \
	$(VFS_LIBS)                                                       \
	$(GHTTP_LIBS)                                                     \
	$(OAF_LIBS)                                                       \
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
