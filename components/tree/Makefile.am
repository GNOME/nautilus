NULL =

INCLUDES =						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"Nautilus-Tree\"		\
	-I$(top_srcdir)					\
	$(NAUTILUS_PRIVATE_CFLAGS)			\
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
	libmain.c                          \
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files = Nautilus_View_tree.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)

