include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnautilus-private.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	$(CORE_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	-DDATADIR=\""$(datadir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 	\
	$(NULL)

dependency_static_libs = \
        $(top_builddir)/cut-n-paste-code/widgets/gimphwrapbox/libgtkhwrapbox.la \
        $(NULL)

libnautilus_private_la_LDFLAGS =	\
	$(dependency_static_libs)	\
	$(CORE_LIBS)			\
	$(NULL)

nautilus_metafile_server_idl_sources = \
	nautilus-metafile-server-stubs.c \
	nautilus-metafile-server-skels.c \
	nautilus-metafile-server.h \
	nautilus-metafile-server-common.c \
	$(NULL)

marshal_sources = \
	nautilus-marshal.h \
	nautilus-marshal-guts.c \
	$(NULL)

libnautilus_private_la_SOURCES = \
	$(nautilus_metafile_server_idl_sources)	\
	nautilus-audio-player.c \
	nautilus-audio-player.h \
	nautilus-authn-manager.c \
	nautilus-authn-manager.h \
	nautilus-bonobo-extensions.c \
	nautilus-bonobo-extensions.h \
	nautilus-bookmark.c \
	nautilus-bookmark.h \
	nautilus-cdrom-extensions.h \
	nautilus-customization-data.c \
	nautilus-customization-data.h \
	nautilus-default-file-icon.c \
	nautilus-default-file-icon.h \
	nautilus-desktop-file-loader.c \
	nautilus-desktop-file-loader.h \
	nautilus-directory-async.c \
	nautilus-directory-background.c \
	nautilus-directory-background.h \
	nautilus-directory-metafile-monitor.c \
	nautilus-directory-metafile-monitor.h \
	nautilus-directory-metafile.c \
	nautilus-directory-metafile.h \
	nautilus-directory-notify.h \
	nautilus-directory-private.h \
	nautilus-directory.c \
	nautilus-directory.h \
	nautilus-dnd.c \
	nautilus-dnd.h \
	nautilus-drag-window.c \
	nautilus-drag-window.h \
	nautilus-entry.c \
	nautilus-entry.h \
	nautilus-file-attributes.h \
	nautilus-file-changes-queue.c \
	nautilus-file-changes-queue.h \
	nautilus-file-dnd.c \
	nautilus-file-dnd.h \
	nautilus-file-operations-progress.c \
	nautilus-file-operations-progress.h \
	nautilus-file-operations.c \
	nautilus-file-operations.h \
	nautilus-file-private.h \
	nautilus-file-queue.c \
	nautilus-file-queue.h \
	nautilus-file-utilities.c \
	nautilus-file-utilities.h \
	nautilus-file.c \
	nautilus-file.h \
	nautilus-find-icon-image.c \
	nautilus-find-icon-image.h \
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
	nautilus-icon-factory-private.h \
	nautilus-icon-factory.c \
	nautilus-icon-factory.h \
	nautilus-icon-private.h \
	nautilus-icon-text-item.c \
	nautilus-icon-text-item.h \
	nautilus-iso9660.h \
	nautilus-keep-last-vertical-box.c \
	nautilus-keep-last-vertical-box.h \
	nautilus-lib-self-check-functions.c \
	nautilus-lib-self-check-functions.h \
	nautilus-link-desktop-file.c \
	nautilus-link-desktop-file.h \
	nautilus-link-historical.c \
	nautilus-link-historical.h \
	nautilus-link.c \
	nautilus-link.h \
	nautilus-marshal.c \
	nautilus-marshal.h \
	nautilus-medusa-support.c \
	nautilus-medusa-support.h \
	nautilus-merged-directory.c \
	nautilus-merged-directory.h \
	nautilus-metadata.h \
	nautilus-metafile-factory.c \
	nautilus-metafile-factory.h \
	nautilus-metafile.c \
	nautilus-metafile.h \
	nautilus-mime-actions.c \
	nautilus-mime-actions.h \
	nautilus-monitor.c \
	nautilus-monitor.h \
	nautilus-program-chooser.c \
	nautilus-program-chooser.h \
	nautilus-program-choosing.c \
	nautilus-program-choosing.h \
	nautilus-search-uri.c \
	nautilus-search-uri.h \
	nautilus-sidebar-functions.c \
	nautilus-sidebar-functions.h \
	nautilus-sound.c \
	nautilus-sound.h \
	nautilus-theme.c \
	nautilus-theme.h \
	nautilus-thumbnails-jpeg.c \
	nautilus-thumbnails-jpeg.h \
	nautilus-thumbnails.c \
	nautilus-thumbnails.h \
	nautilus-trash-directory.c \
	nautilus-trash-directory.h \
	nautilus-trash-file.c \
	nautilus-trash-file.h \
	nautilus-trash-monitor.c \
	nautilus-trash-monitor.h \
	nautilus-undo-context.c \
	nautilus-undo-context.h \
	nautilus-undo-manager.c \
	nautilus-undo-manager.h \
	nautilus-undo-signal-handlers.c \
	nautilus-undo-signal-handlers.h \
	nautilus-vfs-directory.c \
	nautilus-vfs-directory.h \
	nautilus-vfs-file.c \
	nautilus-vfs-file.h \
	nautilus-view-identifier.c \
	nautilus-view-identifier.h \
	nautilus-volume-monitor.c \
	nautilus-volume-monitor.h \
	$(NULL)

fsattributesdir = $(datadir)/nautilus
fsattributes_DATA = filesystem-attributes.xml

$(lib_LTLIBRARIES): $(dependency_static_libs)

$(nautilus_metafile_server_idl_sources): nautilus_metafile_server_idl_stamp
nautilus_metafile_server_idl_stamp: nautilus-metafile-server.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I$(top_srcdir) $(CORE_IDL_INCLUDES) $<
	touch $@

nautilus-marshal.h: nautilus-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=nautilus_marshal > $@
nautilus-marshal-guts.c: nautilus-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=nautilus_marshal > $@

$(libnautilus_private_la_OBJECTS): nautilus_metafile_server_idl_stamp $(marshal_sources)

EXTRA_DIST = 				\
	$(fsattributes_DATA)		\
	nautilus-metafile-server.idl	\
	nautilus-marshal.list		\
	$(NULL)

CLEANFILES = \
	$(nautilus_metafile_server_idl_sources)		\
	nautilus_metafile_server_idl_stamp		\
	$(marshal_sources)				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
