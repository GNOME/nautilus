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
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-bookmarks-window.c		\
	nautilus-bookmarks-window.h		\
	nautilus-desktop-window.c		\
	nautilus-desktop-window.h		\
	nautilus-sidebar-tabs.c			\
	nautilus-sidebar-tabs.h			\
	nautilus-sidebar-title.c		\
	nautilus-sidebar-title.h		\
	nautilus-location-bar.c         	\
	nautilus-location-bar.h			\
	nautilus-property-browser.c     	\
	nautilus-property-browser.h		\
	nautilus-self-check-functions.c 	\
	nautilus-self-check-functions.h 	\
	nautilus-signaller.c			\
	nautilus-signaller.h			\
	nautilus-window-menus.c         	\
	nautilus-window-toolbars.c      	\
	nautilus-zoom-control.c			\
	nautilus-zoom-control.h			\
	nautilus-zoomable-frame-corba.c		\
	nautilus.h				\
	nautilus-application.c			\
	nautilus-application.h			\
	nautilus-sidebar.c			\
	nautilus-sidebar.h			\
	nautilus-main.c 			\
	nautilus-applicable-views.c		\
	nautilus-applicable-views.h		\
	nautilus-view-frame-bonobo-control.c	\
	nautilus-view-frame-bonobo-embeddable.c	\
	nautilus-view-frame-corba.c		\
	nautilus-view-frame-nautilus-view.c	\
	nautilus-view-frame-private.h		\
	nautilus-view-frame.c			\
	nautilus-view.h				\
	nautilus-window-manage-views.c		\
	nautilus-window-manage-views.h		\
	nautilus-window-private.h		\
	nautilus-window-state.c			\
	nautlus-window-state.h			\
	nautilus-window.c			\
	nautilus-window.h			\
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
