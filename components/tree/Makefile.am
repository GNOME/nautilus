NULL =

INCLUDES =						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"Nautilus-Tree\"		\
	-I$(top_srcdir)					\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

TREE_VIEW_COMMON_SOURCES =			\
	nautilus-tree-change-queue.c		\
	nautilus-tree-change-queue.h		\
	nautilus-tree-expansion-state.c		\
	nautilus-tree-expansion-state.h		\
	nautilus-tree-model.c			\
	nautilus-tree-model.h			\
	nautilus-tree-node-private.h		\
	nautilus-tree-node.c			\
	nautilus-tree-node.h			\
	nautilus-tree-view-iids.h		\
	nautilus-tree-view.c			\
	nautilus-tree-view.h			\
	nautilus-tree-view-private.h		\
	nautilus-tree-view-dnd.c		\
	nautilus-tree-view-dnd.h		\
	$(NULL)

lib_LTLIBRARIES =			\
	libnautilus-tree-view.la

libnautilus_tree_view_la_SOURCES =         \
	$(TREE_VIEW_COMMON_SOURCES)        \
	$(COMPONENT_LIBS)		   \
	libmain.c                          \
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_tree.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA)

