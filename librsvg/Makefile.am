NULL=

bin_PROGRAMS=test-rsvg test-ft

test_rsvg_LDADD =\
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	librsvg.la

test_ft_LDADD =\
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	librsvg.la

lib_LTLIBRARIES=librsvg.la

INCLUDES = \
	-I$(top_srcdir)/cut-n-paste-code \
	$(GNOME_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(GDK_PIXBUF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT \
	$(NULL)

librsvg_la_LDFLAGS=\
	$(top_builddir)/cut-n-paste-code/freetype/libfreetype2.la
	$(GNOME_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG) \
	$(NULL)

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
	rsvg-ft.c		\
	rsvg-ft.h		\
	rsvg-paint-server.c 	\
	rsvg-paint-server.h 	\
	rsvg-path.c 		\
	rsvg-path.h 		\
	art_render.c		\
	art_render.h		\
	art_render_gradient.c	\
	art_render_gradient.h	\
	art_render_mask.c	\
	art_render_mask.h	\
	art_render_svp.c	\
	art_render_svp.h	\
	$(NULL)

test_rsvg_LIBS =\
	$(GDK_PIXBUF_LIBS)

test_rsvg_SOURCES =\
	test-rsvg.c	\
	$(NULL)
