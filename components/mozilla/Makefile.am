NULL = 

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\" \
	-DDATADIR=\"$(datadir)\" \
	$(MOZILLA_COMPONENT_DEBUG_FLAGS) \
	$(NULL)

if EAZEL_SERVICES
EAZEL_SERVICES_INCLUDES = \
	$(AMMONITE_CFLAGS) \
	-I$(top_srcdir)/components/services/trilobite/libammonite-gtk \
	$(NULL)

EAZEL_SERVICES_LIBS = \
	$(top_srcdir)/components/services/trilobite/libammonite-gtk/libammonite-gtk.a \
	$(AMMONITE_LIBS) 				\
	$(NULL)
else

EAZEL_SERVICES_INCLUDES =
EAZEL_SERVICES_LIBS = 

endif

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BONOBO_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(MOZILLA_COMPONENT_CFLAGS) \
	-DMOZILLA_MILESTONE=$(MOZILLA_MILESTONE) \
	$(EAZEL_SERVICES_INCLUDES)
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
	$(EAZEL_SERVICES_LIBS)				\
	$(top_builddir)/libnautilus/libnautilus.la 	\
	$(BONOBO_LIBS) 					\
	$(GCONF_LIBS) 					\
	$(GNOMEUI_LIBS) 				\
	$(VFS_LIBS) 					\
	$(OAF_LIBS) 					\
	$(MOZILLA_COMPONENT_LDFLAGS) 			\
	$(MOZILLA_COMPONENT_LIBS) 			\
	$(NULL)

oafdir = $(datadir)/oaf
oaf_DATA = \
	nautilus-mozilla-content-view.oafinfo \
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = \
	nautilus-mozilla-ui.xml \
	$(NULL)

EXTRA_DIST = $(oaf_DATA) $(ui_DATA)
