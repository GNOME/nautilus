NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-eazel-login\"		\
	-DSTANDALONE						\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
        -I$(top_srcdir)/components/services/trilobite		\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

noinst_LIBRARIES = liblogin_corba.a

## CORBA magic

CORBA_GENERATED =						\
	trilobite-eazel-login-common.c					\
	trilobite-eazel-login-skels.c					\
	trilobite-eazel-login-stubs.c					\
	$(NULL)

$(CORBA_GENERATED): trilobite-eazel-login.h

IDLDIR = $(top_srcdir)/components/services/login/idl
IDL = $(IDLDIR)/trilobite-eazel-login.idl

trilobite-eazel-login.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-login.h

liblogin_corba_a_SOURCES = 					\
	$(CORBA_GENERATED)					\
	$(NULL)

