NULL =

INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DG_LOG_DOMAIN=\"Nautilus-service-install\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/components/services/install/lib	\
	-I$(top_srcdir)/components/services/trilobite	\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite \
	-I$(top_builddir)/components/services/install/lib \
	-I$(top_builddir)/components/services/trilobite	\
	-I$(top_builddir)/components/services/trilobite/libtrilobite \
	-I$(top_srcdir)/components/services/nautilus-dependent-shared	\
	-I$(top_builddir)/components/services/install/lib \
	-I$(top_builddir)				\
	$(GCONF_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(GNOMEUI_CFLAGS)				\
	$(VFS_CFLAGS)					\
	$(NULL)

bin_PROGRAMS = nautilus-service-install-view

noinst_HEADERS =					\
	nautilus-service-install.c			\
	nautilus-service-install.h			\
	nautilus-service-install-view.h			\
	callbacks.h					\
	forms.h						\
	$(NULL)

nautilus_service_install_view_SOURCES =		        \
	nautilus-service-install-view.c			\
	main.c						\
	callbacks.c					\
	forms.c						\
	$(NULL)

nautilus_service_install_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/components/services/install/lib/libeazelinstall.la \
	$(top_builddir)/components/services/install/lib/libeazelpackagesystem.la \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/nautilus-dependent-shared/libnautilusdepshared.a \
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(OAF_LIBS)					\
	$(AMMONITE_LIBS)				\
	$(VFS_LIBS)					\
	$(RPM_LIBS)					\
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_install.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@


EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)
