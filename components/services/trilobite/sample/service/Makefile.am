NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/components/services/trilobite/src \
	$(GNET_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(OAF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	$(NULL)

LDFLAGS = \
	$(GLIB_LIBS)	\
	$(OAF_LIBS) 	\
	$(GNOMEUI_LIBS) \
	$(NULL)

bin_PROGRAMS = nautilus-eazel-sample-service

gnorbadir = $(sysconfdir)/CORBA/servers

# CORBA magic

CORBA_GENERATED =	\
	sample-service-stubs.c	\
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

nautilus_eazel_sample_service_SOURCES =	\
	sample-service-impl.c	\
	sample-service-common.c	\
	sample-service-skels.c	\
	$(NULL)

nautilus_eazel_sample_service_LDFLAGS =	\
	$(top_builddir)/components/services/trilobite/src/libtrilobited.la \
	$(NULL)
