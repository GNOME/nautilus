NULL=

lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) -I. \
	$(GNOME_CFLAGS) \
	$(BONOBOX_CFLAGS) \
	$(OAF_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-D_REENTRANT

libnautilus_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(OAF_LIBS) \
	$(BONOBOX_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG)

nautilus_view_component_idl_sources =		\
	nautilus-view-component-stubs.c		\
	nautilus-view-component-skels.c		\
	nautilus-view-component.h		\
	nautilus-view-component-common.c

nautilus_distributed_undo_idl_sources =	\
	nautilus-distributed-undo-stubs.c	\
	nautilus-distributed-undo-skels.c	\
	nautilus-distributed-undo.h		\
	nautilus-distributed-undo-common.c

BUILT_SOURCES = \
	$(nautilus_view_component_idl_sources)	\
	$(nautilus_distributed_undo_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus

libnautilusinclude_HEADERS=			\
	libnautilus.h				\
	nautilus-bonobo-ui.h			\
	nautilus-clipboard.h			\
	nautilus-distributed-undo.h		\
	nautilus-view-component.h		\
	nautilus-view.h				\
	nautilus-undo.h				\
	nautilus-undo-private.h			\
	nautilus-zoomable.h			\
	$(NULL)

libnautilus_la_SOURCES=					\
	$(nautilus_view_component_idl_sources)		\
	$(nautilus_distributed_undo_idl_sources)	\
	nautilus-clipboard.c				\
	nautilus-undo-transaction.c			\
	nautilus-undo-transaction.h			\
	nautilus-undo.c					\
	nautilus-view.c					\
	nautilus-zoomable.c				\
	$(NULL)

$(nautilus_view_component_idl_sources): nautilus_view_component_idl_stamp
nautilus_view_component_idl_stamp: nautilus-view-component.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) $(srcdir)/nautilus-view-component.idl
	touch nautilus_view_component_idl_stamp

$(nautilus_distributed_undo_idl_sources): nautilus_distributed_undo_idl_stamp
nautilus_distributed_undo_idl_stamp: nautilus-distributed-undo.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) $(srcdir)/nautilus-distributed-undo.idl
	touch nautilus_distributed_undo_idl_stamp

CLEANFILES = \
	$(nautilus_view_component_idl_sources)		\
	nautilus_view_component_idl_stamp		\
	$(nautilus_distributed_undo_idl_sources)	\
	nautilus_distributed_undo_idl_stamp

EXTRA_DIST = \
	nautilus-view-component.idl	\
	nautilus-distributed-undo.idl	\
	$(NULL)

