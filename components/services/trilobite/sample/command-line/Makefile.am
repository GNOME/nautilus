NULL =

CPPFLAGS =								\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"trilobite-eazel-sample-service\"

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/components/services/trilobite 			\
	-I$(top_srcdir)/components/services/trilobite/sample/lib 	\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS)							\
	$(WERROR)							\
	$(NULL)

bin_PROGRAMS =								\
	trilobite-eazel-sample-client

trilobite_eazel_sample_client_SOURCES =					\
	main.c								\
	$(NULL)

trilobite_eazel_sample_client_LDFLAGS =					\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(OAF_LIBS)							\
	$(NULL)

trilobite_eazel_sample_client_LDADD = 					\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/trilobite/sample/lib/libsample_corba.a \
	$(NULL)

