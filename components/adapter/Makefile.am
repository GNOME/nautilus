NULL =

CPPFLAGS =						\
	-DPREFIX=\"$(prefix)\"				\
	-DG_LOG_DOMAIN=\"Nautilus-Adapter\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\"	\
	$(NULL)

INCLUDES =					\
	-I$(top_srcdir) -I$(top_builddir) -I.	\
	$(GNOMEUI_CFLAGS)			\
	$(BONOBOX_CFLAGS)			\
	$(OAF_CFLAGS)				\
	$(XML_CFLAGS)				\
	-D_REENTRANT				\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-adapter.oafinfo		\
	$(NULL)

bin_PROGRAMS =					\
	nautilus-adapter			\
	$(NULL)

nautilus_adapter_SOURCES =				\
	main.c						\
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
	nautilus-adapter.c				\
	nautilus-adapter.h				\
	$(NULL)

nautilus_adapter_LDADD =							\
	$(top_builddir)/libnautilus/libnautilus.la				\
	$(top_builddir)/libnautilus-adapter/libnautilus-adapter.la		\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(GNOMEUI_LIBS)								\
	$(OAF_LIBS)								\
	$(BONOBO_PRINT_LIBS)							\
	$(VFS_LIBS)								\
	$(XML_LIBS)								\
	$(BONOBOX_LIBS)								\
	$(GNOMECANVASPIXBUF_LIBS)						\
	$(GHTTP_LIBS)								\
	$(GCONF_LIBS)								\
	$(LIBPNG)								\
	$(NULL)


EXTRA_DIST = $(oaf_DATA)
