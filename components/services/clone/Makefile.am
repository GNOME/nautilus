include $(top_srcdir)/Makefile.shared

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"EazelClone\"					\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
        -I$(top_builddir)/components/services/trilobite/libtrilobite	\
        -I$(top_srcdir)/components/services/trilobite			\
        -I$(top_builddir)/components/services/trilobite			\
        -I$(top_srcdir)/components/services/install/lib			\
        -I$(top_builddir)/components/services/install/lib		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(GNOME_CFLAGS)							\
	$(GCONF_CFLAGS)							\
	$(VFS_CFLAGS)							\
	$(XML_CFLAGS)							\
	$(GHTTP_CFLAGS)							\
	$(NULL)

LDADD =											\
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la	\
        $(top_builddir)/components/services/install/lib/libeazelinstall.la		\
        $(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
	$(top_builddir)/components/services/clone/libeazelinstallclone.la		\
	$(BONOBO_LIBS)									\
	$(GNOME_LIBS)									\
	$(GCONF_LIBS)									\
	$(VFS_LIBS)									\
	$(GHTTP_LIBS)									\
	$(RPM_LIBS)									\
	$(NULL)

bin_PROGRAMS = eazel-clone

eazel_clone_SOURCES =				\
	eazel-clone.c				\
	$(NULL)

lib_LTLIBRARIES = libeazelinstallclone.la

libeazelinstallclone_la_SOURCES =		\
	eazel-clone-lib.h			\
	eazel-clone-lib.c			\
	$(NULL)

