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
	$(GHTTP_CFLAGS)						\
	$(WERROR)						\
	$(NULL)

bin_PROGRAMS = eazel-alt-install-corba

eazel_alt_install_corba_SOURCES = 	\
	eazel-alt-install-corba.c		\
	$(NULL)

eazel_alt_install_corba_LDADD =                                           \
        $(top_builddir)/components/services/install/lib/libinstall_corba.a\
        $(top_builddir)/components/services/install/lib/libinstall_base.a \
        $(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(GNOME_LIBS)                                                     \
	$(GCONF_LIBS)                                                     \
	$(GHTTP_LIBS)                                                     \
	$(RPM_LIBS)                                                       \
	$(NULL)
