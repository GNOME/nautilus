include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-private.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/cut-n-paste-code		\
	$(CORE_CFLAGS)					\
	$(WARNING_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(TRACKER_CFLAGS)				\
	$(BEAGLE_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 	\
	-DNAUTILUS_EXTENSIONDIR=\""$(libdir)/nautilus/extensions-2.0"\" \
	$(NULL)

BUILT_SOURCES = \
	nautilus-marshal.c \
	nautilus-marshal.h \
	$(NULL)

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/libegg/libegg.la \
        $(NULL)

libnautilus_private_la_LDFLAGS =	\
	-no-undefined			\
	$(NULL)

libnautilus_private_la_LIBADD =		\
	$(dependency_static_libs)	\
	$(SELINUX_LIBS)			\
	$(BEAGLE_LIBS)			\
	$(TRACKER_LIBS)			\
	$(top_builddir)/eel/libeel-2.la \
	$(top_builddir)/libnautilus-extension/libnautilus-extension.la \
	$(CORE_LIBS)			\
	$(NULL)

libnautilus_private_la_SOURCES = \
	nautilus-autorun.c \
	nautilus-autorun.h \
	nautilus-bookmark.c \
	nautilus-bookmark.h \
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
	nautilus-directory-notify.h \
	nautilus-directory-private.h \
	nautilus-directory.c \
	nautilus-directory.h \
	nautilus-dnd.c \
	nautilus-dnd.h \
	nautilus-entry.c \
	nautilus-entry.h \
	nautilus-file-attributes.h \
	nautilus-file-changes-queue.c \
	nautilus-file-changes-queue.h \
	nautilus-file-conflict-dialog.c \
	nautilus-file-conflict-dialog.h \
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
	nautilus-keep-last-vertical-box.c \
	nautilus-keep-last-vertical-box.h \
	nautilus-lib-self-check-functions.c \
	nautilus-lib-self-check-functions.h \
	nautilus-link.c \
	nautilus-link.h \
	nautilus-merged-directory.c \
	nautilus-merged-directory.h \
	nautilus-metadata.h \
	nautilus-metadata.c \
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
	nautilus-search-engine-beagle.c \
	nautilus-search-engine-beagle.h \
	nautilus-search-engine-tracker.c \
	nautilus-search-engine-tracker.h \
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
	nautilus-window-slot-info.c \
	nautilus-window-slot-info.h \
	$(NULL)

nodist_libnautilus_private_la_SOURCES =\
	$(BUILT_SOURCES) \
	$(NULL)

$(lib_LTLIBRARIES): $(dependency_static_libs)

nautilus-marshal.list: $(libnautilus_private_la_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*nautilus_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libnautilus_private_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.h: %-marshal.list Makefile
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	$(AM_V_GEN)echo "#include \"nautilus-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< >> $*-marshal.c

gsettingsschema_in_files = org.gnome.nautilus.gschema.xml.in org.gnome.media-handling.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

convertdir = $(datadir)/GConf/gsettings
convert_DATA = nautilus.convert

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

EXTRA_DIST =				\
	nautilus-marshal.list		\
	nautilus.convert		\
	$(gsettingsschema_in_files)     \
	$(BEAGLE_SOURCES)		\
	$(TRACKER_SOURCES)		\
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)				\
	$(gsettings_SCHEMAS)				\
	nautilus-marshal.list				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
