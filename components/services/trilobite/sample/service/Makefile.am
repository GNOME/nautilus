NULL =

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-sample-service\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_builddir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/components/services/trilobite/sample/lib	\
	-I$(top_srcdir)/components/services/trilobite/sample/lib	\
	-I../server/							\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(NULL)

bin_PROGRAMS = trilobite-eazel-sample-service

trilobite_eazel_sample_service_SOURCES =			\
	sample-service-impl.c					\
	main.c							\
	$(NULL)

trilobite_eazel_sample_service_LDADD =				\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite-service.la \
	$(top_builddir)/components/services/trilobite/sample/lib/libsample_corba.a \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS) \
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files = Trilobite_Service_sample.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST =					\
	$(oaf_in_files)				\
	$(oaf_DATA)				\
	$(NULL)



