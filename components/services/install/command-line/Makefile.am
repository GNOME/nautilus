NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite/lib	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

bin_PROGRAMS = eazel-install

eazel_install_SOURCES =		        	\
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
	eazel-install.c				\
	$(NULL)

eazel_install_LDFLAGS =                                                   \
        $(top_builddir)/components/services/trilobite/lib/libtrilobite.la \
        $(top_builddir)/libnautilus/libnautilus.la                        \
        $(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
        $(top_builddir)/nautilus-widgets/libnautilus-widgets.la           \
        $(top_builddir)/librsvg/librsvg.la                                \
	$(GNOME_LIBS)                                                     \
	$(GCONF_LIBS)                                                     \
	$(GNET_LIBS)                                                      \
	$(RPM_LIBS)                                                       \
	$(NULL)
