include $(top_srcdir)/Makefile.shared

SUBDIRS=file-manager

bin_PROGRAMS=					\
	nautilus				\
	nautilus-file-management-properties	\
	nautilus-autorun-software		\
	nautilus-connect-server			\
	$(NULL)

libexec_PROGRAMS=				\
	nautilus-convert-metadata		\
	$(NULL)

INCLUDES =							\
	-I$(top_srcdir) 					\
	-I$(top_srcdir)/cut-n-paste-code			\
	-I$(top_builddir)/libnautilus-private 			\
	$(CORE_CFLAGS) 						\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 		\
	-DUIDIR=\""$(datadir)/nautilus/ui"\" 		\
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" 	\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 				\
	$(DISABLE_DEPRECATED_CFLAGS)                            \
	$(NULL)

LDADD =\
	$(top_builddir)/src/file-manager/libnautilus-file-manager.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la  \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
	$(NULL)

@INTLTOOL_DESKTOP_RULE@

desktop_in_files=network-scheme.desktop.in
desktop_files=$(desktop_in_files:.desktop.in=.desktop)
desktopdir = $(datadir)/gnome/network/

schemedir = $(datadir)/applications
scheme_DATA = network-scheme.desktop

nautilus_SOURCES = \
	ephy-spinner.h				\
	ephy-spinner.c				\
	nautilus-actions.h			\
	nautilus-application.c			\
	nautilus-application.h			\
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-bookmarks-window.c		\
	nautilus-bookmarks-window.h		\
	nautilus-connect-server-dialog.c	\
	nautilus-connect-server-dialog.h	\
	nautilus-connect-server-dialog-nonmain.c	\
	nautilus-desktop-window.c		\
	nautilus-desktop-window.h		\
	nautilus-emblem-sidebar.c		\
	nautilus-emblem-sidebar.h		\
	nautilus-file-management-properties.c	\
	nautilus-file-management-properties.h	\
	nautilus-history-sidebar.c		\
	nautilus-history-sidebar.h		\
	nautilus-image-properties-page.c	\
	nautilus-image-properties-page.h	\
	nautilus-information-panel.c            \
	nautilus-information-panel.h            \
	nautilus-location-bar.c         	\
	nautilus-location-bar.h			\
	nautilus-location-dialog.c              \
	nautilus-location-dialog.h              \
	nautilus-location-entry.c               \
	nautilus-location-entry.h               \
	nautilus-main.c 			\
	nautilus-main.h				\
	nautilus-navigation-action.c		\
	nautilus-navigation-action.h		\
	nautilus-navigation-bar.c         	\
	nautilus-navigation-bar.h		\
	nautilus-navigation-window-menus.c      \
	nautilus-navigation-window.c            \
	nautilus-navigation-window.h            \
	nautilus-navigation-window-slot.c	\
	nautilus-navigation-window-slot.h	\
	nautilus-notebook.c			\
	nautilus-notebook.h			\
	nautilus-notes-viewer.c			\
	nautilus-notes-viewer.h			\
	nautilus-pathbar.c			\
	nautilus-pathbar.h			\
	nautilus-places-sidebar.c		\
	nautilus-places-sidebar.h		\
	nautilus-property-browser.c     	\
	nautilus-property-browser.h		\
	nautilus-query-editor.c			\
	nautilus-query-editor.h			\
	nautilus-search-bar.c			\
	nautilus-search-bar.h			\
	nautilus-self-check-functions.c 	\
	nautilus-self-check-functions.h 	\
	nautilus-side-pane.c                    \
	nautilus-side-pane.h			\
	nautilus-sidebar-title.c		\
	nautilus-sidebar-title.h		\
	nautilus-spatial-window.c               \
	nautilus-spatial-window.h               \
	nautilus-throbber.c			\
	nautilus-throbber.h			\
	nautilus-trash-bar.c			\
	nautilus-trash-bar.h			\
	nautilus-window-bookmarks.c		\
	nautilus-window-bookmarks.h		\
	nautilus-window-manage-views.c		\
	nautilus-window-manage-views.h		\
	nautilus-window-menus.c         	\
	nautilus-window-private.h		\
	nautilus-window-slot.c			\
	nautilus-window-slot.h			\
	nautilus-window-toolbars.c              \
	nautilus-window.c			\
	nautilus-window.h			\
	nautilus-x-content-bar.c		\
	nautilus-x-content-bar.h		\
	nautilus-zoom-control.c			\
	nautilus-zoom-control.h			\
	$(NULL)

nautilus_file_management_properties_SOURCES= \
	nautilus-file-management-properties.c		\
	nautilus-file-management-properties.h		\
	nautilus-file-management-properties-main.c	\
	$(NULL)

nautilus_autorun_software_SOURCES= 			\
	nautilus-autorun-software.c			\
	$(NULL)

nautilus_connect_server_SOURCES= \
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-connect-server-dialog.c	\
	nautilus-connect-server-dialog.h	\
	nautilus-connect-server-dialog-main.c	\
	nautilus-location-entry.c		\
	nautilus-location-entry.h		\
	$(NULL)

nautilus_convert_metadata_SOURCES= \
	nautilus-convert-metadata.c	\
	$(NULL)

TESTS=check-nautilus

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/nautilus/ui
ui_DATA = 					\
	nautilus-shell-ui.xml			\
	nautilus-navigation-window-ui.xml       \
	nautilus-spatial-window-ui.xml          \
	nautilus-file-management-properties.ui	\
	nautilus-bookmarks-window.ui		\
	$(NULL)

CLEANFILES = \
	$(desktop_files)			\
	$(server_DATA)				\
	$(NULL)

EXTRA_DIST =					\
	$(server_in_files)			\
	$(ui_DATA)				\
	check-nautilus				\
	$(desktop_in_files)			\
	$(NULL)

BUILT_SOURCES = \
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
