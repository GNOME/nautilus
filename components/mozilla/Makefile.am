NULL = 

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Mozilla\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BONOBO_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(MOZILLA_COMPONENT_CFLAGS) \
	-DMOZILLA_MILESTONE=$(MOZILLA_MILESTONE) \
	$(NULL)

bin_PROGRAMS =\
	nautilus-mozilla-content-view \
	$(NULL)

nautilus_mozilla_content_view_SOURCES = \
	nautilus-mozilla-content-view.h \
	nautilus-mozilla-content-view.c \
	main.c \
	mozilla-preferences.cpp \
	$(NULL)

noinst_HEADERS =				\
	mozilla-preferences.h \
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

caca:
	@echo $(MOZILLA_MILESTONE)
