NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

noinst_LIBRARIES = libinstall.a

libinstall_a_SOURCES =		        	\
	eazel-install-types.h	 		\
	eazel-install-metadata.h		\
	eazel-install-metadata.c		\
	eazel-install-protocols.h		\
	eazel-install-rpm-glue.h		\
	eazel-install-protocols.c		\
	eazel-install-rpm-glue.h		\
	eazel-install-rpm-glue.c		\
	eazel-install-tests.h			\
	eazel-install-tests.c			\
	eazel-install-xml-package-list.h	\
	eazel-install-xml-package-list.c	\
	$(NULL)

