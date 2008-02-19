include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-private.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/cut-n-paste-code		\
	$(CORE_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(TRACKER_CFLAGS)				\
	$(BEAGLE_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 	\
	-DNAUTILUS_EXTENSIONDIR=\""$(libdir)/nautilus/extensions-2.0"\" \
	$(NULL)

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/libegg/libegg.la \
	$(top_builddir)/libbackground/libbackground.la \
        $(NULL)

libnautilus_private_la_LDFLAGS =	\
	-version-info 2:0:0		\
	-no-undefined			\
	$(NULL)

libnautilus_private_la_LIBADD =		\
	$(dependency_static_libs)	\
	$(SELINUX_LIBS)			\
	$(BEAGLE_LIBS)			\
	$(TRACKER_LIBS)			\
	$(top_builddir)/libnautilus-extension/libnautilus-extension.la \
	$(CORE_LIBS)			\
	$(NULL)

marshal_sources = \
	nautilus-marshal.h \
	nautilus-marshal-guts.c \
	$(NULL)

libnautilus_private_la_SOURCES = \
	nautilus-autorun.c \
	nautilus-autorun.h \
	nautilus-bookmark.c \
	nautilus-bookmark.h \
	nautilus-cell-renderer-pixbuf-emblem.c \
	nautilus-cell-renderer-pixbuf-emblem.h \
	nautilus-cell-renderer-text-ellipsized.c \
	nautilus-cell-renderer-text-ellipsized.h \
	nautilus-clipboard-monitor.c \
	nautilus-clipboard-monitor.h \
	nautilus-clipboard.c \
	nautilus-clipboard.h \
	nautilus-column-chooser.c \
	nautilus-column-chooser.h \
	nautilus-column-utilities.c \
	nautilus-column-utilities.h \
	nautilus-customization-data.c \
	nautilus-customization-data.h \
	nautilus-debug-log.c \
	nautilus-debug-log.h \
	nautilus-default-file-icon.c \
	nautilus-default-file-icon.h \
	nautilus-desktop-directory-file.c \
	nautilus-desktop-directory-file.h \
	nautilus-desktop-directory.c \
	nautilus-desktop-directory.h \
	nautilus-desktop-icon-file.c \
	nautilus-desktop-icon-file.h \
	nautilus-desktop-link-monitor.c \
	nautilus-desktop-link-monitor.h \
	nautilus-desktop-link.c \
	nautilus-desktop-link.h \
	nautilus-directory-async.c \
	nautilus-directory-background.c \
	nautilus-directory-background.h \
	nautilus-directory-metafile.c \
	nautilus-directory-metafile.h \
	nautilus-directory-notify.h \
	nautilus-directory-private.h \
	nautilus-directory.c \
	nautilus-directory.h \
	nautilus-dnd.c \
	nautilus-dnd.h \
	nautilus-emblem-utils.c \
	nautilus-emblem-utils.h \
	nautilus-entry.c \
	nautilus-entry.h \
	nautilus-file-attributes.h \
	nautilus-file-changes-queue.c \
	nautilus-file-changes-queue.h \
	nautilus-file-dnd.c \
	nautilus-file-dnd.h \
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
	nautilus-horizontal-splitter.c \
	nautilus-horizontal-splitter.h \
	nautilus-icon-canvas-item.c \
	nautilus-icon-canvas-item.h \
	nautilus-icon-container.c \
	nautilus-icon-container.h \
	nautilus-icon-dnd.c \
	nautilus-icon-dnd.h \
	nautilus-icon-private.h \
	nautilus-icon-info.c \
	nautilus-icon-info.h \
	nautilus-icon-names.h \
	nautilus-idle-queue.c \
	nautilus-idle-queue.h \
	nautilus-iso9660.h \
	nautilus-keep-last-vertical-box.c \
	nautilus-keep-last-vertical-box.h \
	nautilus-lib-self-check-functions.c \
	nautilus-lib-self-check-functions.h \
	nautilus-link.c \
	nautilus-link.h \
	nautilus-marshal.c \
	nautilus-marshal.h \
	nautilus-merged-directory.c \
	nautilus-merged-directory.h \
	nautilus-metadata.h \
	nautilus-metafile.c \
	nautilus-metafile.h \
	nautilus-mime-actions.c \
	nautilus-mime-actions.h \
	nautilus-mime-application-chooser.c \
	nautilus-mime-application-chooser.h \
	nautilus-module.c \
	nautilus-module.h \
	nautilus-monitor.c \
	nautilus-monitor.h \
	nautilus-open-with-dialog.c \
	nautilus-open-with-dialog.h \
	nautilus-progress-info.c \
	nautilus-progress-info.h \
	nautilus-program-choosing.c \
	nautilus-program-choosing.h \
	nautilus-recent.c \
	nautilus-recent.h \
	nautilus-saved-search-file.c \
	nautilus-saved-search-file.h \
	nautilus-search-directory.c \
	nautilus-search-directory.h \
	nautilus-search-directory-file.c \
	nautilus-search-directory-file.h \
	nautilus-search-engine.c \
	nautilus-search-engine.h \
	nautilus-search-engine-simple.c \
	nautilus-search-engine-simple.h \
	nautilus-sidebar-provider.c \
	nautilus-sidebar-provider.h \
	nautilus-sidebar.c \
	nautilus-sidebar.h \
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
	nautilus-undo-manager.c \
	nautilus-undo-manager.h \
	nautilus-undo-private.h \
	nautilus-undo-signal-handlers.c \
	nautilus-undo-signal-handlers.h \
	nautilus-undo-transaction.c \
	nautilus-undo-transaction.h \
	nautilus-undo.c \
	nautilus-undo.h \
	nautilus-users-groups-cache.c \
	nautilus-users-groups-cache.h \
	nautilus-vfs-directory.c \
	nautilus-vfs-directory.h \
	nautilus-vfs-file.c \
	nautilus-vfs-file.h \
	nautilus-view-factory.c \
	nautilus-view-factory.h \
	nautilus-view.c \
	nautilus-view.h \
	nautilus-window-info.c \
	nautilus-window-info.h \
	$(NULL)

BEAGLE_SOURCES = \
	nautilus-search-engine-beagle.c \
	nautilus-search-engine-beagle.h

if HAVE_BEAGLE
libnautilus_private_la_SOURCES += $(BEAGLE_SOURCES)
endif

TRACKER_SOURCES = \
	nautilus-search-engine-tracker.c \
	nautilus-search-engine-tracker.h

if HAVE_TRACKER
libnautilus_private_la_SOURCES += $(TRACKER_SOURCES)
endif

$(lib_LTLIBRARIES): $(dependency_static_libs)

nautilus-marshal.h: nautilus-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=nautilus_marshal > $@
nautilus-marshal-guts.c: nautilus-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=nautilus_marshal > $@

$(libnautilus_private_la_OBJECTS): $(marshal_sources)


schema_in_files =  apps_nautilus_preferences.schemas.in
schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

# don't do this if we are building in eg. rpm
if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi
endif

EXTRA_DIST = 				\
	nautilus-marshal.list		\
	$(schema_in_files)              \
	$(BEAGLE_SOURCES)		\
	$(TRACKER_SOURCES)		\
	$(NULL)

CLEANFILES = \
	$(marshal_sources)				\
	$(schema_DATA)			                \
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

