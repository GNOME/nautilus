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

nautilus_view_component_idl_sources=nautilus-view-component-stubs.c nautilus-view-component-skels.c nautilus-view-component.h nautilus-view-component-common.c
BUILT_SOURCES=$(nautilus_view_component_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus

libnautilusinclude_HEADERS=			\
	libnautilus.h				\
	nautilus-view-component.h		\
	nautilus-content-view-frame.h		\
	nautilus-meta-view-frame.h			\
	nautilus-view-frame.h			\
	$(NULL)

libnautilus_la_SOURCES=$(nautilus_view_component_idl_sources)	\
	nautilus-zoomable.c			\
	nautilus-content-view-frame.c		\
	nautilus-meta-view-frame.c			\
	nautilus-view-frame.c			\
	$(NULL)

$(nautilus_view_component_idl_sources): nautilus_view_component_idl_stamp

nautilus_view_component_idl_stamp: nautilus-view-component.idl
	orbit-idl `gnome-config --cflags idl` nautilus-view-component.idl
	touch nautilus_view_component_idl_stamp

CLEANFILES=nautilus_view_component_idl_stamp

EXTRA_DIST = nautilus-view-component.idl
