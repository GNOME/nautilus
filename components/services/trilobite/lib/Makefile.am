NULL =

lib_LTLIBRARIES = libtrilobite.la

INCLUDES = \
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I/usr/include/rpm	\
	$(GNOME_CFLAGS)		\
	$(GNET_CFLAGS)		\
	$(GCONF_CFLAGS)		\
	$(OAF_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(WERROR)		\
	$(NULL)

libtrilobite_la_LDFLAGS =	\
	$(GNOME_LIBS)		\
	$(GNOMEUI_LIBS)		\
	$(OAF_LIBS)		\
	$(GCONF_LIBS)		\
	$(GNET_LIBS)		\
	$(BONOBO_LIBS)		\
	$(RPM_LIBS)		\
	$(NULL)

libtrilobiteincludedir = $(includedir)/libtrilobite

libtrilobiteinclude_HEADERS =		\
	eazel-register.h		\
	eazel-install-metadata.h	\
	eazel-install-protocols.h	\
	eazel-install-rpm-glue.h	\
	eazel-install-tests.h		\
	eazel-install-types.h		\
	eazel-install-utils.h		\
	eazel-install-xml-package-list.h	\
	helixcode-install-utils.h	\
	$(NULL)

libtrilobite_la_SOURCES =		\
	eazel-register.c		\
	eazel-install-metadata.c	\
	eazel-install-protocols.c	\
	eazel-install-rpm-glue.c	\
	eazel-install-tests.c		\
	eazel-install-utils.c		\
	eazel-install-xml-package-list.c	\
	helixcode-install-utils.c	\
	$(NULL)

