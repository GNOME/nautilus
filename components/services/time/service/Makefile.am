NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"trilobite-eazel-time-service\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NULL)

INCLUDES = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_srcdir)/components/services/trilobite \
	-I../server/		\
	$(GHTTP_CFLAGS) 		\
	$(GCONF_CFLAGS) 	\
	$(GNOME_CFLAGS) 	\
	$(GNOME_INCLUDEDIR) 	\
	$(OAF_CFLAGS) 		\
	$(XML_CFLAGS) 		\
	$(WERROR) 		\
	$(NULL)

LDFLAGS = \
	$(OAF_LIBS) 	\
	$(GHTTP_LIBS) 	\
	$(NULL)



pixmapdir = $(datadir)/pixmaps
pixmap_DATA = trilobite-eazel-time-service.png

# CORBA magic

oafdir = $(datadir)/oaf
oaf_DATA = trilobite-eazel-time-service.oafinfo

CORBA_GENERATED = \
	trilobite-eazel-time-service.h 		\
	trilobite-eazel-sample-service-common.c	\
	trilobite-eazel-time-service-skels.c	\
	trilobite-eazel-time-service-stubs.c	\
	trilobite-eazel-time-service-common.c	\
	$(NULL)


IDLDIR = $(top_srcdir)/components/services/time/idl
IDL = $(IDLDIR)/trilobite-eazel-time-service.idl

$(CORBA_GENERATED): IDL_COMPILE

IDL_COMPILE: $(IDL)
	$(prefix)/bin/orbit-idl -I $(IDLDIR)  -I${datadir}/idl $(IDL)

trilobite-eazel-time-service.c main.c: $(CORBA_GENERATED)

CLEANFILES+=$(CORBA_GENERATED)

## Binaries

bin_PROGRAMS = trilobite-eazel-time-service

trilobite_eazel_time_service_SOURCES =		\
	trilobite-eazel-time-service.c		\
	trilobite-eazel-time-service-public.h	\
	trilobite-eazel-time-service-private.h	\
	main.c					\
	trilobite-eazel-time-service-common.c	\
	trilobite-eazel-time-service-skels.c	\
	$(NULL)

trilobite_eazel_time_service_LDADD =	\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(NULL)


## Extra stuff

EXTRA_DIST = \
	$(oaf_DATA)		\
	$(pixmap_DATA) 		\
	$(NULL)
