NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-sample-service\"		\
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

noinst_LIBRARIES = libsample_corba.a

## CORBA magic

CORBA_GENERATED =						\
	sample-service.h					\
	sample-service-common.c					\
	sample-service-skels.c					\
	sample-service-stubs.c					\
	$(NULL)

$(CORBA_GENERATED): idl_stamp

IDLDIR = $(top_srcdir)/components/services/trilobite/sample/idl
IDL = $(IDLDIR)/sample-service.idl

idl_stamp: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)
	touch idl_stamp

CLEANFILES+=$(CORBA_GENERATED) idl_stamp

noinst_HEADERS =		\
	sample-service-public.h	\
	$(NULL)

libsample_corba_a_SOURCES = 					\
	$(CORBA_GENERATED)					\
	$(NULL)

