NULL =

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Adapter\"	\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir) \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS) \
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_ComponentAdapterFactory_std.server.in
server_DATA = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

libexec_PROGRAMS=nautilus-adapter

nautilus_adapter_SOURCES =				\
	main.c						\
	nautilus-adapter-control-embed-strategy.c	\
	nautilus-adapter-control-embed-strategy.h	\
	nautilus-adapter-embed-strategy-private.h	\
	nautilus-adapter-embed-strategy.c		\
	nautilus-adapter-embed-strategy.h		\
	nautilus-adapter-control-factory-embed-strategy.c	\
	nautilus-adapter-control-factory-embed-strategy.h	\
	nautilus-adapter-factory-server.c		\
	nautilus-adapter-factory-server.h		\
	nautilus-adapter-file-load-strategy.c		\
	nautilus-adapter-file-load-strategy.h		\
	nautilus-adapter-load-strategy.c		\
	nautilus-adapter-load-strategy.h		\
	nautilus-adapter-load-strategy.h		\
	nautilus-adapter-stream-load-strategy.c		\
	nautilus-adapter-stream-load-strategy.h		\
	nautilus-zoomable-proxy.c			\
	nautilus-zoomable-proxy.h			\
	nautilus-adapter.c				\
	nautilus-adapter.h				\
	$(NULL)

nautilus_adapter_LDADD =				\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-adapter/libnautilus-adapter.la	\
	$(COMPONENT_LIBS)				\
	$(NULL)

EXTRA_DIST = $(server_in_files).in
CLEANFILES = $(server_in_files) $(server_DATA)
