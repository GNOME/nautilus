NULL =

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Adapter\"	\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir) \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files =						\
	Nautilus_ComponentAdapterFactory_std.oaf.in	\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

bin_PROGRAMS =					\
	nautilus-adapter			\
	$(NULL)

nautilus_adapter_SOURCES =				\
	main.c						\
	bonobo-stream-vfs.c				\
	bonobo-stream-vfs.h				\
	nautilus-adapter-control-embed-strategy.c	\
	nautilus-adapter-control-embed-strategy.h	\
	nautilus-adapter-embed-strategy-private.h	\
	nautilus-adapter-embed-strategy.c		\
	nautilus-adapter-embed-strategy.h		\
	nautilus-adapter-embeddable-embed-strategy.c	\
	nautilus-adapter-embeddable-embed-strategy.h	\
	nautilus-adapter-factory-server.c		\
	nautilus-adapter-factory-server.h		\
	nautilus-adapter-file-load-strategy.c		\
	nautilus-adapter-file-load-strategy.h		\
	nautilus-adapter-load-strategy.c		\
	nautilus-adapter-load-strategy.h		\
	nautilus-adapter-load-strategy.h		\
	nautilus-adapter-progressive-load-strategy.c	\
	nautilus-adapter-progressive-load-strategy.h	\
	nautilus-adapter-stream-load-strategy.c		\
	nautilus-adapter-stream-load-strategy.h		\
	nautilus-zoomable-proxy.c			\
	nautilus-zoomable-proxy.h			\
	nautilus-adapter.c				\
	nautilus-adapter.h				\
	$(NULL)

nautilus_adapter_LDADD =							\
	$(top_builddir)/libnautilus/libnautilus.la				\
	$(top_builddir)/libnautilus-adapter/libnautilus-adapter.la		\
	$(NAUTILUS_PRIVATE_LIBS)						\
	$(NULL)

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)

