NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite/libtrilobite	\
        -I$(top_srcdir)/components/services/install/lib		\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

bin_PROGRAMS = eazel-install

eazel_install_SOURCES =		        	\
	eazel-install.c				\
	$(NULL)

eazel_install_LDADD =                                                     \
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(GNOME_LIBS)                                                     \
	$(GCONF_LIBS)                                                     \
	$(GNET_LIBS)                                                      \
	$(GHTTP_LIBS)                                                     \
	$(RPM_LIBS)                                                       \
        $(top_builddir)/components/services/install/lib/libinstall.a      \
	$(NULL)
