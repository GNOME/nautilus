include $(top_srcdir)/Makefile.shared

INCLUDES = \
	-DPREFIX=\"$(prefix)\"					\
	-DG_LOG_DOMAIN=\"Nautilus-Services\"			\
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
	$(VFS_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(NULL)

LDADD =                                           \
        $(top_builddir)/components/services/install/lib/libeazelinstall.la	\
	$(top_builddir)/components/services/install/lib/libeazelsoftcat.la	\
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(BONOBO_LIBS) \
	$(GNOME_LIBS)                   \
	$(GCONF_LIBS)			\
	$(VFS_LIBS)			\
	$(GHTTP_LIBS)			\
	$(RPM_LIBS)			\
	$(NULL)

bin_PROGRAMS = eazel-install eazel-gen-xml eazel-test-packsys eazel-softcat

eazel_install_SOURCES = 		\
	eazel-alt-install-corba.c	\
	$(NULL)

eazel_gen_xml_SOURCES = 	\
	eazel-gen-xml.c         \
	$(NULL)                

eazel_test_packsys_SOURCES = 	\
	eazel-test-packsys.c         \
	$(NULL)                

eazel_test_packsys_LDADD = \
	$(top_builddir)/components/services/install/lib/libeazelpackagesystem.la \
	$(top_builddir)/components/services/install/lib/libeazelpackagesystem-rpm3.la \
	$(LDADD) \
	$(NULL)

eazel_softcat_SOURCES =	\
	eazel-test-softcat.c	\
	$(NULL)
