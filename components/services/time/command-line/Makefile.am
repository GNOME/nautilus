NULL =

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"trilobite-eazel-time-service-cli\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/components/services/trilobite/src \
	-I$(top_srcdir)/components/services/trilobite/lib \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS)				\
	$(WERROR)				\
	$(NULL)



oafdir = $(datadir)/oaf

# CORBA magic

CORBA_GENERATED =	\
	time-service-stubs.c	\
	time-service-common.c	\
	time-service-skels.c	\
	$(NULL)

CORBA_HEADER = trilobite-eazel-time-service.h

$(CORBA_GENERATED): $(CORBA_HEADER)

IDLDIR = $(top_srcdir)/components/services/time/idl
IDL = $(IDLDIR)/trilobite-eazel-time-service.idl

$(CORBA_HEADER): $(IDL)
	$(prefix)/bin/orbit-idl -I${datadir}/idl $(IDL)

main.c $(CORBA_GENERATED): $(CORBA_HEADER)

CLEANFILES+=$(CORBA_GENERATED) $(CORBA_HEADER)

## Binaries

bin_PROGRAMS =					\
	trilobite-eazel-time-service-cli

trilobite_eazel_time_service_cli_SOURCES =	\
	main.c					\
	trilobite-eazel-time-service-common.c	\
	trilobite-eazel-time-service-stubs.c	\
	$(NULL)

trilobite_eazel_time_service_cli_LDFLAGS =	\
	$(BONOBO_LIBS)				\
	$(GNOMEUI_LIBS)				\
	$(GCONF_LIBS)				\
	$(VFS_LIBS)				\
	$(OAF_LIBS)				\
	$(NULL)

trilobite_eazel_time_service_cli_LDADD = 	\
	$(top_builddir)/components/services/trilobite/src/libtrilobited.la \
	$(top_builddir)/components/services/trilobite/lib/libtrilobite.la \
	$(NULL)

