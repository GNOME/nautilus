NULL =

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"nautilus-eazel-sample-service-cli\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/components/services/trilobite/src \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS)				\
	$(WERROR)				\
	$(NULL)

oafdir = $(datadir)/oaf

# CORBA magic

CORBA_GENERATED =	\
	sample-service-stubs.c	\
	sample-service-common.c	\
	sample-service-skels.c	\
	$(NULL)

$(CORBA_GENERATED): sample-service.h

IDLDIR = $(top_srcdir)/components/services/trilobite/idl
IDL = $(IDLDIR)/sample-service.idl

sample-service.h: $(IDL) $(IDLDIR)/trilobite-service.idl
	$(prefix)/bin/orbit-idl -I $(IDLDIR)  -I${datadir}/idl $(IDL)

main.c: sample-service.h

CLEANFILES+=$(CORBA_GENERATED) sample-service.h

## Binaries

bin_PROGRAMS =					\
	nautilus-eazel-sample-service-cli

nautilus_eazel_sample_service_cli_SOURCES =	\
	sample-service-common.c			\
	sample-service-stubs.c			\
	main.c					\
	$(NULL)

nautilus_eazel_sample_service_cli_LDFLAGS =	\
	$(BONOBO_LIBS)				\
	$(GNOMEUI_LIBS)				\
	$(GCONF_LIBS)				\
	$(VFS_LIBS)				\
	$(OAF_LIBS)				\
	$(NULL)

