# FIXME bugzilla.eazel.com 714: I (Elliot) want the HTML component to be a
# shared library component, not an exe one.

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-HTML\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS)			\
	$(GTKHTML_CFLAGS)			\
	$(BONOBO_CFLAGS)			\
	$(VFS_CFLAGS)				\
	$(LIBWWW_CFLAGS)			\
	-D_VERSION=\"$(VERSION)\"

oafdir = $(datadir)/oaf
oaf_DATA =					\
	ntl-web-browser.oafinfo


bin_PROGRAMS =					\
	ntl-web-browser

ntl_web_browser_SOURCES =		\
	ntl-web-browser.c		\
	glibwww-init.c			\
	glibwww-trans.c			\
	glibwww-callbacks.c		\
	glibwww.h			\
	gnome-dialogs.c

ntl_web_browser_LDADD =				\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(GTKHTML_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)					\
	$(LIBWWW_LIBS)

EXTRA_DIST = $(oaf_DATA)
