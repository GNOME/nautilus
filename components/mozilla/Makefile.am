NULL = 

SUBDIRS = eazel-protocol-handler

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\" \
	$(MOZILLA_COMPONENT_DEBUG_FLAGS) \
	$(NULL)

component_path=$(prefix)/share/nautilus/mozilla-components/libeazel-protocol-handler.so

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BONOBO_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(MOZILLA_COMPONENT_CFLAGS) \
	-DMOZILLA_MILESTONE=$(MOZILLA_MILESTONE) \
	-DMOZILLA_EAZEL_PROTOCOL_HANDLER_COMPONENT_PATH="\"$(component_path)\""
	$(NULL)

bin_PROGRAMS =\
	nautilus-mozilla-content-view \
	$(NULL)

nautilus_mozilla_content_view_SOURCES = \
	main.c \
	mozilla-components.cpp \
	mozilla-events.cpp \
	mozilla-preferences.cpp \
	nautilus-mozilla-content-view.c \
	$(NULL)

noinst_HEADERS =\
	mozilla-components.h \
	mozilla-events.h \
	mozilla-preferences.h \
	nautilus-mozilla-content-view.h \
	$(NULL)

nautilus_mozilla_content_view_LDADD = \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(OAF_LIBS) \
	$(MOZILLA_COMPONENT_LDFLAGS) \
	$(MOZILLA_COMPONENT_LIBS) \
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA = \
	nautilus-mozilla-content-view.oafinfo \
	$(NULL)

EXTRA_DIST = $(oaf_DATA) $(NULL)
