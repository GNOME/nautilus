NULL =

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"                  \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"Nautilus-Tree\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(VFS_CFLAGS) 				\
	$(GCONF_CFLAGS) 			\
	$(GNOMEUI_CFLAGS)			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-tree-view.oafinfo


bin_PROGRAMS =					\
	nautilus-tree-view

nautilus_tree_view_SOURCES =	       \
	nautilus-tree-view.h           \
	nautilus-tree-view.c           \
	nautilus-tree-model.h          \
	nautilus-tree-model.c          \
	nautilus-tree-node-private.h   \
	nautilus-tree-node.h           \
	nautilus-tree-node.c           \
	main.c                         \
	$(NULL)

nautilus_tree_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)                                      \
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
