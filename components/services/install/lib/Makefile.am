NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 			\
	-DG_LOG_DOMAIN=\"eazel-install\"	\
	-DSTANDALONE				\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

noinst_LIBRARIES = libinstall.a libinstall_corba.a

libinstall_a_SOURCES =		        	\
	eazel-install-types.h	 		\
	eazel-install-types.c	 		\
	eazel-install-metadata.h		\
	eazel-install-metadata.c		\
	eazel-install-protocols.h		\
	eazel-install-rpm-glue.h		\
	eazel-install-protocols.c		\
	eazel-install-rpm-glue.h		\
	eazel-install-rpm-glue.c		\
	eazel-install-tests.h			\
	eazel-install-tests.c			\
	eazel-install-xml-package-list.h	\
	eazel-install-xml-package-list.c	\
	eazel-install-object.c			\
	eazel-install-public.h			\
	eazel-install-private.h			\
	$(NULL)

## CORBA magic

oafdir = $(datadir)/oaf
oaf_DATA = trilobite-eazel-install-service.oafinfo

CORBA_GENERATED =				\
	trilobite-eazel-install-common.c	\
	trilobite-eazel-install-skels.c		\
	trilobite-eazel-install-stubs.c		\
	$(NULL)

$(CORBA_GENERATED): trilobite-eazel-install.h

IDLDIR = $(top_srcdir)/components/services/install/idl
IDL = $(IDLDIR)/trilobite-eazel-install.idl

trilobite-eazel-install.h: $(IDL)
	$(prefix)/bin/orbit-idl -I $(IDLDIR)  -I${datadir}/idl $(IDL)

eazel-install-object.c : trilobite-eazel-install.h

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-install.h

libinstall_corba_a_SOURCES = 		\
	$(CORBA_GENERATED)		\
	$(NULL)

EXTRA_DIST = \
	$(oaf_DATA)
