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

noinst_LIBRARIES = libinstall_gtk.a libinstall_base.a libinstall_corba.a

noinst_HEADERS = \
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

libinstall_gtk_a_SOURCES =			\
	eazel-install-object.c			\
	$(NULL)


libinstall_gtk_a_CPPFLAGS = 			\
	-DEAZEL_INSTALL_NO_CORBA		\
	$(NULL)

libinstall_base_a_SOURCES =		       	\
	eazel-install-types.c	 		\
	eazel-install-metadata.c		\
	eazel-install-protocols.c		\
	eazel-install-rpm-glue.c		\
	eazel-install-tests.c			\
	eazel-install-xml-package-list.c	\
	eazel-install-object.c			\
	eazel-install-public.h			\
	$(NULL)

## CORBA magic

CORBA_GENERATED =				\
	trilobite-eazel-install-common.c	\
	trilobite-eazel-install-skels.c		\
	trilobite-eazel-install-stubs.c		\
	$(NULL)

eazel-install-public.h $(CORBA_GENERATED): trilobite-eazel-install.h

IDLDIR = $(top_srcdir)/components/services/install/idl
IDL = $(IDLDIR)/trilobite-eazel-install.idl

trilobite-eazel-install.h: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)

eazel-install-types.c : trilobite-eazel-install.h

CLEANFILES+=$(CORBA_GENERATED) trilobite-eazel-install.h

libinstall_corba_a_SOURCES = 		\
	eazel-install-corba.c		\
	eazel-install-object.c		\
	eazel-install-corba-types.c	 	\
	eazel-install-corba-callback.c	\
	$(CORBA_GENERATED)		\
	$(NULL)


