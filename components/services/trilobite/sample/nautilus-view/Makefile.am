NULL =

SUBDIR = icons

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"					\
	 -DG_LOG_DOMAIN=\"trilobite-sample-service\"		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/trilobite/sample/lib	\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =								\
	nautilus-sample-service-view.oafinfo


bin_PROGRAMS =								\
	nautilus-sample-service-view

nautilus_sample_service_view_SOURCES =				\
	nautilus-sample-service-view.h				\
	nautilus-sample-service-view.c    			\
	main.c								\
	$(NULL)

nautilus_sample_service_view_LDADD =				\
	$(top_builddir)/components/services/trilobite/sample/lib/libsample_corba.a \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS)					\
	$(VFS_LIBS)					\
	$(GHTTP_LIBS)					\
	$(OAF_LIBS)					\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
