NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 			\
	-DG_LOG_DOMAIN=\"eazel-install\"	\
	$(NULL)

#       -DEAZEL_INSTALL_NO_CORBA                \


INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

noinst_LIBRARIES = libinstall_corba.a libinstall_base.a libinstall_gtk.a

noinst_HEADERS = \
	trilobite-eazel-install.h		\
	eazel-install-types.h	 		\
	eazel-install-corba-types.h	 	\
	eazel-install-metadata.h		\
	eazel-install-protocols.h		\
	eazel-install-rpm-glue.h		\
	eazel-install-rpm-glue.h		\
	eazel-install-tests.h			\
	eazel-install-xml-package-list.h	\
	eazel-install-private.h			\
	eazel-install-corba-callback.h		\
	$(NULL)

libinstall_corba_a_SOURCES = 		\
	$(CORBA_GENERATED)		\
	eazel-install-corba.c		\
	eazel-install-object.c		\
	eazel-install-corba-types.c	 	\
	eazel-install-corba-callback.c	\
	$(NULL)

libinstall_base_a_SOURCES =		       	\
	trilobite-eazel-install.h		\
	eazel-install-types.c	 		\
	eazel-install-metadata.c		\
	eazel-install-protocols.c		\
	eazel-install-rpm-glue.c		\
	eazel-install-tests.c			\
	eazel-install-xml-package-list.c	\
	eazel-install-object.c			\
	eazel-install-public.h			\
	$(NULL)

libinstall_gtk_a_SOURCES =			\
	eazel-install-object.c			\
	$(NULL)

## CORBA magic

CORBA_GENERATED =				\
	trilobite-eazel-install-common.c	\
	trilobite-eazel-install-skels.c		\
	trilobite-eazel-install-stubs.c		\
	$(NULL)

IDLDIR = $(top_srcdir)/components/services/install/idl
IDL = $(IDLDIR)/trilobite-eazel-install.idl

$(CORBA_GENERATED): trilobite-eazel-install.h

trilobite-eazel-install.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-install.h

##

