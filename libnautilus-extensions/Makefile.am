NULL=

lib_LTLIBRARIES=libnautilus-extensions.la

INCLUDES= -I$(top_srcdir) -I$(top_builddir) -I$(top_builddir)/libnautilus -I. \
	$(GNOME_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT

libnautilus_extensions_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(OAF_LIBS) \
	$(BONOBO_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG) \
	$(top_src_dir)/libnautilus/.libs/libnautilus.so

libnautilus_extensionsincludedir=$(includedir)/libnautilus-extensions

libnautilus_extensionsinclude_HEADERS=		\
	nautilus-background-canvas-group.h	\
	nautilus-file-changes-queue.h		\
	nautilus-icon-canvas-item.h		\
	nautilus-icon-dnd.h			\
	nautilus-icon-grid.h			\
	bonobo-stream-vfs.h			\
	gtkscrollframe.h			\
	libnautilus-extensions.h		\
	nautilus-alloc.h			\
	nautilus-background.h			\
	nautilus-bonobo-extensions.h		\
	nautilus-bookmark.h			\
	nautilus-debug.h			\
	nautilus-directory-background.h		\
	nautilus-directory.h			\
	nautilus-entry.h			\
	nautilus-file-attributes.h		\
	nautilus-file-utilities.h		\
	nautilus-file.h				\
	nautilus-gdk-extensions.h		\
	nautilus-gdk-pixbuf-extensions.h	\
	nautilus-glib-extensions.h		\
	nautilus-global-preferences.h		\
	nautilus-gnome-extensions.h		\
	nautilus-graphic-effects.h		\
	nautilus-gtk-extensions.h		\
	nautilus-gtk-macros.h			\
	nautilus-icon-container.h		\
	nautilus-icon-factory.h			\
	nautilus-icon-text-item.h		\
	nautilus-keep-last-vertical-box.h	\
	nautilus-link.h				\
	nautilus-list-column-title.h		\
	nautilus-list.h                         \
	nautilus-metadata.h			\
	nautilus-mime-type.h			\
	nautilus-self-checks.h			\
	nautilus-string-list.h			\
	nautilus-string.h			\
	nautilus-view-identifier.h		\
	nautilus-xml-extensions.h		\
	$(NULL)

libnautilus_extensions_la_SOURCES = \
	nautilus-file-changes-queue-private.h	\
	nautilus-icon-private.h			\
	nautilus-file-private.h			\
	nautilus-directory-private.h		\
	nautilus-directory-metafile.h		\
	nautilus-lib-self-check-functions.h	\
	bonobo-stream-vfs.c			\
	gtkscrollframe.c			\
	nautilus-background-canvas-group.c	\
	nautilus-background.c			\
	nautilus-bonobo-extensions.c		\
	nautilus-bookmark.c			\
	nautilus-debug.c			\
	nautilus-default-file-icon.c		\
	nautilus-default-file-icon.h		\
	nautilus-directory-async.c		\
	nautilus-directory-background.c		\
	nautilus-directory-metafile.c		\
	nautilus-directory.c			\
	nautilus-entry.c			\
	nautilus-file-changes-queue.c		\
	nautilus-file-utilities.c		\
	nautilus-file.c				\
	nautilus-gdk-extensions.c		\
	nautilus-gdk-pixbuf-extensions.c	\
	nautilus-glib-extensions.c		\
	nautilus-global-preferences.c		\
	nautilus-gnome-extensions.c		\
	nautilus-graphic-effects.c		\
	nautilus-gtk-extensions.c		\
	nautilus-icon-canvas-item.c       	\
	nautilus-icon-container.c		\
	nautilus-icon-dnd.c			\
	nautilus-icon-factory.c			\
	nautilus-icon-grid.c			\
	nautilus-icon-text-item.c		\
	nautilus-keep-last-vertical-box.c	\
	nautilus-lib-self-check-functions.c	\
	nautilus-link.c				\
	nautilus-list-column-title.c		\
	nautilus-list.c                         \
	nautilus-mime-type.c			\
	nautilus-self-checks.c			\
	nautilus-string-list.c			\
	nautilus-string.c			\
	nautilus-view-identifier.c		\
	nautilus-xml-extensions.c		\
	$(NULL)
