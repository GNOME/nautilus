NULL=

bin_PROGRAMS=test-rsvg test-ft

noinst_LTLIBRARIES = librsvg.la

INCLUDES = \
	-I$(top_srcdir)/cut-n-paste-code \
	$(GNOME_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(GDK_PIXBUF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT \
	$(NULL)

librsvg_la_LDFLAGS= -static

noinst_HEADERS =\
	rsvg.h \
	$(NULL)

librsvg_la_SOURCES =\
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

test_rsvg_SOURCES = test-rsvg.c
test_ft_SOURCES = test-ft.c

test_rsvg_LDADD =\
	librsvg.la \
	$(top_builddir)/cut-n-paste-code/freetype/libfreetype2.la \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	$(NULL)

test_ft_LDADD =\
	librsvg.la \
	$(top_builddir)/cut-n-paste-code/freetype/libfreetype2.la \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	$(NULL)
