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
	$(VFS_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(NULL)

LDADD =                                           \
        $(top_builddir)/components/services/install/lib/libeazelinstall.la\
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(BONOBO_LIBS) \
	$(GNOME_LIBS)                   \
	$(GCONF_LIBS)			\
	$(VFS_LIBS)			\
	$(GHTTP_LIBS)			\
	$(RPM_LIBS)			\
	$(NULL)

bin_PROGRAMS = eazel-install eazel-gen-xml

eazel_install_SOURCES = 		\
	eazel-alt-install-corba.c	\
	$(NULL)

eazel_gen_xml_SOURCES = 	\
	eazel-gen-xml.c         \
	$(NULL)                

# Yes, I know these aren't headers, but this is a way to make sure
# that the check-headers-in-Makefile.pl doesn't complain about these
# dead sources. If we get rid of these source files we can also get
# rid of this hack.
noinst_HEADERS = \
	eazel-alt-install.c \
	eazel-install.c \
	eazel-uninstall.c \
	eazel-package-query.c \
	eazel-revert.c \
	$(NULL)
