NULL =

INCLUDES = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-eazel-install-service\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
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

lib_LTLIBRARIES = \
	libeazelinstall.la \
	libeazelpackagesystem.la \
	libeazelsoftcat.la	\
	$(NULL)

# libeazelinstall

libeazelinstallincludedir=$(includedir)/eazel/install

libeazelinstallinclude_HEADERS = 		\
	eazel-install-public.h			\
	trilobite-eazel-install.h		\
	libeazelinstall.h			\
	eazel-install-types.h	 		\
	eazel-install-corba-types.h	 	\
	eazel-install-logic.h			\
	eazel-install-md5.h			\
	eazel-install-metadata.h		\
	eazel-install-protocols.h		\
	eazel-install-rpm-glue.h		\
	eazel-install-rpm-signature.h		\
	eazel-install-query.h			\
	eazel-install-tests.h			\
	eazel-install-xml-package-list.h	\
	eazel-install-private.h			\
	eazel-install-corba-callback.h		\
	eazel-install-problem.h			\
	$(NULL)

libeazelinstall_la_SOURCES = \
	$(CORBA_GENERATED)		\
	eazel-install-corba.c		\
	eazel-install-corba-types.c	\
	eazel-install-corba-callback.c	\
	eazel-install-types.c	 		\
	eazel-install-md5.c			\
	eazel-install-metadata.c		\
	eazel-install-protocols.c		\
	eazel-install-logic.c			\
	eazel-install-rpm-glue.c		\
	eazel-install-rpm-signature.c		\
	eazel-install-query.c			\
	eazel-install-tests.c			\
	eazel-install-xml-package-list.c	\
	eazel-install-object.c			\
	eazel-install-problem.c			\
	$(NULL)

libeazelinstall_la_LDFLAGS = \
	$(GHTTP_LIBS)

# libeazelpackagesystem

libeazelpackagesystemincludedir=$(includedir)/eazel/package-system/

libeazelpackagesysteminclude_HEADERS = 		\
	eazel-install-types.h	 		\
	eazel-package-system.h			\
	eazel-package-system-private.h		\
	$(NULL)

libeazelpackagesystem_la_SOURCES = 		\
	eazel-install-types.o	 		\
	eazel-package-system.o			\
	$(NULL)

libeazelinstall_la_LDFLAGS = \
	$(GHTTP_LIBS)

# libeazelsoftcat

libeazelsoftcatincludedir=$(includedir)/eazel/package-system/

libeazelsoftcatinclude_HEADERS = 		\
	eazel-install-types.h	 		\
	eazel-softcat.h				\
	eazel-softcat-private.h			\
	$(NULL)

libeazelsoftcat_la_SOURCES = 			\
	eazel-install-types.o	 		\
	eazel-softcat.o				\
	$(NULL)

libeazelinstall_la_LDFLAGS = \
	$(GHTTP_LIBS)


# stuff

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

