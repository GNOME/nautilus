NULL =

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"trilobite-eazel-time-service-cli\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite \
	-I$(top_builddir)/components/services/trilobite/libtrilobite \
	-I$(top_srcdir)/components/services/trilobite \
	-I$(top_builddir)/components/services/trilobite \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS)				\
	$(NULL)



oafdir = $(datadir)/oaf

# CORBA magic

CORBA_GENERATED = \
	trilobite-eazel-time-service.h 		\
	trilobite-eazel-time-service-skels.c	\
	trilobite-eazel-time-service-stubs.c	\
	trilobite-eazel-time-service-common.c	\
	$(NULL)

IDLDIR = $(top_srcdir)/components/services/time/idl
IDL = $(IDLDIR)/trilobite-eazel-time-service.idl

# make the auto-generated files depend on a timestamp file.
# this is to ensure the individual files wait for each other before
# proceeding which is needed in parallel makes

$(CORBA_GENERATED): idl_stamp

idl_stamp: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(IDL)
	touch idl_stamp

main.c: $(CORBA_GENERATED)

CLEANFILES+=$(CORBA_GENERATED) idl_stamp

## Binaries

bin_PROGRAMS =					\
	eazel-time

eazel_time_SOURCES =	\
	main.c					\
	trilobite-eazel-time-service-common.c	\
	trilobite-eazel-time-service-stubs.c	\
	$(NULL)

eazel_time_LDADD =	\
	$(BONOBO_LIBS)				\
	$(GNOMEUI_LIBS)				\
	$(GCONF_LIBS)				\
	$(VFS_LIBS)				\
	$(OAF_LIBS)				\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(NULL)

