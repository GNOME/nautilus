NULL=

lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT

libnautilus_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(OAF_LIBS) \
	$(BONOBO_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG)

nautilus_undo_manager_component_idl_sources = 	 \
	nautilus-undo-manager-component-stubs.c  \
	nautilus-undo-manager-component-skels.c  \
	nautilus-undo-manager-component-common.c \
	nautilus-undo-manager-component.h

nautilus_view_component_idl_sources =		\
	nautilus-view-component-stubs.c		\
	nautilus-view-component-skels.c		\
	nautilus-view-component.h		\
	nautilus-view-component-common.c

BUILT_SOURCES =  \
		$(nautilus_undo_manager_component_idl_sources)  \
		$(nautilus_view_component_idl_sources) 		\
		$(NULL)

libnautilusincludedir=$(includedir)/libnautilus

libnautilusinclude_HEADERS=			\
	libnautilus.h				\
	nautilus-bonobo-ui.h			\
	nautilus-clipboard.h			\
	nautilus-content-view-frame.h		\
	nautilus-meta-view-frame.h		\
	nautilus-view-component.h		\
	nautilus-view-frame.h			\
	nautilus-zoomable.h			\
	nautilus-undo-manager.h			\
	nautilus-undo-transaction.h		\
	nautilus-undoable.h			\
	$(NULL)

libnautilus_la_SOURCES=					\
	nautilus-undo-manager-private.h			\
	nautilus-view-frame-private.h			\
	$(nautilus_undo_manager_component_idl_sources) 	\
	$(nautilus_view_component_idl_sources)		\
	nautilus-clipboard.c				\
	nautilus-content-view-frame.c			\
	nautilus-meta-view-frame.c			\
	nautilus-view-frame.c				\
	nautilus-zoomable.c				\
	nautilus-undo-manager.c				\
	nautilus-undo-transaction.c			\
	nautilus-undoable.c				\
	$(NULL)

$(nautilus_undo_manager_component_idl_sources): nautilus_undo_manager_component_idl_stamp
nautilus_undo_manager_component_idl_stamp: nautilus-undo-manager-component.idl
	orbit-idl -I${datadir}/idl `gnome-config --cflags idl` $(srcdir)/nautilus-undo-manager-component.idl
	touch nautilus_undo_manager_component_idl_stamp

$(nautilus_view_component_idl_sources): nautilus_view_component_idl_stamp
nautilus_view_component_idl_stamp: nautilus-view-component.idl
	orbit-idl -I${datadir}/idl `gnome-config --cflags idl` $(srcdir)/nautilus-view-component.idl
	touch nautilus_view_component_idl_stamp

CLEANFILES=nautilus_view_component_idl_stamp nautilus_undo_manager_component_idl_stamp

EXTRA_DIST = nautilus-undo-manager-component.idl nautilus-view-component.idl
