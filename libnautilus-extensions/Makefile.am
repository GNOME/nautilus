include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnautilus-extensions.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/src/file-manager 		\
	-I$(top_builddir) 				\
	-I$(top_builddir)/libnautilus 			\
	-I$(top_srcdir)/cut-n-paste-code 		\
	$(BONOBO_PRINT_CFLAGS) 				\
	$(BONOBOX_CFLAGS) 				\
	$(ESD_CFLAGS)					\
	$(GCONF_CFLAGS) 				\
	$(GHTTP_CFLAGS) 				\
	$(GNOME_CFLAGS) 				\
	$(MEDUSA_CFLAGS) 				\
	$(OAF_CFLAGS) 					\
	$(VFS_CFLAGS) 					\
	$(XML_CFLAGS) 					\
	-D_REENTRANT 					\
	-DDATADIR=\""$(datadir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 	\
	$(NULL)

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la \
	$(top_builddir)/cut-n-paste-code/widgets/nautilusclist/libnautilusclist.la \
	$(top_builddir)/cut-n-paste-code/widgets/gimphwrapbox/libgtkhwrapbox.la \
	$(top_builddir)/librsvg/librsvg.la \
	$(NULL)

libnautilus_extensions_la_LDFLAGS =	\
	$(dependency_static_libs)	\
	$(BONOBO_PRINT_LIBS) 		\
	$(BONOBOX_LIBS) 		\
	$(ESD_LIBS)			\
	$(FREETYPE2_LIBS)		\
	$(GCONF_LIBS)			\
	$(GHTTP_LIBS) 			\
	$(GNOMECANVASPIXBUF_LIBS)	\
	$(GNOME_LIBS) 			\
	$(LIBPNG) 			\
	$(MEDUSA_LIBS) 			\
	$(OAF_LIBS) 			\
	$(VFS_LIBS) 			\
	$(XML_LIBS) 			\
	$(NULL)

nautilus_metafile_server_idl_sources =		\
	nautilus-metafile-server-stubs.c	\
	nautilus-metafile-server-skels.c	\
	nautilus-metafile-server.h		\
	nautilus-metafile-server-common.c

libnautilus_extensions_la_SOURCES = 		\
	$(nautilus_metafile_server_idl_sources)	\
	bug-5712-pr3-workaround--gdk-pixbuf-drawable.c \
	bug-5712-pr3-workaround--gdk-pixbuf-private.h \
	bug-5712-pr3-workaround--gdkimage.c \
	nautilus-art-extensions.c		\
	nautilus-art-gtk-extensions.c		\
	nautilus-background-canvas-group.c	\
	nautilus-background.c			\
	nautilus-bonobo-extensions.c		\
	nautilus-bookmark.c			\
	nautilus-caption-table.c		\
	nautilus-caption.c			\
	nautilus-clickable-image.c		\
	nautilus-ctree.c			\
	nautilus-customization-data.c		\
	nautilus-dateedit-extensions.c		\
	nautilus-debug-drawing.c		\
	nautilus-debug.c			\
	nautilus-default-file-icon.c		\
	nautilus-directory-async.c		\
	nautilus-directory-background.c		\
	nautilus-directory-metafile-monitor.c   \
	nautilus-directory-metafile.c		\
	nautilus-directory.c			\
	nautilus-drag-window.c			\
	nautilus-drag.c				\
	nautilus-druid-page-eazel.c		\
	nautilus-druid.c			\
	nautilus-ellipsizing-label.c		\
	nautilus-entry.c			\
	nautilus-enumeration.c			\
	nautilus-file-changes-queue.c		\
	nautilus-file-operations-progress.c	\
	nautilus-file-operations.c		\
	nautilus-file-utilities.c		\
	nautilus-file.c				\
	nautilus-font-factory.c			\
	nautilus-font-manager.c			\
	nautilus-font-picker.c			\
	nautilus-gconf-extensions.c		\
	nautilus-gdk-extensions.c		\
	nautilus-gdk-font-extensions.c		\
	nautilus-gdk-pixbuf-extensions.c	\
	nautilus-generous-bin.c			\
	nautilus-glib-extensions.c		\
	nautilus-global-preferences.c		\
	nautilus-glyph.c			\
	nautilus-gnome-extensions.c		\
	nautilus-graphic-effects.c		\
	nautilus-gtk-extensions.c		\
	nautilus-horizontal-splitter.c		\
	nautilus-icon-canvas-item.c       	\
	nautilus-icon-container.c		\
	nautilus-icon-dnd.c			\
	nautilus-icon-factory.c			\
	nautilus-icon-text-item.c		\
	nautilus-image-table.c			\
	nautilus-image-with-background.c	\
	nautilus-image.c			\
	nautilus-keep-last-vertical-box.c	\
	nautilus-label-with-background.c	\
	nautilus-label.c			\
	nautilus-labeled-image.c		\
	nautilus-lib-self-check-functions.c	\
	nautilus-link-set.c			\
	nautilus-link.c				\
	nautilus-list-column-title.c		\
	nautilus-list.c                         \
	nautilus-medusa-support.c		\
	nautilus-merged-directory.c		\
	nautilus-metafile-factory.c             \
	nautilus-metafile.c                     \
	nautilus-mime-actions.c			\
	nautilus-password-dialog.c		\
	nautilus-preferences-box.c		\
	nautilus-preferences-dialog.c		\
	nautilus-preferences-group.c		\
	nautilus-preferences-item.c		\
	nautilus-preferences-pane.c		\
	nautilus-preferences.c			\
	nautilus-program-chooser.c		\
	nautilus-program-choosing.c		\
	nautilus-radio-button-group.c		\
	nautilus-region.c			\
	nautilus-scalable-font.c		\
	nautilus-search-uri.c			\
	nautilus-self-checks.c			\
	nautilus-smooth-text-layout.c		\
	nautilus-smooth-text-layout-cache.c	\
	nautilus-smooth-widget.c		\
	nautilus-sound.c			\
	nautilus-stock-dialogs.c		\
	nautilus-string-list.c			\
	nautilus-string-map.c			\
	nautilus-string-picker.c		\
	nautilus-string.c			\
	nautilus-tabs.c				\
	nautilus-text-caption.c			\
	nautilus-theme.c			\
	nautilus-thumbnails.c			\
	nautilus-trash-directory.c		\
	nautilus-trash-file.c			\
	nautilus-trash-monitor.c		\
	nautilus-undo-context.c			\
	nautilus-undo-manager.c			\
	nautilus-undo-signal-handlers.c		\
	nautilus-vfs-directory.c		\
	nautilus-vfs-file.c			\
	nautilus-view-identifier.c		\
	nautilus-viewport.c			\
	nautilus-volume-monitor.c		\
	nautilus-wrap-table.c			\
	nautilus-xml-extensions.c		\
	$(NULL)

# Everything is private for now
noinst_HEADERS =				\
	nautilus-art-extensions.h		\
	nautilus-art-gtk-extensions.h		\
	nautilus-background-canvas-group.h	\
	nautilus-background.h			\
	nautilus-bonobo-extensions.h		\
	nautilus-bookmark.h			\
	nautilus-caption-table.h		\
	nautilus-caption.h			\
	nautilus-cdrom-extensions.h		\
	nautilus-clickable-image.h		\
	nautilus-ctree.h			\
	nautilus-customization-data.h		\
	nautilus-dateedit-extensions.h 		\
	nautilus-debug-drawing.h		\
	nautilus-debug.h			\
	nautilus-default-file-icon.h		\
	nautilus-directory-background.h		\
	nautilus-directory-metafile-monitor.h   \
	nautilus-directory-metafile.h		\
	nautilus-directory-notify.h		\
	nautilus-directory-private.h		\
	nautilus-directory.h			\
	nautilus-drag-window.h			\
	nautilus-drag.h				\
	nautilus-druid-page-eazel.h		\
	nautilus-druid.h			\
	nautilus-ellipsizing-label.h		\
	nautilus-entry.h			\
	nautilus-enumeration.h			\
	nautilus-file-attributes.h		\
	nautilus-file-changes-queue.h		\
	nautilus-file-operations-progress.h 	\
	nautilus-file-operations.h 		\
	nautilus-file-private.h			\
	nautilus-file-utilities.h		\
	nautilus-file.h				\
	nautilus-font-factory.h			\
	nautilus-font-manager.h			\
	nautilus-font-picker.h			\
	nautilus-gconf-extensions.h		\
	nautilus-gdk-extensions.h		\
	nautilus-gdk-font-extensions.h		\
	nautilus-gdk-pixbuf-extensions.h	\
	nautilus-generous-bin.h			\
	nautilus-glib-extensions.h		\
	nautilus-global-preferences.h		\
	nautilus-glyph.h			\
	nautilus-gnome-extensions.h		\
	nautilus-graphic-effects.h		\
	nautilus-gtk-extensions.h		\
	nautilus-gtk-macros.h			\
	nautilus-horizontal-splitter.h		\
	nautilus-icon-canvas-item.h		\
	nautilus-icon-container.h		\
	nautilus-icon-dnd.h			\
	nautilus-icon-factory-private.h		\
	nautilus-icon-factory.h			\
	nautilus-icon-private.h			\
	nautilus-icon-text-item.h		\
	nautilus-image-table.h			\
	nautilus-image-with-background.h	\
	nautilus-image.h			\
	nautilus-iso9660.h			\
	nautilus-keep-last-vertical-box.h	\
	nautilus-label-with-background.h	\
	nautilus-label.h			\
	nautilus-labeled-image.h		\
	nautilus-lib-self-check-functions.h	\
	nautilus-link-set.h			\
	nautilus-link.h				\
	nautilus-list-column-title.h		\
	nautilus-list.h                         \
	nautilus-medusa-support.h		\
	nautilus-merged-directory.h		\
	nautilus-metadata.h			\
	nautilus-metafile-factory.h             \
	nautilus-metafile.h                     \
	nautilus-mime-actions.h			\
	nautilus-password-dialog.h		\
	nautilus-preferences-box.h		\
	nautilus-preferences-dialog.h		\
	nautilus-preferences-group.h		\
	nautilus-preferences-item.h		\
	nautilus-preferences-pane.h		\
	nautilus-preferences-private.h		\
	nautilus-preferences.h			\
	nautilus-program-chooser.h		\
	nautilus-program-choosing.h		\
	nautilus-radio-button-group.h		\
	nautilus-region.h			\
	nautilus-scalable-font-private.h	\
	nautilus-scalable-font-private.h	\
	nautilus-scalable-font.h		\
	nautilus-search-uri.h			\
	nautilus-self-checks.h			\
	nautilus-smooth-text-layout.h		\
	nautilus-smooth-text-layout-cache.h	\
	nautilus-smooth-widget.h		\
	nautilus-sound.h			\
	nautilus-stock-dialogs.h		\
	nautilus-string-list.h			\
	nautilus-string-map.h			\
	nautilus-string-picker.h		\
	nautilus-string.h			\
	nautilus-tabs.h				\
	nautilus-text-caption.h			\
	nautilus-theme.h			\
	nautilus-thumbnails.h			\
	nautilus-trash-directory.h		\
	nautilus-trash-file.h			\
	nautilus-trash-monitor.h		\
	nautilus-undo-context.h			\
	nautilus-undo-manager.h			\
	nautilus-undo-signal-handlers.h		\
	nautilus-vfs-directory.h		\
	nautilus-vfs-file.h			\
	nautilus-view-identifier.h		\
	nautilus-viewport.h			\
	nautilus-volume-monitor.h		\
	nautilus-wrap-table.h			\
	nautilus-xml-extensions.h		\
	$(NULL)

$(lib_LTLIBRARIES): $(dependency_static_libs)

$(nautilus_metafile_server_idl_sources): nautilus_metafile_server_idl_stamp

nautilus_metafile_server_idl_stamp: nautilus-metafile-server.idl $(ORBIT_IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(srcdir)/nautilus-metafile-server.idl
	touch nautilus_metafile_server_idl_stamp

EXTRA_DIST = \
	nautilus-metafile-server.idl	\
	$(NULL)

BUILT_SOURCES = \
	$(nautilus_metafile_server_idl_sources)		\
	$(NULL)

CLEANFILES = \
	$(nautilus_metafile_server_idl_sources)		\
	nautilus_metafile_server_idl_stamp		\
	$(NULL)
