NULL =

CPPFLAGS = 								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-eazel-login\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/login/lib			\
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(WERROR) 							\
	$(NULL)

LDFLAGS = 								\
	$(GLIB_LIBS)							\
	$(OAF_LIBS) 							\
	$(NULL)


oafdir = $(datadir)/oaf

oaf_DATA = trilobite-eazel-login-service.oafinfo

bin_PROGRAMS = trilobite-eazel-login-service

trilobite_eazel_login_service_SOURCES =					\
	trilobite-eazel-login-impl.c					\
	main.c								\
	$(NULL)

trilobite_eazel_login_service_LDADD =					\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/login/lib/liblogin_corba.a	\
	$(NULL)

EXTRA_DIST = 								\
	$(oaf_DATA)							\
	$(NULL)
