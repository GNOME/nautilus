NULL=

SUBDIRS=file-manager

bin_PROGRAMS=nautilus

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/cut-n-paste-code \
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
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	$(NULL)

LDADD =\
	file-manager/libntl-file-manager.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la \
	$(top_builddir)/cut-n-paste-code/widgets/nautilus-druid/libnautilus-druid.la \
	$(top_builddir)/librsvg/librsvg.la                                \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_HEADERS = \
	nautilus-application.h			\
	nautilus-applicable-views.h		\
	nautilus-bookmark-list.h		\
	nautilus-bookmarks-window.h		\
	nautilus-desktop-window.h		\
	nautilus-first-time-druid.h		\
	nautilus-link-set-window.h		\
	nautilus-location-bar.h			\
	nautilus-search-bar.h			\
	nautilus-search-bar-criterion.h		\
	nautilus-switchable-search-bar.h	\
	nautilus-simple-search-bar.h		\
	nautilus-complex-search-bar.h		\
	nautilus-navigation-bar.h		\
	nautilus-property-browser.h		\
	nautilus-self-check-functions.h 	\
	nautilus-signaller.h			\
	nautilus-zoom-control.h			\
	nautilus-switchable-navigation-bar.h	\
	nautilus-sidebar.h			\
	nautilus-sidebar-tabs.h			\
	nautilus-sidebar-title.h		\
	nautilus-view-frame-private.h		\
	nautilus-view-frame.h			\
	nautilus-window-manage-views.h		\
	nautilus-window-private.h		\
	nautilus-window.h			\
	$(NULL)

nautilus_SOURCES =\
	nautilus-bookmark-list.c		\
	nautilus-bookmarks-window.c		\
	nautilus-desktop-window.c		\
	nautilus-first-time-druid.c		\
	nautilus-link-set-window.c		\
	nautilus-location-bar.c         	\
	nautilus-search-bar.c			\
	nautilus-switchable-search-bar.c	\
	nautilus-search-bar-criterion.c		\
	nautilus-simple-search-bar.c		\
	nautilus-complex-search-bar.c		\
	nautilus-navigation-bar.c         	\
	nautilus-property-browser.c     	\
	nautilus-self-check-functions.c 	\
	nautilus-signaller.c			\
	nautilus-window-menus.c         	\
	nautilus-window-toolbars.c      	\
	nautilus-zoom-control.c			\
	nautilus-zoomable-frame-corba.c		\
	nautilus-application.c			\
	nautilus-sidebar.c			\
	nautilus-sidebar-tabs.c			\
	nautilus-sidebar-title.c		\
	nautilus-switchable-navigation-bar.c	\
	nautilus-main.c 			\
	nautilus-applicable-views.c		\
	nautilus-view-frame-bonobo-control.c	\
	nautilus-view-frame-bonobo-embeddable.c	\
	nautilus-view-frame-corba.c		\
	nautilus-view-frame-nautilus-view.c	\
	nautilus-view-frame.c			\
	nautilus-window-manage-views.c		\
	nautilus-window.c			\
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
