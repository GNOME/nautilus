NULL =

lib_LTLIBRARIES=libtrilobited.la

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"lib-trilobited\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GNET_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(OAF_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	$(NULL)

TRILOBITE_CORBA_GENERATED =		\
	trilobite-service-stubs.c	\
	trilobite-service-common.c	\
	trilobite-service-skels.c	\
	$(NULL)

$(TRILOBITE_CORBA_GENERATED): trilobite-service.h

IDL = $(top_srcdir)/components/services/trilobite/idl/trilobite-service.idl

trilobite-service.h: $(IDL)
	$(prefix)/bin/orbit-idl -I${datadir}/idl $(IDL)

CLEANFILES= $(TRILOBITE_CORBA_GENERATED) trilobite-service.h

trilobite-service.c: trilobite-service.h

libtrilobited_la_LDFLAGS=\
	$(GLIB_LIBS) 	\
	$(OAF_LIBS)	\
	$(BONOBO_LIBS)	\
	$(NULL)

libtrilobitedincludedir=$(includedir)/libtrilobited

libtrilobitedinclude_HEADERS=			\
	trilobite-service.h			\
	$(NULL)

libtrilobited_la_SOURCES=			\
	trilobite-service-common.c		\
	trilobite-service-stubs.c		\
	trilobite-service-skels.c		\
	trilobite-service.c			\
	$(NULL)

gnorbadir = $(sysconfdir)/CORBA/servers

