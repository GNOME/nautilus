NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-eazel-summary\"		\
	-DSTANDALONE						\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
        -I$(top_srcdir)/components/services/trilobite		\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(NULL)

noinst_LIBRARIES = libsummary_corba.a

## CORBA magic

CORBA_GENERATED =							\
	trilobite-eazel-summary-common.c				\
	trilobite-eazel-summary-skels.c					\
	trilobite-eazel-summary-stubs.c					\
	$(NULL)

$(CORBA_GENERATED): trilobite-eazel-summary.h

IDLDIR = $(top_srcdir)/components/services/summary/idl
IDL = $(IDLDIR)/trilobite-eazel-summary.idl

trilobite-eazel-summary.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-summary.h

libsummary_corba_a_SOURCES = 					\
	$(CORBA_GENERATED)					\
	$(NULL)

