include $(top_srcdir)/Makefile.shared

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"EazelHelperTest\"				\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
        -I$(top_builddir)/components/services/trilobite/libtrilobite	\
        -I$(top_srcdir)/components/services/trilobite			\
        -I$(top_builddir)/components/services/trilobite			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(GNOME_CFLAGS)							\
	$(GCONF_CFLAGS)							\
	$(VFS_CFLAGS)							\
	$(XML_CFLAGS)							\
	$(GHTTP_CFLAGS)							\
	$(NULL)

LDADD =											\
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la	\
	$(BONOBO_LIBS)									\
	$(GNOME_LIBS)									\
	$(GCONF_LIBS)									\
	$(VFS_LIBS)									\
	$(GHTTP_LIBS)									\
	$(RPM_LIBS)									\
	$(NULL)

bin_PROGRAMS = eazel-helper-test

eazel_helper_test_SOURCES =			\
	eazel-helper-test.c			\
	$(NULL)


