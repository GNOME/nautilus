NULL =

CPPFLAGS =								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-sample-client\"			\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_builddir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite 	\
	-I$(top_builddir)/components/services/trilobite/libtrilobite 	\
	-I$(top_srcdir)/components/services/trilobite/sample/lib 	\
	-I$(top_builddir)/components/services/trilobite/sample/lib 	\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS)							\
	$(NULL)

bin_PROGRAMS =								\
	eazel-sample-client

eazel_sample_client_SOURCES =						\
	main.c								\
	$(NULL)

eazel_sample_client_LDADD =						\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(OAF_LIBS)							\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/trilobite/sample/lib/libsample_corba.a \
	$(NULL)

