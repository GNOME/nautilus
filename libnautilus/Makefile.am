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
	$(XML_LIBS)

nautilus_idl_sources=nautilus-stubs.c nautilus-skels.c nautilus.h nautilus-common.c
fsextension_idl_sources=fsextension-stubs.c fsextension-skels.c fsextension-common.c fsextension.h
BUILT_SOURCES=$(nautilus_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus
libnautilusinclude_HEADERS=			\
	bonobo-stream-vfs.h			\
	gdk-extensions.h			\
	gnome-icon-container-dnd.h		\
	gnome-icon-container-private.h		\
	gnome-icon-container.h			\
	gtkflist.h				\
	gtkscrollframe.h			\
	nautilus.h				\
	nautilus-alloc.h			\
	nautilus-background.h			\
	nautilus-bookmark.h			\
	nautilus-debug.h			\
	nautilus-directory.h			\
	nautilus-file-utilities.h		\
	nautilus-glib-extensions.h		\
	nautilus-gtk-extensions.h		\
	nautilus-icon-factory.h			\
	nautilus-icons-controller.h		\
	nautilus-icons-view-icon-item.h         \
        nautilus-lib-self-check-functions.h	\
	nautilus-metadata.h			\
	nautilus-self-checks.h			\
	nautilus-string.h			\
	ntl-content-view-frame.h		\
	ntl-meta-view-frame.h			\
	ntl-view-frame.h

libnautilus_la_SOURCES=$(nautilus_idl_sources)	\
	bonobo-stream-vfs.c			\
	gdk-extensions.c			\
	gnome-icon-container-dnd.c		\
	gnome-icon-container.c			\
	gtkflist.c				\
	gtkscrollframe.c			\
	nautilus-background.c			\
	nautilus-background-canvas-group.c	\
	nautilus-bookmark.c			\
	nautilus-debug.c			\
	nautilus-default-file-icon.c		\
	nautilus-default-file-icon.h		\
	nautilus-directory.c			\
	nautilus-file-utilities.c		\
	nautilus-glib-extensions.c		\
	nautilus-gtk-extensions.c		\
	nautilus-icon-factory.c			\
	nautilus-icons-controller.c		\
	nautilus-icons-view-icon-item.c         \
	nautilus-lib-self-check-functions.c	\
	nautilus-self-checks.c			\
	nautilus-string.c			\
	ntl-content-view-frame.c		\
	ntl-meta-view-frame.c			\
	ntl-view-frame.c			\
	$(fsextension_idl_sources)

$(nautilus_idl_sources): nautilus_idl_stamp
$(fsextension_idl_sources): fsextension_idl_stamp

nautilus_idl_stamp: $(top_srcdir)/idl/nautilus.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/nautilus.idl
	touch nautilus_idl_stamp

fsextension_idl_stamp: $(top_srcdir)/idl/fsextension.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/fsextension.idl
	touch fsextension_idl_stamp

CLEANFILES=nautilus_idl_stamp fsextension_idl_stamp
