NULL=

bin_PROGRAMS=test-rsvg

test_rsvg_LDADD =\
	librsvg.la

lib_LTLIBRARIES=librsvg.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(GDK_PIXBUF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT

librsvg_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG)

librsvgincludedir=$(includedir)/librsvg
librsvginclude_HEADERS=			\
	rsvg.h				\
	$(NULL)

librsvg_la_SOURCES=	\
	rsvg.c	 		\
	rsvg.h	 		\
	rsvg-bpath-util.c 	\
	rsvg-bpath-util.h	\
	rsvg-css.c		\
	rsvg-css.h		\
	rsvg-defs.c		\
	rsvg-defs.h		\
	rsvg-paint-server.c 	\
	rsvg-paint-server.h 	\
	rsvg-path.c 		\
	rsvg-path.h 		\
	art_render.c		\
	art_render.h		\
	art_render_gradient.c	\
	art_render_gradient.h	\
	art_render_svp.c	\
	art_render_svp.h	\
	$(NULL)

test_rsvg_SOURCES =\
	test-rsvg.c	\
	$(NULL)