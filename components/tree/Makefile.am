NULL =

INCLUDES =						\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-DG_LOG_DOMAIN=\"Nautilus-Tree\"		\
	-I$(top_srcdir)					\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

TREE_VIEW_COMMON_SOURCES =			\
	nautilus-tree-model.c			\
	nautilus-tree-model.h			\
	nautilus-tree-view.c			\
	nautilus-tree-view.h			\
	nautilus-tree-view-iids.h		\
	$(NULL)

lib_LTLIBRARIES =				\
	libnautilus-tree-view.la

libnautilus_tree_view_la_SOURCES = $(TREE_VIEW_COMMON_SOURCES) libmain.c 

libnautilus_tree_view_la_LDFLAGS = -module -avoid-version

libnautilus_tree_view_la_LIBADD =               \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(COMPONENT_LIBS)
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_tree.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA)
