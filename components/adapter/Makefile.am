CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Adapter\"	\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\"

INCLUDES=-I$(top_srcdir) -I$(top_builddir) -I. \
	$(GNOMEUI_CFLAGS) \
	$(BONOBOX_CFLAGS) \
	$(OAF_CFLAGS) \
	$(XML_CFLAGS) \
	-D_REENTRANT

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-adapter.oafinfo

bin_PROGRAMS =					\
	nautilus-adapter

nautilus_adapter_SOURCES =		\
	main.c	                    \
	nautilus-adapter-factory-server.h          \
	nautilus-adapter-factory-server.c	   \
	nautilus-adapter.h   		           \
	nautilus-adapter.c

nautilus_adapter_LDADD =		\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-adapter/libnautilus-adapter.la	 \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(GNOMEUI_LIBS) \
	$(OAF_LIBS) \
	$(BONOBO_PRINT_LIBS) 		\
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(BONOBOX_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS)	\
	$(GHTTP_LIBS) 			\
	$(GCONF_LIBS) 			\
	$(LIBPNG)


EXTRA_DIST = $(oaf_DATA)
