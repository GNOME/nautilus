NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"trilobite-eazel-time-service\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NULL)

INCLUDES = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_srcdir)/components/services/trilobite/lib \
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

CORBA_GENERATED =	\
	trilobite-eazel-sample-service-common.c	\
	trilobite-eazel-time-service-skels.c	\
	$(NULL)

$(CORBA_GENERATED): trilobite-eazel-time-service.h

IDLDIR = $(top_srcdir)/components/services/time/idl
IDL = $(IDLDIR)/trilobite-eazel-time-service.idl

trilobite-eazel-time-service.h: $(IDL)
	$(prefix)/bin/orbit-idl -I $(IDLDIR)  -I${datadir}/idl $(IDL)

trilobite-eazel-time-service.c main.c: trilobite-eazel-time-service.h

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-time-service.h

## Binaries

bin_PROGRAMS = trilobite-eazel-time-service

trilobite_eazel_time_service_SOURCES =		\
	trilobite-eazel-time-service.c		\
	main.c					\
	trilobite-eazel-time-service-common.c	\
	trilobite-eazel-time-service-skels.c	\
	$(NULL)

trilobite_eazel_time_service_LDADD =	\
	$(top_builddir)/components/services/trilobite/lib/libtrilobite.la \
	$(NULL)


## Extra stuff

EXTRA_DIST = \
	$(oaf_DATA)		\
	$(pixmap_DATA) 		\
	$(NULL)
