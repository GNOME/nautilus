NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-eazel-inventory\"		\
	-DSTANDALONE						\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
        -I$(top_srcdir)/components/services/trilobite		\
        -I/usr/include/rpm					\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

noinst_LIBRARIES = libinventory.a libinventory_corba.a

libinventory_a_SOURCES =						\
	eazel-inventory-shared.h					\
	eazel-inventory-shared.c					\
	$(NULL)

## CORBA magic

CORBA_GENERATED =							\
	trilobite-eazel-inventory-common.c				\
	trilobite-eazel-inventory-skels.c				\
	trilobite-eazel-inventory-stubs.c				\
	$(NULL)

$(CORBA_GENERATED): trilobite-eazel-inventory.h

IDLDIR = $(top_srcdir)/components/services/inventory/idl
IDL = $(IDLDIR)/trilobite-eazel-inventory.idl

trilobite-eazel-inventory.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-inventory.h

libinventory_corba_a_SOURCES = 					\
	$(CORBA_GENERATED)					\
	$(NULL)

