NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite\
        -I$(top_builddir)/components/services/trilobite/libtrilobite\
        -I$(top_srcdir)/components/services/trilobite 		\
        -I$(top_builddir)/components/services/trilobite 	\
        -I$(top_srcdir)/components/services/install/lib		\
        -I$(top_builddir)/components/services/install/lib	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(NULL)

LDADD =                                           \
        $(top_builddir)/components/services/install/lib/libeazelinstall.la\
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(BONOBO_LIBS) \
	$(GNOME_LIBS)                                                     \
	$(GCONF_LIBS)                                                     \
	$(GHTTP_LIBS)                                                     \
	$(RPM_LIBS)                                                       \
	$(NULL)

bin_PROGRAMS = eazel-install eazel-uninstall eazel-gen-xml eazel-package-query eazel-revert

eazel_install_SOURCES = 		\
	eazel-alt-install-corba.c	\
	$(NULL)

eazel_revert_SOURCES = 		\
	eazel-revert.c	\
	$(NULL)

eazel_uninstall_SOURCES = 	\
	eazel-uninstall.c	\
	$(NULL)

eazel_gen_xml_SOURCES = 	\
	eazel-gen-xml.c		\
	$(NULL)

eazel_package_query_SOURCES = 	\
	eazel-package-query.c	\
	$(NULL)
