NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I/usr/include/rpm \
	$(GNET_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(OAF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	$(NULL)

gnorbadir = $(sysconfdir)/CORBA/servers

bin_PROGRAMS = eazel-install

eazel_install_SOURCES =		        	\
	eazel-install-metadata.c		\
	eazel-install-metadata.h		\
	eazel-install-protocols.c		\
	eazel-install-protocols.h		\
	eazel-install-rpm-glue.c		\
	eazel-install-rpm-glue.h		\
	eazel-install-tests.c			\
	eazel-install-tests.h			\
	eazel-install-types.h			\
	eazel-install-utils.c			\
	eazel-install-utils.h			\
	eazel-install-xml-package-list.c	\
	eazel-install-xml-package-list.h	\
	eazel-install.c				\
	helixcode-install-utils.c		\
	helixcode-install-utils.h		\
	$(NULL)

eazel_install_LDFLAGS =	\
	-lgnet \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)	\
	$(OAF_LIBS) \
	$(XML_LIBS) \
	$(GNET_LIBS) \
	$(RPM_LIBS) \
	$(NULL)
