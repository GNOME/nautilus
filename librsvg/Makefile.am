NULL=

noinst_PROGRAMS =\
	test-rsvg \
	test-ft \
	test-ft-gtk \
	$(NULL)

noinst_LTLIBRARIES = librsvg.la

INCLUDES = \
	$(FREETYPE2_CFLAGS) \
	$(GNOME_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(GDK_PIXBUF_CFLAGS) \
	$(XML_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT \
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
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
	art_rgba.c		\
	art_rgba.h		\
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
test_ft_gtk_SOURCES = test-ft-gtk.c

test_rsvg_LDADD =\
	librsvg.la \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	$(FREETYPE2_LIBS) \
	$(LIBPNG) \
	$(NULL)

test_ft_LDADD =\
	librsvg.la \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	$(FREETYPE2_LIBS) \
	$(LIBPNG) \
	$(NULL)

test_ft_gtk_LDADD =\
	librsvg.la \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(XML_LIBS) \
	$(FREETYPE2_LIBS) \
	$(LIBPNG) \
	$(NULL)

