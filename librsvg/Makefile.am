NULL=

bin_PROGRAMS=test-rsvg

test_rsvg_LDADD =\
	librsvg.la

lib_LTLIBRARIES=librsvg.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
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
	rsvg-path.c 		\
	rsvg-bpath-util.c 	\
	art_rgba.c 		\
	art_rgba_svp.c		\
	$(NULL)

test_rsvg_SOURCES =\
	test-rsvg.c	\
	$(NULL)