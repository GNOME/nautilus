NULL =

SUBDIRS = icons

INCLUDES =								\
	-DPREFIX=\"$(prefix)\" 						\
	 -DG_LOG_DOMAIN=\"trilobite-sample-service\"			\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/trilobite/sample/lib	\
	-I$(top_builddir)						\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	$(NULL)

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
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS)					\
	$(VFS_LIBS)					\
	$(GHTTP_LIBS)					\
	$(OAF_LIBS)					\
	$(NULL)


oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_service-sample.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@


EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)
