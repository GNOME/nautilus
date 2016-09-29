include $(top_srcdir)/Makefile.shared

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS=					\
	nautilus				\
	nautilus-autorun-software		\
	$(NULL)

noinst_LTLIBRARIES=libnautilus.la

AM_CPPFLAGS =							\
	-I$(top_srcdir) 					\
	-I$(top_srcdir)/libgd                                   \
	$(BASE_CFLAGS) 						\
	$(COMMON_CFLAGS)					\
	$(NAUTILUS_CFLAGS)					\
	$(WARNING_CFLAGS)					\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	$(TRACKER_CFLAGS)					\
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLOCALEDIR=\""$(localedir)"\"				\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 		\
	-DNAUTILUS_EXTENSIONDIR=\""$(libdir)/nautilus/extensions-3.0"\" \
	-DPREFIX=\""$(prefix)"\" 				\
	-DVERSION="\"$(VERSION)\"" 				\
	$(DISABLE_DEPRECATED) 		                        \
	$(NULL)

LDADD =\
	libnautilus.la \
	$(NULL)

libnautilus_la_LIBADD =\
	$(top_builddir)/libgd/libgd.la \
	$(BASE_LIBS) \
	$(COMMON_LIBS) \
	$(NAUTILUS_LIBS) \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
	$(TRACKER_LIBS) \
	$(SELINUX_LIBS) \
	$(top_builddir)/eel/libeel-2.la \
	$(top_builddir)/libnautilus-extension/libnautilus-extension.la \
	$(NULL)

dbus_built_sources = 					\
	nautilus-generated.c				\
	nautilus-generated.h

$(dbus_built_sources) : Makefile.am $(top_srcdir)/data/dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.gnome.Nautilus.					\
		--c-namespace NautilusDBus						\
		--c-generate-object-manager						\
		--generate-c-code nautilus-generated					\
		$(top_srcdir)/data/dbus-interfaces.xml					\
		$(NULL)

dbus_freedesktop_built_sources =			\
	nautilus-freedesktop-generated.c		\
	nautilus-freedesktop-generated.h

$(dbus_freedesktop_built_sources) : Makefile.am $(top_srcdir)/data/freedesktop-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.freedesktop.					\
		--c-namespace NautilusFreedesktop					\
		--c-generate-object-manager						\
		--generate-c-code nautilus-freedesktop-generated			\
		$(top_srcdir)/data/freedesktop-dbus-interfaces.xml			\
		$(NULL)

dbus_shell_search_provider_built_sources =		\
	nautilus-shell-search-provider-generated.c	\
	nautilus-shell-search-provider-generated.h

$(dbus_shell_search_provider_built_sources) : Makefile.am $(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.gnome.						\
		--c-namespace Nautilus							\
		--generate-c-code nautilus-shell-search-provider-generated		\
		$(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml		\
		$(NULL)

headers =					\
	nautilus-search-popover.h		\
	nautilus-special-location-bar.h		\
	nautilus-query.h			\
	nautilus-search-provider.h		\
	$(NULL)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir)/resources --generate-dependencies $(srcdir)/resources/nautilus.gresource.xml)
nautilus-resources.c: resources/nautilus.gresource.xml $(resource_files)
	glib-compile-resources --manual-register --target=$@ --sourcedir=$(srcdir)/resources --generate-source --c-name nautilus $(srcdir)/resources/nautilus.gresource.xml
nautilus-resources.h: resources/nautilus.gresource.xml $(resource_files)
	glib-compile-resources --manual-register --target=$@ --sourcedir=$(srcdir)/resources --generate-header --c-name nautilus $(srcdir)/resources/nautilus.gresource.xml

nautilus-enum-types.h: nautilus-enum-types.h.template $(headers)
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
		--template nautilus-enum-types.h.template \
		$(headers)) \
	> $@

nautilus-enum-types.c: nautilus-enum-types.c.template $(headers)
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
		--template nautilus-enum-types.c.template \
		$(headers)) \
	> $@

if ENABLE_TRACKER
nautilus_tracker_engine_sources = \
	nautilus-search-engine-tracker.c \
	nautilus-search-engine-tracker.h
endif

if ENABLE_TRACKER
nautilus_batch_renaming_tracker_sources = \
	nautilus-batch-rename-dialog.c \
	nautilus-batch-rename-dialog.h \
	nautilus-batch-rename-utilities.c \
	nautilus-batch-rename-utilities.h
endif

nautilus_built_sources = \
	$(dbus_built_sources) \
	$(dbus_freedesktop_built_sources) \
	$(dbus_shell_search_provider_built_sources) \
	nautilus-resources.c \
	nautilus-resources.h \
	nautilus-enum-types.h \
	nautilus-enum-types.c \
	$(NULL)

nautilus_no_main_sources = \
	gtk/nautilusgtkplacesview.c			\
	gtk/nautilusgtkplacesviewprivate.h		\
	gtk/nautilusgtkplacesviewrow.c			\
	gtk/nautilusgtkplacesviewrowprivate.h		\
	nautilus-application.c			\
	nautilus-application.h			\
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-canvas-view.c			\
	nautilus-canvas-view.h			\
	nautilus-canvas-view-container.c	\
	nautilus-canvas-view-container.h	\
	nautilus-dbus-manager.c 		\
	nautilus-dbus-manager.h 		\
	nautilus-desktop-item-properties.c	\
	nautilus-desktop-item-properties.h	\
	nautilus-error-reporting.c		\
	nautilus-error-reporting.h		\
	nautilus-preferences-window.c	\
	nautilus-preferences-window.h	\
	nautilus-files-view.c			\
	nautilus-files-view.h			\
	nautilus-files-view-dnd.c		\
	nautilus-files-view-dnd.h		\
	nautilus-floating-bar.c			\
	nautilus-floating-bar.h			\
	nautilus-freedesktop-dbus.c		\
	nautilus-freedesktop-dbus.h		\
	nautilus-image-properties-page.c	\
	nautilus-image-properties-page.h	\
	nautilus-list-model.c			\
	nautilus-list-model.h			\
	nautilus-list-view.c			\
	nautilus-list-view.h 			\
	nautilus-list-view-private.h		\
	nautilus-list-view-dnd.c		\
	nautilus-list-view-dnd.h		\
	nautilus-location-entry.c               \
	nautilus-location-entry.h               \
	nautilus-mime-actions.c 		\
	nautilus-mime-actions.h 		\
	nautilus-notebook.c			\
	nautilus-notebook.h			\
	nautilus-other-locations-window-slot.c	\
	nautilus-other-locations-window-slot.h	\
	nautilus-pathbar.c			\
	nautilus-pathbar.h			\
	nautilus-places-view.c			\
	nautilus-places-view.h			\
	nautilus-previewer.c			\
	nautilus-previewer.h			\
	nautilus-progress-info-widget.c		\
	nautilus-progress-info-widget.h		\
	nautilus-progress-persistence-handler.c	\
	nautilus-progress-persistence-handler.h	\
	nautilus-properties-window.c		\
	nautilus-properties-window.h		\
	nautilus-query-editor.c			\
	nautilus-query-editor.h			\
	nautilus-search-popover.c		\
	nautilus-search-popover.h		\
	nautilus-self-check-functions.c 	\
	nautilus-self-check-functions.h 	\
	nautilus-shell-search-provider.h	\
	nautilus-shell-search-provider.c	\
	nautilus-special-location-bar.c		\
	nautilus-special-location-bar.h		\
	nautilus-toolbar.c			\
	nautilus-toolbar.h			\
	nautilus-toolbar-menu-sections.h	\
	nautilus-trash-bar.c			\
	nautilus-trash-bar.h			\
	nautilus-view.c				\
	nautilus-view.h				\
	nautilus-window-slot.c			\
	nautilus-window-slot.h			\
	nautilus-window-slot-dnd.c		\
	nautilus-window-slot-dnd.h		\
	nautilus-window.c			\
	nautilus-window.h			\
	nautilus-x-content-bar.c		\
	nautilus-x-content-bar.h		\
	nautilus-bookmark.c \
	nautilus-bookmark.h \
	nautilus-canvas-container.c \
	nautilus-canvas-container.h \
	nautilus-canvas-dnd.c \
	nautilus-canvas-dnd.h \
	nautilus-canvas-item.c \
	nautilus-canvas-item.h \
	nautilus-canvas-private.h \
	nautilus-clipboard.c \
	nautilus-clipboard.h \
	nautilus-column-chooser.c \
	nautilus-column-chooser.h \
	nautilus-column-utilities.c \
	nautilus-column-utilities.h \
	nautilus-debug.c \
	nautilus-debug.h \
	nautilus-default-file-icon.c \
	nautilus-default-file-icon.h \
	nautilus-directory-async.c \
	nautilus-directory-notify.h \
	nautilus-directory-private.h \
	nautilus-directory.c \
	nautilus-directory.h \
	nautilus-dnd.c \
	nautilus-dnd.h \
	nautilus-file-attributes.h \
	nautilus-file-changes-queue.c \
	nautilus-file-changes-queue.h \
	nautilus-file-conflict-dialog.c \
	nautilus-file-conflict-dialog.h \
	nautilus-file-name-widget-controller.c \
	nautilus-file-name-widget-controller.h \
	nautilus-rename-file-popover-controller.c \
	nautilus-rename-file-popover-controller.h \
	nautilus-new-folder-dialog-controller.c \
	nautilus-new-folder-dialog-controller.h \
	nautilus-compress-dialog-controller.c \
	nautilus-compress-dialog-controller.h \
	nautilus-operations-ui-manager.c \
	nautilus-operations-ui-manager.h \
	nautilus-file-operations.c \
	nautilus-file-operations.h \
	nautilus-file-private.h \
	nautilus-file-queue.c \
	nautilus-file-queue.h \
	nautilus-file-utilities.c \
	nautilus-file-utilities.h \
	nautilus-file.c \
	nautilus-file.h \
	nautilus-global-preferences.c \
	nautilus-global-preferences.h \
	nautilus-icon-info.c \
	nautilus-icon-info.h \
	nautilus-icon-names.h \
	nautilus-keyfile-metadata.c \
	nautilus-keyfile-metadata.h \
	nautilus-lib-self-check-functions.c \
	nautilus-lib-self-check-functions.h \
	nautilus-link.c \
	nautilus-link.h \
	nautilus-metadata.h \
	nautilus-metadata.c \
	nautilus-mime-application-chooser.c \
	nautilus-mime-application-chooser.h \
	nautilus-module.c \
	nautilus-module.h \
	nautilus-monitor.c \
	nautilus-monitor.h \
	nautilus-profile.c \
	nautilus-profile.h \
	nautilus-progress-info.c \
	nautilus-progress-info.h \
	nautilus-progress-info-manager.c \
	nautilus-progress-info-manager.h \
	nautilus-program-choosing.c \
	nautilus-program-choosing.h \
	nautilus-recent.c \
	nautilus-recent.h \
	nautilus-search-directory.c \
	nautilus-search-directory.h \
	nautilus-search-directory-file.c \
	nautilus-search-directory-file.h \
	nautilus-search-provider.c \
	nautilus-search-provider.h \
	nautilus-search-engine.c \
	nautilus-search-engine.h \
	nautilus-search-engine-model.c \
	nautilus-search-engine-model.h \
	nautilus-search-engine-simple.c \
	nautilus-search-engine-simple.h \
	nautilus-search-hit.c \
	nautilus-search-hit.h \
	nautilus-selection-canvas-item.c \
	nautilus-selection-canvas-item.h \
	nautilus-signaller.h \
	nautilus-signaller.c \
	nautilus-query.c \
	nautilus-query.h \
	nautilus-thumbnails.c \
	nautilus-thumbnails.h \
	nautilus-trash-monitor.c \
	nautilus-trash-monitor.h \
	nautilus-tree-view-drag-dest.c \
	nautilus-tree-view-drag-dest.h \
	nautilus-ui-utilities.c \
	nautilus-ui-utilities.h \
	nautilus-video-mime-types.h \
	nautilus-vfs-directory.c \
	nautilus-vfs-directory.h \
	nautilus-vfs-file.c \
	nautilus-vfs-file.h \
	nautilus-file-undo-operations.c \
	nautilus-file-undo-operations.h \
	nautilus-file-undo-manager.c \
	nautilus-file-undo-manager.h \
	$(nautilus_tracker_engine_sources)	\
	$(nautilus_batch_renaming_tracker_sources)	\
	$(NULL)

libnautilus_la_SOURCES = \
	$(nautilus_built_sources) \
	$(nautilus_no_main_sources) \
	$(NULL)

nautilus_SOURCES = \
	nautilus-main.c				\
	$(NULL)

EMPTY_VIEW_SOURCES = \
	nautilus-empty-view.c 			\
	nautilus-empty-view.h

if ENABLE_EMPTY_VIEW
nautilus_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

nautilus_autorun_software_SOURCES= 			\
	nautilus-autorun-software.c			\
	$(NULL)

BUILT_SOURCES = 					\
	$(nautilus_built_sources) 			\
	$(NULL)

TESTS=check-nautilus

CLEANFILES = \
	$(BUILT_SOURCES)			\
	$(server_DATA)				\
	$(NULL)

EXTRA_DIST =					\
	$(resource_files)			\
	$(server_in_files)			\
	resources/nautilus.gresource.xml	\
	check-nautilus				\
	nautilus-enum-types.h.template		\
	nautilus-enum-types.c.template		\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
