NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 			\
	-DG_LOG_DOMAIN=\"eazel-install\"	\
	$(NULL)

CFLAGS=-g -O

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite		\
        -I$(top_builddir)/components/services/trilobite		\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite\
        -I$(top_builddir)/components/services/trilobite/libtrilobite\
        -I$(top_srcdir)/components/services/trilobite/lib	\
        -I$(top_builddir)/components/services/trilobite/lib	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(NULL)

lib_LTLIBRARIES = libeazelinstall.la

noinst_HEADERS = \
	trilobite-eazel-install.h		\
	libeazelinstall.h			\
	eazel-install-types.h	 		\
	eazel-install-corba-types.h	 	\
	eazel-install-metadata.h		\
	eazel-install-protocols.h		\
	eazel-install-rpm-glue.h		\
	eazel-install-rpm-signature.h		\
	eazel-install-query.h			\
	eazel-install-tests.h			\
	eazel-install-xml-package-list.h	\
	eazel-install-private.h			\
	eazel-install-corba-callback.h		\
	$(NULL)

libeazelinstallincludedir=$(includedir)/eazel-install

libeazelinstallinclude_HEADERS = 		\
	eazel-install-public.h			\
	$(NULL)

libeazelinstall_la_SOURCES = \
	$(CORBA_GENERATED)		\
	eazel-install-corba.c		\
	eazel-install-corba-types.c	\
	eazel-install-corba-callback.c	\
	eazel-install-types.c	 		\
	eazel-install-metadata.c		\
	eazel-install-protocols.c		\
	eazel-install-rpm-glue.c		\
	eazel-install-rpm-signature.c		\
	eazel-install-query.c			\
	eazel-install-tests.c			\
	eazel-install-xml-package-list.c	\
	eazel-install-object.c			\
	$(NULL)

libeazelinstall_la_LDFLAGS = \
	$(GHTTP_LIBS)

EXTRA_DIST = eazel-install-query-lex.l

eazel-install-query-lex.c: $(srcdir)/eazel-install-query-lex.l eazel-install-query-parse.h
	@rm -f $@
	$(LEX) -t $(srcdir)/eazel-install-query-lex.l >$@


## CORBA magic

CORBA_GENERATED =				\
	trilobite-eazel-install-common.c	\
	trilobite-eazel-install-skels.c		\
	trilobite-eazel-install-stubs.c		\
	trilobite-eazel-install.h		\
	$(NULL)

IDLDIR = $(top_srcdir)/components/services/install/idl
IDL = $(IDLDIR)/trilobite-eazel-install.idl

$(CORBA_GENERATED): idl_stamp
idl_stamp: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(IDLDIR) $(IDL)
	touch idl_stamp

CLEANFILES+=$(CORBA_GENERATED) idl_stamp

BUILT_SOURCES = \
	$(CORBA_GENERATED)
##

