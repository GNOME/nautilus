NULL =

CPPFLAGS = 								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-eazel-summary\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/summary/lib			\
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(NULL)

LDFLAGS = 								\
	$(GLIB_LIBS)							\
	$(OAF_LIBS) 							\
	$(NULL)


oafdir = $(datadir)/oaf

oaf_DATA = trilobite-eazel-summary-service.oafinfo

bin_PROGRAMS = trilobite-eazel-summary-service

trilobite_eazel_summary_service_SOURCES =					\
	trilobite-eazel-summary-impl.c					\
	main.c								\
	$(NULL)

trilobite_eazel_summary_service_LDADD =					\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/summary/lib/libsummary_corba.a	\
	$(NULL)

EXTRA_DIST = 								\
	$(oaf_DATA)							\
	$(NULL)
