include $(top_srcdir)/Makefile.shared

INCLUDES = \
	-I$(top_srcdir) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\" \
	-DDATADIR=\"$(datadir)\" \
	-DPREFIX=\"$(prefix)\" \
	$(MOZILLA_COMPONENT_DEBUG_FLAGS) \
	$(MOZILLA_PROFILES_CFLAGS) \
	$(MOZILLA_COMPONENT_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(NAUTILUS_DEPENDENCY_CFLAGS) \
	$(NULL)

bin_PROGRAMS =\
	nautilus-mozilla-content-view \
	$(NULL)

nautilus_mozilla_content_view_SOURCES = \
	bonobo-extensions.c \
	main.c \
	mozilla-components.cpp \
	mozilla-events.cpp \
	mozilla-preferences.cpp \
	nautilus-mozilla-content-view.c \
	nautilus-mozilla-embed-extensions.cpp \
	nautilus-mozilla-encoding-tables.c \
	bonobo-extensions.h \
	mozilla-components.h \
	mozilla-events.h \
	mozilla-preferences.h \
	nautilus-mozilla-content-view.h \
	nautilus-mozilla-embed-extensions.h \
	nautilus-mozilla-encoding-tables.h \
	$(NULL)

nautilus_mozilla_content_view_LDADD = \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(MOZILLA_COMPONENT_LDFLAGS) \
	$(MOZILLA_COMPONENT_LIBS) \
	$(GCONF_LIBS) \
	$(NAUTILUS_DEPENDENCY_LIBS) \
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
