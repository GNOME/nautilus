# yes, i know its hardcoded

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\"

INCLUDES =					\
	-I/home/ramiro/b/sekhmet/s/mozilla/dist/include \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

gnorbadir = $(sysconfdir)/CORBA/servers

gnorba_DATA =					\
	nautilus-mozilla-content-view.goad


bin_PROGRAMS =					\
	nautilus-mozilla-content-view

nautilus_mozilla_content_view_SOURCES =		\
	nautilus-mozilla-content-view.c          \
	main.c

nautilus_mozilla_content_view_LDFLAGS =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS) \
	-L/home/ramiro/b/sekhmet/s/mozilla/dist/bin \
	-lgtkembedmoz \
	-lgtksuperwin \
	-lxpcom \
	-lnspr4 \
	-lplc4 \
	-lplds4 

EXTRA_DIST = nautilus-mozilla-content-view.goad
