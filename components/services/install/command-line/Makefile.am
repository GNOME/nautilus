NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

## CFLAGS = -O -g
## LDFLAGS = -static

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I/usr/include/rpm					\
        -I$(top_srcdir)/components/services/trilobite 		\
        -I$(top_srcdir)/components/services/install/lib		\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(GNET_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

bin_PROGRAMS = eazel-alt-install

eazel_alt_install_SOURCES = 	\
	eazel-alt-install.c		\
	$(NULL)

eazel_alt_install_LDADD =                                                     \
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(GNOME_LIBS)                                                     \
	$(GCONF_LIBS)                                                     \
	$(GNET_LIBS)                                                      \
	$(GHTTP_LIBS)                                                     \
	$(RPM_LIBS)                                                       \
        $(top_builddir)/components/services/install/lib/libinstall.a      \
	$(NULL)
