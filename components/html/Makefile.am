# FIXME I want the HTML component to be a shared library component, not an exe
# one.

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-HTML\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)			\
	$(GTKHTML_CFLAGS)			\
	$(LIBWWW_CFLAGS)

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA =					\
	ntl-web-browser.goad


bin_PROGRAMS =					\
	ntl-web-browser

ntl_web_browser_SOURCES =		\
	ntl-web-browser.c		\
	glibwww-init.c			\
	glibwww-trans.c			\
	glibwww-callbacks.c		\
	glibwww.h			\
	gnome-dialogs.c

ntl_web_browser_LDFLAGS =				\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(GTKHTML_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)					\
	$(LIBWWW_LIBS)

EXTRA_DIST = ntl-web-browser.goad
