CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"                  \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"Nautilus-Tree\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(VFS_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-tree-view.oafinfo


bin_PROGRAMS =					\
	nautilus-tree-view

nautilus_tree_view_SOURCES =		\
	nautilus-tree-view.h          \
	nautilus-tree-view.c          \
	main.c

nautilus_tree_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la \
	$(top_builddir)/librsvg/librsvg.la		\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)

EXTRA_DIST = $(oaf_DATA)
