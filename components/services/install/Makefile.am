NULL=

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I/usr/include/rpm 			\
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)				\
	$(NULL)

gnorbadir = $(sysconfdir)/CORBA/servers

bin_PROGRAMS =					\
	eazel-install

eazel_install_SOURCES =		\
	eazel-install-lib-util.c	\
	eazel-install-lib-debug.c	\
	eazel-install-lib-xml.c		\
	eazel-install-lib-rpm.c		\
	eazel-install.c			\
	$(NULL)

eazel_install_LDFLAGS =			\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) \
	-lrpm						\
	-lpopt						\
	-lxml						\
	-lgnet						\
	$(NULL)

