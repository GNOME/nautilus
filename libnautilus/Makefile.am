NULL=

lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) -I. \
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

nautilus_view_component_idl_sources =		\
	nautilus-view-component-stubs.c		\
	nautilus-view-component-skels.c		\
	nautilus-view-component.h		\
	nautilus-view-component-common.c

nautilus_undo_idl_sources =	\
	nautilus-undo-stubs.c	\
	nautilus-undo-skels.c	\
	nautilus-undo.h		\
	nautilus-undo-common.c

BUILT_SOURCES = \
	$(nautilus_view_component_idl_sources)	\
	$(nautilus_undo_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus

libnautilusinclude_HEADERS=			\
	libnautilus.h				\
	nautilus-bonobo-ui.h			\
	nautilus-clipboard.h			\
	nautilus-content-view-frame.h		\
	nautilus-meta-view-frame.h		\
	nautilus-view-component.h		\
	nautilus-view-frame.h			\
	nautilus-undo-context.h			\
	nautilus-undo-manager.h			\
	nautilus-undo-transaction.h		\
	nautilus-undoable.h			\
	nautilus-zoomable.h			\
	$(NULL)

libnautilus_la_SOURCES=					\
	nautilus-undo-manager-private.h			\
	nautilus-view-frame-private.h			\
	$(nautilus_view_component_idl_sources)		\
	$(nautilus_undo_idl_sources)			\
	nautilus-clipboard.c				\
	nautilus-content-view-frame.c			\
	nautilus-meta-view-frame.c			\
	nautilus-view-frame.c				\
	nautilus-undo-context.c				\
	nautilus-undo-manager.c				\
	nautilus-undo-transaction.c			\
	nautilus-undoable.c				\
	nautilus-zoomable.c				\
	$(NULL)

$(nautilus_view_component_idl_sources): nautilus_view_component_idl_stamp
nautilus_view_component_idl_stamp: nautilus-view-component.idl
	$(prefix)/bin/orbit-idl -I${datadir}/idl $(srcdir)/nautilus-view-component.idl
	touch nautilus_view_component_idl_stamp

$(nautilus_undo_idl_sources): nautilus_undo_idl_stamp
nautilus_undo_idl_stamp: nautilus-undo.idl
	$(prefix)/bin/orbit-idl -I${datadir}/idl $(srcdir)/nautilus-undo.idl
	touch nautilus_undo_idl_stamp

CLEANFILES = \
	$(nautilus_view_component_idl_sources)	\
	nautilus_view_component_idl_stamp	\
	$(nautilus_undo_idl_sources)		\
	nautilus_undo_idl_stamp

EXTRA_DIST = \
	nautilus-view-component.idl	\
	nautilus-undo.idl
