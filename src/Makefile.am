include $(top_srcdir)/Makefile.shared

SUBDIRS=file-manager

bin_PROGRAMS=nautilus

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_srcdir)/cut-n-paste-code/widgets/e-paned \
	-I$(top_srcdir)/cut-n-paste-code/widgets/gimphwrapbox \
	$(NAUTILUS_SRC_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DVERSION="\"$(VERSION)\"" \
	$(NULL)

LDADD =\
	$(top_builddir)/src/file-manager/libnautilus-file-manager.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-adapter/libnautilus-adapter.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la  \
	$(NAUTILUS_SRC_LIBS) \
	$(NULL)

nautilus_shell_interface_idl_sources =		\
	nautilus-shell-interface-stubs.c	\
	nautilus-shell-interface-skels.c	\
	nautilus-shell-interface.h		\
	nautilus-shell-interface-common.c

if ENABLE_PROFILER
profiler_headers = nautilus-profiler.h
profiler_sources = nautilus-profiler.c
endif

nautilus_SOURCES = \
	$(nautilus_shell_interface_idl_sources)	\
	nautilus-about.c			\
	nautilus-applicable-views.c		\
	nautilus-application.c			\
	nautilus-bookmark-list.c		\
	nautilus-bookmark-parsing.c		\
	nautilus-bookmarks-window.c		\
	nautilus-complex-search-bar.c		\
	nautilus-component-adapter-factory.c	\
	nautilus-desktop-window.c		\
	nautilus-first-time-druid.c		\
	nautilus-link-set-window.c		\
	nautilus-location-bar.c         	\
	nautilus-main.c 			\
	nautilus-navigation-bar.c         	\
	nautilus-preferences-dialog.c		\
	nautilus-property-browser.c     	\
	nautilus-search-bar-criterion.c		\
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
	nautilus-theme-selector.c		\
	nautilus-view-frame-corba.c		\
	nautilus-view-frame.c			\
	nautilus-window-manage-views.c		\
	nautilus-window-menus.c         	\
	nautilus-window-toolbars.c      	\
	nautilus-window.c			\
	nautilus-zoom-control.c			\
	$(profiler_sources)			\
	nautilus-about.h			\
	nautilus-applicable-views.h		\
	nautilus-application.h			\
	nautilus-bookmark-list.h		\
	nautilus-bookmark-parsing.h		\
	nautilus-bookmarks-window.h		\
	nautilus-complex-search-bar.h		\
	nautilus-component-adapter-factory.h	\
	nautilus-desktop-window.h		\
	nautilus-first-time-druid.h		\
	nautilus-link-set-window.h		\
	nautilus-location-bar.h			\
	nautilus-main.h				\
	nautilus-navigation-bar.h		\
	nautilus-preferences-dialog.h \
	nautilus-property-browser.h		\
	nautilus-search-bar-criterion-private.h	\
	nautilus-search-bar-criterion.h		\
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
	nautilus-theme-selector.h		\
	nautilus-view-frame-private.h		\
	nautilus-view-frame.h			\
	nautilus-window-manage-views.h		\
	nautilus-window-private.h		\
	nautilus-window.h			\
	nautilus-zoom-control.h			\
	$(profiler_headers)			\
	$(NULL)

$(nautilus_shell_interface_idl_sources): nautilus_shell_interface_idl_stamp
nautilus_shell_interface_idl_stamp: nautilus-shell-interface.idl $(ORBIT_IDL) $(top_srcdir)/libnautilus/nautilus-view-component.idl
	$(ORBIT_IDL) $(NAUTILUS_SRC_IDL_INCLUDES) -I$(top_srcdir) $<
	touch $@

$(nautilus_OBJECTS): nautilus_shell_interface_idl_stamp

TESTS=check-nautilus

serverdir = $(datadir)/server
server_in_files = Nautilus_shell.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome/ui
ui_DATA = 					\
	nautilus-shell-ui.xml			\
	$(NULL)

CLEANFILES = \
	$(nautilus_shell_interface_idl_sources)	\
	nautilus_shell_interface_idl_stamp	\
	$(NULL)

EXTRA_DIST =					\
	$(server_in_files)				\
	$(server_DATA)				\
	$(ui_DATA)				\
	check-nautilus				\
	nautilus-shell-interface.idl		\
	run-nautilus				\
	$(NULL)

BUILT_SOURCES = \
	$(nautilus_shell_interface_idl_sources) \
	$(NULL)

install-data-hook: run-nautilus
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='run-nautilus'; for p in $$list; do\
	  if test -f $$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $$p $(DESTDIR)/$(prefix)/bin/$$p; \
	  else :; fi; \
	done

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
