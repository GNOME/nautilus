include $(top_srcdir)/Makefile.shared

SUBDIRS = icons

INCLUDES = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"eazel-service\"			\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/components/services/nautilus-dependent-shared	\
        -I$(top_srcdir)/components/services/trilobite		\
        -I$(top_builddir)/components/services/trilobite		\
        -I$(top_srcdir)/components/services/trilobite/lib	\
        -I$(top_builddir)/components/services/trilobite/lib	\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(NULL)

noinst_LIBRARIES = libnautilusdepshared.a

noinst_HEADERS =\
	eazel-services-extensions.h \
	eazel-services-footer.h \
	eazel-services-header.h \
	$(NULL)

libnautilusdepshared_a_SOURCES =\
	eazel-services-extensions.c \
	eazel-services-footer.c \
	eazel-services-header.c \
	$(NULL)

test_footer_header_LDADD = \
	$(top_builddir)/components/services/nautilus-dependent-shared/libnautilusdepshared.a \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-footer-header \
	$(NULL)

test_footer_header_SOURCES = test-footer-header.c

