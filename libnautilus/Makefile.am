lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOME_CFLAGS) $(GNORBA_CFLAGS)

nautilus_idl_sources=nautilus-stubs.c nautilus-skels.c nautilus.h nautilus-common.c
BUILT_SOURCES=$(nautilus_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus
libnautilusinclude_HEADERS=			\
	ntl-view-client.h			\
	ntl-meta-view-client.h			\
	ntl-content-view-client.h		\
	nautilus.h				\
	gnome-icon-container-dnd.h		\
	gnome-icon-container-layout.h		\
	gnome-icon-container-private.h		\
	gnome-icon-container.h			\
	gnome-progressive-loader.c		\
	gnome-progressive-loader.h		\
	gtkflist.h				\
	gtkscrollframe.h

libnautilus_la_SOURCES=$(nautilus_idl_sources)	\
	ntl-view-client.c			\
	ntl-meta-view-client.c			\
	ntl-content-view-client.c			\
	gnome-icon-container-dnd.c		\
	gnome-icon-container-layout.c		\
	gnome-icon-container.c			\
	gtkflist.c				\
	gtkscrollframe.c

libnautilus_la_LDFLAGS=$(GNOME_LIBS) $(GNORBA_LIBS) $(GDK_PIXBUF_LIBS)

$(nautilus_idl_sources): nautilus_idl_stamp

nautilus_idl_stamp: $(top_srcdir)/idl/nautilus.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/nautilus.idl
	touch nautilus_idl_stamp

CLEANFILES=nautilus_idl_stamp

