include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnautilus-private.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/cut-n-paste-code/widgets/e-paned	\
	$(NAUTILUS_PRIVATE_CFLAGS)			\
	$(ESD_CFLAGS)					\
	$(MEDUSA_CFLAGS) 				\
	$(LIBRSVG_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 	\
	$(NULL)

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la \
        $(top_builddir)/cut-n-paste-code/widgets/gimphwrapbox/libgtkhwrapbox.la \
        $(NULL)

if HAVE_CDDA
CDDALIBADD = -lcdda_paranoia -lcdda_interface
else
CDDALIBADD =
endif

libnautilus_private_la_LDFLAGS =	\
	$(dependency_static_libs)	\
	$(ESD_LIBS)			\
	$(LIBJPEG) 			\
	$(MEDUSA_LIBS) 			\
	$(FAM_LIBS) 			\
	$(ICONV_LIBS)			\
	$(CDDALIBADD)			\
	$(NULL)

nautilus_metafile_server_idl_sources = \
	nautilus-metafile-server-stubs.c \
	nautilus-metafile-server-skels.c \
	nautilus-metafile-server.h \
	nautilus-metafile-server-common.c \
	$(NULL)

libnautilus_private_la_SOURCES = \
	$(nautilus_metafile_server_idl_sources)	\
	nautilus-audio-player.c \
	nautilus-bonobo-extensions.c \
	nautilus-bookmark.c \
	nautilus-customization-data.c \
	nautilus-default-file-icon.c \
	nautilus-desktop-file-loader.c \
	nautilus-directory-async.c \
	nautilus-directory-background.c \
	nautilus-directory-metafile-monitor.c \
	nautilus-directory-metafile.c \
	nautilus-directory.c \
	nautilus-drag-window.c \
	nautilus-druid-page-eazel.c \
	nautilus-druid.c \
	nautilus-entry.c \
	nautilus-file-changes-queue.c \
	nautilus-file-dnd.c \
	nautilus-file-operations-progress.c \
	nautilus-file-queue.c \
	nautilus-file-operations.c \
	nautilus-file-utilities.c \
	nautilus-file.c \
	nautilus-find-icon-image.c \
	nautilus-font-factory.c \
	nautilus-global-preferences.c \
	nautilus-horizontal-splitter.c \
	nautilus-icon-canvas-item.c \
	nautilus-icon-container.c \
	nautilus-icon-dnd.c \
	nautilus-icon-factory.c \
	nautilus-icon-text-item.c \
	nautilus-keep-last-vertical-box.c \
	nautilus-lib-self-check-functions.c \
	nautilus-link-set.c \
	nautilus-link.c \
	nautilus-link-desktop-file.c \
	nautilus-link-historical.c \
	nautilus-medusa-support.c \
	nautilus-merged-directory.c \
	nautilus-metafile-factory.c \
	nautilus-metafile.c \
	nautilus-mime-actions.c \
	nautilus-monitor.c \
	nautilus-program-chooser.c \
	nautilus-program-choosing.c \
	nautilus-search-uri.c \
	nautilus-sidebar-functions.c \
	nautilus-sound.c \
	nautilus-theme.c \
	nautilus-thumbnails.c \
	nautilus-thumbnails-jpeg.c \
	nautilus-trash-directory.c \
	nautilus-trash-file.c \
	nautilus-trash-monitor.c \
	nautilus-undo-context.c \
	nautilus-undo-manager.c \
	nautilus-undo-signal-handlers.c \
	nautilus-vfs-directory.c \
	nautilus-vfs-file.c \
	nautilus-view-identifier.c \
	nautilus-volume-monitor.c \
	nautilus-authn-manager.c \
	nautilus-audio-player.h \
	nautilus-bonobo-extensions.h \
	nautilus-bookmark.h \
	nautilus-cdrom-extensions.h \
	nautilus-customization-data.h \
	nautilus-default-file-icon.h \
	nautilus-desktop-file-loader.h \
	nautilus-directory-background.h \
	nautilus-directory-metafile-monitor.h \
	nautilus-directory-metafile.h \
	nautilus-directory-notify.h \
	nautilus-directory-private.h \
	nautilus-directory.h \
	nautilus-drag-window.h \
	nautilus-druid-page-eazel.h \
	nautilus-druid.h \
	nautilus-entry.h \
	nautilus-file-attributes.h \
	nautilus-file-changes-queue.h \
	nautilus-file-dnd.h \
	nautilus-file-operations-progress.h \
	nautilus-file-operations.h \
	nautilus-file-private.h \
	nautilus-file-utilities.h \
	nautilus-file.h \
	nautilus-file-queue.h \
	nautilus-find-icon-image.h \
	nautilus-font-factory.h \
	nautilus-global-preferences.h \
	nautilus-horizontal-splitter.h \
	nautilus-icon-canvas-item.h \
	nautilus-icon-container.h \
	nautilus-icon-dnd.h \
	nautilus-icon-factory-private.h \
	nautilus-icon-factory.h \
	nautilus-icon-private.h \
	nautilus-icon-text-item.h \
	nautilus-iso9660.h \
	nautilus-keep-last-vertical-box.h \
	nautilus-lib-self-check-functions.h \
	nautilus-link-set.h \
	nautilus-link.h \
	nautilus-link-desktop-file.h \
	nautilus-link-historical.h \
	nautilus-medusa-support.h \
	nautilus-merged-directory.h \
	nautilus-metadata.h \
	nautilus-metafile-factory.h \
	nautilus-metafile.h \
	nautilus-mime-actions.h \
	nautilus-monitor.h \
	nautilus-program-chooser.h \
	nautilus-program-choosing.h \
	nautilus-search-uri.h \
	nautilus-sidebar-functions.h \
	nautilus-sound.h \
	nautilus-theme.h \
	nautilus-thumbnails.h \
	nautilus-thumbnails-jpeg.h \
	nautilus-trash-directory.h \
	nautilus-trash-file.h \
	nautilus-trash-monitor.h \
	nautilus-undo-context.h \
	nautilus-undo-manager.h \
	nautilus-undo-signal-handlers.h \
	nautilus-vfs-directory.h \
	nautilus-vfs-file.h \
	nautilus-view-identifier.h \
	nautilus-volume-monitor.h \
	nautilus-authn-manager.h \
	$(NULL)

fsattributesdir = $(datadir)/nautilus
fsattributes_DATA = filesystem-attributes.xml

$(lib_LTLIBRARIES): $(dependency_static_libs)

$(nautilus_metafile_server_idl_sources): nautilus_metafile_server_idl_stamp
nautilus_metafile_server_idl_stamp: nautilus-metafile-server.idl $(ORBIT_IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(srcdir)/nautilus-metafile-server.idl
	touch nautilus_metafile_server_idl_stamp

$(libnautilus_private_la_OBJECTS): nautilus_metafile_server_idl_stamp

EXTRA_DIST = 				\
	$(fsattributes_DATA)		\
	nautilus-metafile-server.idl	\
	$(NULL)

BUILT_SOURCES = \
	$(nautilus_metafile_server_idl_sources)		\
	$(NULL)

CLEANFILES = \
	$(nautilus_metafile_server_idl_sources)		\
	nautilus_metafile_server_idl_stamp		\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
