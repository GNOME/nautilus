NULL=

lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(GNORBA_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT

libnautilus_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(GNORBA_LIBS) \
	$(BONOBO_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG)

nautilus_idl_sources=nautilus-stubs.c nautilus-skels.c nautilus.h nautilus-common.c
fsextension_idl_sources=fsextension-stubs.c fsextension-skels.c fsextension-common.c fsextension.h
BUILT_SOURCES=$(nautilus_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus
libnautilusinclude_HEADERS=			\
	bonobo-stream-vfs.h			\
	gtkscrollframe.h			\
	libnautilus.h				\
	nautilus-alloc.h			\
	nautilus-background.h			\
	nautilus-bonobo-extensions.h		\
	nautilus-bookmark.h			\
	nautilus-debug.h			\
	nautilus-directory.h			\
	nautilus-file-utilities.h		\
	nautilus-file.h				\
	nautilus-gdk-extensions.h		\
	nautilus-gdk-pixbuf-extensions.h	\
	nautilus-glib-extensions.h		\
	nautilus-global-preferences.h		\
	nautilus-gnome-extensions.h		\
	nautilus-graphic-effects.h		\
	nautilus-gtk-extensions.h		\
	nautilus-icon-container.h		\
	nautilus-icon-factory.h			\
	nautilus-list-column-title.h		\
	nautilus-list.h                         \
	nautilus-metadata.h			\
	nautilus-mime-type.h			\
	nautilus-self-checks.h			\
	nautilus-string-list.h			\
	nautilus-string.h			\
	nautilus-xml-extensions.h		\
	nautilus-zoomable.h			\
	nautilus.h				\
	ntl-content-view-frame.h		\
	ntl-meta-view-frame.h			\
	ntl-view-frame.h			\
	$(NULL)

libnautilus_la_SOURCES=$(nautilus_idl_sources)	\
	bonobo-stream-vfs.c			\
	gtkscrollframe.c			\
	nautilus-background-canvas-group.c	\
	nautilus-background.c			\
	nautilus-bonobo-extensions.c		\
	nautilus-bookmark.c			\
	nautilus-debug.c			\
	nautilus-default-file-icon.c		\
	nautilus-default-file-icon.h		\
	nautilus-directory.c			\
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
	nautilus-lib-self-check-functions.c	\
	nautilus-list-column-title.c		\
	nautilus-list.c                         \
	nautilus-mime-type.c			\
	nautilus-self-checks.c			\
	nautilus-string-list.c			\
	nautilus-string.c			\
	nautilus-xml-extensions.c		\
	nautilus-zoomable.c			\
	ntl-content-view-frame.c		\
	ntl-meta-view-frame.c			\
	ntl-view-frame.c			\
	$(fsextension_idl_sources)		\
	$(NULL)

$(nautilus_idl_sources): nautilus_idl_stamp
$(fsextension_idl_sources): fsextension_idl_stamp

nautilus_idl_stamp: $(top_srcdir)/idl/nautilus.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/nautilus.idl
	touch nautilus_idl_stamp

fsextension_idl_stamp: $(top_srcdir)/idl/fsextension.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/fsextension.idl
	touch fsextension_idl_stamp

CLEANFILES=nautilus_idl_stamp fsextension_idl_stamp
