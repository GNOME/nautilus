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

TREE_VIEW_COMMON_SOURCES = 	        \
	nautilus-tree-expansion-state.c \
	nautilus-tree-expansion-state.h \
	nautilus-tree-model.c           \
	nautilus-tree-model.h           \
	nautilus-tree-node-private.h    \
	nautilus-tree-node.c            \
	nautilus-tree-node.h            \
	nautilus-tree-view-iids.h       \
	nautilus-tree-view.c            \
	nautilus-tree-view.h            \
	$(NULL)


if TREE_VIEW_EXE_SERVER

OAFINFO_FILE = nautilus-tree-view.oafinfo.exe

bin_PROGRAMS =				\
	nautilus-tree-view

nautilus_tree_view_SOURCES =            \
	$(TREE_VIEW_COMMON_SOURCES)     \
	main.c                          \
	$(NULL)

else

OAFINFO_FILE = nautilus-tree-view.oafinfo.shlib

lib_LTLIBRARIES =				\
	libnautilus-tree-view.la

libnautilus_tree_view_la_SOURCES =         \
	$(TREE_VIEW_COMMON_SOURCES)        \
	libmain.c                          \
	$(NULL)

endif




nautilus_tree_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)                                      \
	$(NULL)

EXTRA_DIST = nautilus-tree-view.oafinfo.shlib nautilus-tree-view.oafinfo.exe

install-data-hook: $(oaf_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(oafdir)
	$(INSTALL_DATA) $(srcdir)/$(OAFINFO_FILE) $(DESTDIR)$(oafdir)/nautilus-tree-view.oafinfo

uninstall-local:
	@$(NORMAL_UNINSTALL)
	rm -f $(DESTDIR)$(oafdir)/nautilus-tree-view.oafinfo; 



