NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-eazel-vault\"		\
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

noinst_LIBRARIES = libvault_corba.a

## CORBA magic

CORBA_GENERATED =						\
	trilobite-eazel-vault-common.c					\
	trilobite-eazel-vault-skels.c					\
	trilobite-eazel-vault-stubs.c					\
	$(NULL)

$(CORBA_GENERATED): trilobite-eazel-vault.h

IDLDIR = $(top_srcdir)/components/services/vault/idl
IDL = $(IDLDIR)/trilobite-eazel-vault.idl

trilobite-eazel-vault.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-vault.h

libvault_corba_a_SOURCES = 					\
	$(CORBA_GENERATED)					\
	$(NULL)

