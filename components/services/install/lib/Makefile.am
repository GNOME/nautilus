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
	libeazelpackagesystem-rpm3.la \
	libeazelpackagesystem-rpm4.la \
	libeazelpackagesystem-skeleton.la \
	libeazelpackagesystem-dpkg.la \
	$(NULL)


# libeazelinstall

libeazelinstallincludedir=$(includedir)/libtrilobite/eazel/install

libeazelinstallinclude_HEADERS = 		\
	eazel-install-public.h			\
	trilobite-eazel-install.h		\
	libeazelinstall.h			\
	eazel-install-corba-types.h	 	\
	eazel-install-metadata.h		\
	eazel-install-protocols.h		\
	eazel-install-rpm-signature.h		\
	eazel-install-query.h			\
	eazel-install-tests.h			\
	eazel-install-private.h			\
	eazel-install-corba-callback.h		\
	eazel-install-problem.h			\
	eazel-install-logic2.h			\
	$(NULL)

libeazelinstall_la_SOURCES = \
	$(CORBA_GENERATED)		\
	eazel-install-corba.c		\
	eazel-install-corba-types.c	\
	eazel-install-corba-callback.c	\
	eazel-install-metadata.c		\
	eazel-install-protocols.c		\
	eazel-install-logic2.c			\
	eazel-install-rpm-signature.c		\
	eazel-install-tests.c			\
	eazel-install-object.c			\
	eazel-install-problem.c			\
	eazel-package-system-rpm3.c		\
	$(NULL)

libeazelinstall_la_LDFLAGS = \
	$(GHTTP_LIBS)

# libeazelpackagesystem

libeazelpackagesystemincludedir=$(includedir)/libtrilobite/eazel/package-system/

libeazelpackagesysteminclude_HEADERS = 		\
	eazel-package-system.h			\
	eazel-package-system-private.h		\
	eazel-package-system-types.h	 	\
	eazel-install-xml-package-list.h	\
	eazel-softcat.h				\
	eazel-softcat-private.h			\
	$(NULL)

libeazelpackagesystem_la_SOURCES = 		\
	eazel-package-system-types.c		\
	eazel-package-system.c			\
	eazel-softcat.c				\
	eazel-install-xml-package-list.c	\
	$(NULL)

# libeazelpackagesystem-skeleton

libeazelpackagesystem_skeleton_la_SOURCES = 	\
	eazel-package-system-skeleton.h		\
	eazel-package-system-skeleton.c		\
	$(NULL)


# libeazelpackagesystem-dpkg

libeazelpackagesystem_dpkg_la_SOURCES = 	\
	eazel-package-system-dpkg.h		\
	eazel-package-system-dpkg.c		\
	$(NULL)


# libeazelpackagesystem-rpm3
libeazelpackagesystem_rpm3_la_SOURCES = 	\
	eazel-package-system-rpm3.h		\
	eazel-package-system-rpm3-private.h	\
	eazel-package-system-rpm3.c		\
	$(NULL)

# libeazelpackagesystem-rpm4
libeazelpackagesystem_rpm4_la_SOURCES = 	\
	eazel-package-system-rpm3.h		\
	eazel-package-system-rpm3.c		\
	eazel-package-system-rpm4.h		\
	eazel-package-system-rpm4.c		\
	$(NULL)

# stuff

EXTRA_DIST = \
	eazel-install-query-lex.l		\
	eazel-package-system-rpm3-private.h	\
	$(NULL)

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

