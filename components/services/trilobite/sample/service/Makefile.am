NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"nautilus-eazel-sample-service\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NULL)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/components/services/trilobite/src \
	$(GNET_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOME_CFLAGS) \
	$(GNOME_INCLUDEDIR) \
	$(OAF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	$(NULL)

LDFLAGS = \
	$(GLIB_LIBS)	\
	$(OAF_LIBS) 	\
	$(NULL)


# CORBA magic

oafdir = $(datadir)/oaf

oaf_DATA = nautilus-eazel-sample-service.oafinfo

CORBA_GENERATED =	\
	sample-service-common.c	\
	sample-service-skels.c	\
	$(NULL)

$(CORBA_GENERATED): sample-service.h

IDLDIR = $(top_srcdir)/components/services/trilobite/idl
IDL = $(IDLDIR)/sample-service.idl

sample-service.h: 
	$(prefix)/bin/orbit-idl -I $(IDLDIR) $(IDL)

sample-service-impl.c: sample-service.h

CLEANFILES+=$(CORBA_GENERATED) sample-service.h

## Binaries

bin_PROGRAMS = nautilus-eazel-sample-service

nautilus_eazel_sample_service_SOURCES =	\
	sample-service-impl.c	\
	sample-service-common.c	\
	sample-service-stubs.c	\
	$(NULL)

nautilus_eazel_sample_service_LDADD =	\
	$(top_builddir)/components/services/trilobite/src/libtrilobited.la \
	$(NULL)

EXTRA_DIST = \
	$(oaf_DATA)		\
	$(NULL)
