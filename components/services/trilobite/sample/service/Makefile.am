NULL =

CPPFLAGS = 							\
	-DPREFIX=\"$(prefix)\"					\
	-DG_LOG_DOMAIN=\"trilobite-sample-service\"		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_builddir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/components/services/trilobite/sample/lib	\
	-I$(top_srcdir)/components/services/trilobite/sample/lib	\
	-I../server/							\
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(NULL)

LDFLAGS = 							\
	$(GLIB_LIBS)						\
	$(OAF_LIBS) 						\
	$(NULL)


oafdir = $(datadir)/oaf

oaf_DATA = trilobite-eazel-sample-service.oafinfo

bin_PROGRAMS = trilobite-eazel-sample-service

trilobite_eazel_sample_service_SOURCES =			\
	sample-service-impl.c					\
	main.c							\
	$(NULL)

trilobite_eazel_sample_service_LDADD =				\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite-service.la \
	$(top_builddir)/components/services/trilobite/sample/lib/libsample_corba.a \
	$(BONOBO_LIBS) \
	$(NULL)

EXTRA_DIST = 							\
	$(oaf_DATA)						\
	$(NULL)
