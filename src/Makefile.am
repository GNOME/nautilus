NULL=

SUBDIRS=file-manager

bin_PROGRAMS=nautilus

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/cut-n-paste-code \
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
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" \
	$(NULL)

LDADD =\
	$(top_builddir)/src/file-manager/libnautilus-file-manager.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

nautilus_shell_interface_idl_sources =		\
	nautilus-shell-interface-stubs.c	\
	nautilus-shell-interface-skels.c	\
	nautilus-shell-interface.h		\
	nautilus-shell-interface-common.c

BUILT_SOURCES = $(nautilus_shell_interface_idl_sources)

noinst_HEADERS = \
	nautilus-applicable-views.h		\
	nautilus-application.h			\
	nautilus-bookmark-list.h		\
	nautilus-bookmark-parsing.h		\
	nautilus-bookmarks-window.h		\
	nautilus-complex-search-bar.h		\
	nautilus-desktop-window.h		\
	nautilus-first-time-druid.h		\
	nautilus-history-frame.h		\
	nautilus-link-set-window.h		\
	nautilus-location-bar.h			\
	nautilus-navigation-bar.h		\
	nautilus-property-browser.h		\
	nautilus-search-bar-private.h		\
	nautilus-search-bar.h			\
	nautilus-self-check-functions.h 	\
	nautilus-shell.h			\
	nautilus-sidebar-tabs.h			\
	nautilus-sidebar-title.h		\
	nautilus-sidebar.h			\
	nautilus-signaller.h			\
	nautilus-simple-search-bar.h		\
	nautilus-switchable-navigation-bar.h	\
	nautilus-switchable-search-bar.h	\
	nautilus-toolbar.h			\
	nautilus-view-frame-private.h		\
	nautilus-view-frame.h			\
	nautilus-window-manage-views.h		\
	nautilus-window-private.h		\
	nautilus-window.h			\
	nautilus-zoom-control.h			\
	$(NULL)

nautilus_SOURCES = \
	$(nautilus_shell_interface_idl_sources)	\
	nautilus-applicable-views.c		\
	nautilus-application.c			\
	nautilus-bookmark-list.c		\
	nautilus-bookmark-parsing.c		\
	nautilus-bookmarks-window.c		\
	nautilus-complex-search-bar.c		\
	nautilus-desktop-window.c		\
	nautilus-first-time-druid.c		\
	nautilus-history-frame.c		\
	nautilus-link-set-window.c		\
	nautilus-location-bar.c         	\
	nautilus-main.c 			\
	nautilus-navigation-bar.c         	\
	nautilus-property-browser.c     	\
	nautilus-search-bar.c			\
	nautilus-self-check-functions.c 	\
	nautilus-shell.c			\
	nautilus-sidebar-tabs.c			\
	nautilus-sidebar-title.c		\
	nautilus-sidebar.c			\
	nautilus-signaller.c			\
	nautilus-simple-search-bar.c		\
	nautilus-switchable-navigation-bar.c	\
	nautilus-switchable-search-bar.c	\
	nautilus-toolbar.c			\
	nautilus-view-frame-bonobo-control.c	\
	nautilus-view-frame-bonobo-embeddable.c	\
	nautilus-view-frame-corba.c		\
	nautilus-view-frame-nautilus-view.c	\
	nautilus-view-frame.c			\
	nautilus-window-manage-views.c		\
	nautilus-window-menus.c         	\
	nautilus-window-toolbars.c      	\
	nautilus-window.c			\
	nautilus-zoom-control.c			\
	nautilus-zoomable-frame-corba.c		\
	$(NULL)

$(nautilus_shell_interface_idl_sources): nautilus_shell_interface_idl_stamp
nautilus_shell_interface_idl_stamp: nautilus-shell-interface.idl $(top_srcdir)/libnautilus/nautilus-view-component.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) -I$(top_srcdir) $(srcdir)/nautilus-shell-interface.idl
	touch nautilus_shell_interface_idl_stamp

TESTS=check-nautilus

oafdir = $(datadir)/oaf
oaf_DATA =					\
	nautilus.oafinfo

CLEANFILES = \
	$(nautilus_shell_interface_idl_sources)	\
	nautilus_shell_interface_idl_stamp	\
	$(NULL)

EXTRA_DIST = \
	check-nautilus \
	nautilus-shell-interface.idl		\
	$(oaf_DATA)				\
	run-nautilus				\
	$(NULL)

install-data-hook: run-nautilus
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='run-nautilus'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p; \
	  else :; fi; \
	done
