NULL=

SUBDIRS=file-manager

bin_PROGRAMS=nautilus

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libnautilus \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(WERROR) \
	$(NULL)

LDADD =\
	file-manager/libntl-file-manager.la \
	../nautilus-widgets/libnautilus-widgets.la \
	../libnautilus/libnautilus.la \
	../libnautilus-extensions/libnautilus-extensions.la \
	../librsvg/librsvg.la \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

nautilus_SOURCES =\
	nautilus-bookmark-list.c	\
	nautilus-bookmark-list.h	\
	nautilus-bookmarks-window.c	\
	nautilus-bookmarks-window.h	\
	nautilus-desktop-window.c	\
	nautilus-desktop-window.h	\
	nautilus-index-tabs.c		\
	nautilus-index-tabs.h		\
	nautilus-index-title.c		\
	nautilus-index-title.h		\
	nautilus-location-bar.c         \
	nautilus-location-bar.h		\
	nautilus-property-browser.c     \
	nautilus-property-browser.h	\
	nautilus-self-check-functions.c \
	nautilus-self-check-functions.h \
	nautilus-signaller.c		\
	nautilus-signaller.h		\
	nautilus-window-menus.c         \
	nautilus-window-toolbars.c      \
	nautilus-zoom-control.c		\
	nautilus-zoom-control.h		\
	nautilus-zoomable-frame-svr.c	\
	nautilus.h			\
	ntl-app.c			\
	ntl-app.h			\
	ntl-content-view.c		\
	ntl-content-view.h		\
	ntl-index-panel.c		\
	ntl-index-panel.h		\
	ntl-main.c 			\
	ntl-miniicon.c			\
	ntl-miniicon.h			\
	ntl-types.h			\
	ntl-uri-map.c			\
	ntl-uri-map.h			\
	ntl-view-bonobo-control.c	\
	ntl-view-bonobo-subdoc.c	\
	ntl-view-frame-svr.c		\
	ntl-view-nautilus.c		\
	ntl-view-private.h		\
	ntl-view.c			\
	ntl-view.h			\
	ntl-window-msgs.c		\
	ntl-window-msgs.h		\
	ntl-window-private.h		\
	ntl-window-state.c		\
	ntl-window-state.h		\
	ntl-window.c			\
	ntl-window.h			\
	$(NULL)

TESTS=check-nautilus

oafdir = $(datadir)/oaf
oaf_DATA =					\
	nautilus.oafinfo

EXTRA_DIST = $(oaf_DATA) run-nautilus

install-data-hook: run-nautilus
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='run-nautilus'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p; \
	  else :; fi; \
	done
