include $(top_srcdir)/Makefile.shared

INCLUDES = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\" \
	-DDATADIR=\"$(datadir)\" \
	$(MOZILLA_COMPONENT_DEBUG_FLAGS) \
	$(MOZILLA_PROFILES_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BONOBO_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(MOZILLA_COMPONENT_CFLAGS) \
	-DMOZILLA_MILESTONE=$(MOZILLA_MILESTONE) \
	$(AMMONITE_CFLAGS) \
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
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(OAF_LIBS) \
	$(MOZILLA_COMPONENT_LDFLAGS) \
	$(MOZILLA_COMPONENT_LIBS) \
	$(AMMONITE_LIBS) \
	$(NULL)

nautilus_mozilla_content_view_LDFLAGS = \
	$(MOZILLA_COMPONENT_RPATH_FLAGS) \
	$(NULL)


oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_mozilla.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

uidir = $(datadir)/gnome/ui
ui_DATA =					\
	nautilus-mozilla-ui.xml			\
	$(NULL)

EXTRA_DIST = $(oaf_DATA) $(ui_DATA) $(oaf_in_files)
