NULL =

lib_LTLIBRARIES = libtrilobite.la libtrilobite-service.la

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 			\
	-DG_LOG_DOMAIN=\"libtrilobite\"	\
	-DSTANDALONE				\
	$(NULL)

INCLUDES = \
	-I$(srcdir)		\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I/usr/include/rpm	\
	$(GNOME_CFLAGS)		\
	$(GCONF_CFLAGS)		\
	$(OAF_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(NULL)

libtrilobite_la_LDFLAGS =	\
	$(GNOME_LIBS)		\
	$(OAF_LIBS)		\
	$(GCONF_LIBS)		\
	$(BONOBO_LIBS)		\
	$(XML_LIBS)		\
	$(NULL)

libtrilobite_service_la_LDFLAGS =\
	$(GNOME_LIBS)		\
	$(GNOMEUI_LIBS)		\
	$(OAF_LIBS)		\
	$(GCONF_LIBS)		\
	$(BONOBO_LIBS)		\
	$(XML_LIBS)		\
	$(NULL)

libtrilobiteincludedir = $(includedir)/libtrilobite
libtrilobite_serviceincludedir = $(includedir)/libtrilobite

TRILOBITE_CORBA_GENERATED =		\
	trilobite-service.h		\
	trilobite-service-stubs.c	\
	trilobite-service-common.c	\
	trilobite-service-skels.c	\
	$(NULL)

libtrilobiteinclude_HEADERS =			\
	libtrilobite.h				\
	trilobite-core-messaging.h		\
	trilobite-core-distribution.h		\
	trilobite-core-metadata-manager.h	\
	trilobite-core-network.h		\
	trilobite-core-package-manager.h	\
	trilobite-core-security-manager.h	\
	trilobite-core-tests.h			\
	trilobite-core-utils.h			\
	trilobite-root-helper.h			\
	trilobite-root-client-public.h		\
	trilobite-root-client-private.h		\
	helixcode-utils.h			\
	$(NULL)

libtrilobite_la_SOURCES =			\
	$(TRILOBITE_CORBA_GENERATED)		\
	trilobite-core-messaging.c		\
	trilobite-core-distribution.c		\
	trilobite-core-metadata-manager.c	\
	trilobite-core-network.c		\
	trilobite-core-package-manager.c	\
	trilobite-core-security-manager.c	\
	trilobite-core-tests.c			\
	trilobite-core-utils.c			\
	trilobite-root-helper.c			\
	trilobite-root-client.c			\
	helixcode-utils.c			\
	$(NULL)

libtrilobite_serviceinclude_HEADERS =		\
	libtrilobite-service.h			\
	trilobite-service.h			\
	trilobite-service-public.h		\
	trilobite-service-private.h		\
	trilobite-service-passwordquery-public.h	\
	trilobite-service-passwordquery-private.h	\
	$(NULL)

libtrilobite_service_la_SOURCES =		\
	trilobite-service.c			\
	trilobite-service-passwordquery.c	\
	$(NULL)

$(TRILOBITE_CORBA_GENERATED): idl_stamp

IDL = $(top_srcdir)/components/services/trilobite/idl/trilobite-service.idl

idl_stamp: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(IDL)
	touch idl_stamp

CLEANFILES= $(TRILOBITE_CORBA_GENERATED) idl_stamp

trilobite-service.c: trilobite-service.h
