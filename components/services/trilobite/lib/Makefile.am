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

libtrilobiteinclude_HEADERS =			\
	libtrilobite.h				\
	trilobite-core-messaging.h		\
	trilobite-core-metadata-manager.h	\
	trilobite-core-network.h		\
	trilobite-core-package-manager.h	\
	trilobite-core-security-manager.h	\
	trilobite-core-tests.h			\
	trilobite-core-utils.h			\
	eazel-register.h			\
	helixcode-utils.h			\
	$(NULL)

libtrilobite_la_SOURCES =			\
	trilobite-core-messaging.c		\
	trilobite-core-metadata-manager.c	\
	trilobite-core-network.c		\
	trilobite-core-package-manager.c	\
	trilobite-core-security-manager.c	\
	trilobite-core-tests.c			\
	trilobite-core-utils.c			\
	eazel-register.c			\
	helixcode-utils.c			\
	$(NULL)

