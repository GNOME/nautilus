NULL =

lib_LTLIBRARIES = libtrilobite.la

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 			\
	-DG_LOG_DOMAIN=\"libtrilobited\"	\
	-DSTANDALONE				\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I/usr/include/rpm	\
	$(GNOME_CFLAGS)		\
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
	$(BONOBO_LIBS)		\
	$(XML_LIBS)		\
	$(RPM_LIBS)		\
	$(NULL)

libtrilobiteincludedir = $(includedir)/libtrilobite

libtrilobiteinclude_HEADERS =			\
	libtrilobite.h				\
	trilobite-service.h			\
	trilobite-service-public.h		\
	trilobite-service-private.h		\
	trilobite-service-passwordquery-public.h	\
	trilobite-service-passwordquery-private.h	\
	trilobite-core-messaging.h		\
	trilobite-core-distribution.h		\
	trilobite-core-metadata-manager.h	\
	trilobite-core-network.h		\
	trilobite-core-package-manager.h	\
	trilobite-core-security-manager.h	\
	trilobite-core-tests.h			\
	trilobite-core-utils.h			\
	helixcode-utils.h			\
	$(NULL)

libtrilobite_la_SOURCES =			\
	trilobite-service-common.c		\
	trilobite-service-stubs.c		\
	trilobite-service-skels.c		\
	trilobite-service.c			\
	trilobite-service-passwordquery.c	\
	trilobite-core-messaging.c		\
	trilobite-core-distribution.c		\
	trilobite-core-metadata-manager.c	\
	trilobite-core-network.c		\
	trilobite-core-package-manager.c	\
	trilobite-core-security-manager.c	\
	trilobite-core-tests.c			\
	trilobite-core-utils.c			\
	helixcode-utils.c			\
	$(NULL)

TRILOBITE_CORBA_GENERATED =		\
	trilobite-service-stubs.c	\
	trilobite-service-common.c	\
	trilobite-service-skels.c	\
	$(NULL)

$(TRILOBITE_CORBA_GENERATED): trilobite-service.h

IDL = $(top_srcdir)/components/services/trilobite/idl/trilobite-service.idl

trilobite-service.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(IDL)

CLEANFILES= $(TRILOBITE_CORBA_GENERATED) trilobite-service.h

trilobite-service.c: trilobite-service.h
