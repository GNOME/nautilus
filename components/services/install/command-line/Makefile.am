NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
        -I$(top_srcdir)/components/services/trilobite/lib       \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(OAF_CFLAGS) \
	$(WERROR) \
	$(NULL)

bin_PROGRAMS = eazel-install

eazel_install_SOURCES =		        	\
	eazel-install.c				\
	$(NULL)

eazel_install_LDFLAGS =	\
        $(top_builddir)/components/services/trilobite/lib/libtrilobite.la \
        $(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
        $(top_builddir)/libnautilus/libnautilus.la      \
        $(top_builddir)/libnautilus-extensions/libnautilus-extensions.la        \
	$(top_builddir)/librsvg/librsvg.la              \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)	\
	$(OAF_LIBS) \
	$(NULL)
