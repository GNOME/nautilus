lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(GNORBA_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR)

libnautilus_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(GNORBA_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS)

nautilus_idl_sources=nautilus-stubs.c nautilus-skels.c nautilus.h nautilus-common.c
fsextension_idl_sources=fsextension-stubs.c fsextension-skels.c fsextension-common.c fsextension.h
BUILT_SOURCES=$(nautilus_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus
libnautilusinclude_HEADERS=			\
	gdk-extensions.h			\
	gnome-icon-container-dnd.h		\
	gnome-icon-container-private.h		\
	gnome-icon-container.h			\
	gnome-progressive-loader.c		\
	gnome-progressive-loader.h		\
	gtkflist.h				\
	gtkscrollframe.h			\
	nautilus.h				\
	nautilus-background.h			\
	nautilus-debug.h			\
	nautilus-directory.h			\
	nautilus-file-utilities.h		\
	nautilus-glib-extensions.h		\
	nautilus-gtk-extensions.h		\
	nautilus-icons-controller.h		\
	nautilus-icons-view-icon-item.h         \
        nautilus-lib-self-check-functions.h	\
	nautilus-self-checks.h			\
	nautilus-string.h			\
	ntl-content-view-frame.h		\
	ntl-meta-view-frame.h			\
	ntl-view-frame.h

libnautilus_la_SOURCES=$(nautilus_idl_sources)	\
	gdk-extensions.c			\
	gnome-icon-container-dnd.c		\
	gnome-icon-container.c			\
	gtkflist.c				\
	gtkscrollframe.c			\
	nautilus-background.c			\
	nautilus-background-canvas-group.c	\
	nautilus-debug.c			\
	nautilus-directory.c			\
	nautilus-file-utilities.c		\
	nautilus-glib-extensions.c		\
	nautilus-gtk-extensions.c		\
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
