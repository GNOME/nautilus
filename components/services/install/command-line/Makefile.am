include $(top_srcdir)/Makefile.shared

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"Nautilus-Services\"				\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
        -I$(top_builddir)/components/services/trilobite/libtrilobite	\
        -I$(top_srcdir)/components/services/trilobite			\
        -I$(top_builddir)/components/services/trilobite			\
        -I$(top_srcdir)/components/services/install/lib			\
        -I$(top_builddir)/components/services/install/lib		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOME_CFLAGS)							\
	$(GCONF_CFLAGS)							\
	$(VFS_CFLAGS)							\
	$(XML_CFLAGS)							\
	$(GHTTP_CFLAGS)							\
	$(NULL)

LDADD =											\
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la	\
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS)									\
	$(GNOME_LIBS)									\
	$(GCONF_LIBS)									\
	$(VFS_LIBS)									\
	$(GHTTP_LIBS)									\
	$(RPM_LIBS)									\
	$(NULL)

bin_PROGRAMS = eazel-install eazel-gen-xml eazel-test-packsys eazel-softcat eazel-test-install-types

eazel_install_SOURCES =				\
	eazel-alt-install-corba.c		\
	$(NULL)

eazel_install_LDADD =									\
	$(LDADD)									\
        $(top_builddir)/components/services/install/lib/libeazelinstall.la		\
        $(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
	$(NULL)

eazel_gen_xml_SOURCES =				\
	eazel-gen-xml.c				\
	$(NULL)                

eazel_gen_xml_LDADD =									\
	$(LDADD)									\
        $(top_builddir)/components/services/install/lib/libeazelinstall.la		\
        $(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
	$(NULL)

eazel_test_packsys_SOURCES =			\
	eazel-test-packsys.c			\
	$(NULL)                

eazel_test_packsys_LDADD =								\
	$(LDADD)									\
	$(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
	$(NULL)

#eazel_test_inventory_SOURCES =			\
#	eazel-test-inventory.c			\
#	$(NULL)                

#eazel_test_inventory_LDADD =								\
#	$(LDADD)									\
#	$(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
#	$(NULL)

eazel_test_install_types_SOURCES =		\
	eazel-test-types.c			\
	$(NULL)                

eazel_test_install_types_LDADD =							\
	$(LDADD)									\
        $(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
	$(NULL)

eazel_softcat_SOURCES =				\
	eazel-test-softcat.c			\
	$(NULL)

eazel_softcat_LDADD =									\
	$(LDADD)									\
        $(top_builddir)/components/services/install/lib/libeazelpackagesystem.la	\
	$(NULL)
