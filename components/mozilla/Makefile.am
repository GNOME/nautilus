NULL = 

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GNOMEUI_CFLAGS) \
	$(WERROR) \
	$(MOZILLA_COMPONENT_CFLAGS) \
	$(NULL)

gnorbadir = $(sysconfdir)/CORBA/servers

gnorba_DATA = \
	nautilus-mozilla-content-view.goad \
	$(NULL)

bin_PROGRAMS =\
	nautilus-mozilla-content-view \
	$(NULL)

nautilus_mozilla_content_view_SOURCES = \
	nautilus-mozilla-content-view.c \
	main.c \
	$(NULL)

nautilus_mozilla_content_view_LDFLAGS = \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(GNORBA_LIBS) \
	$(MOZILLA_COMPONENT_LDFLAGS) \
	$(MOZILLA_COMPONENT_LIBS) \
	$(NULL)

EXTRA_DIST = \
	nautilus-mozilla-content-view.goad \
	$(NULL)

