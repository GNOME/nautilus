bin_PROGRAMS=hyperbola

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(BONOBO_CFLAGS)
LDADD=$(top_builddir)/libnautilus/libnautilus.la $(BONOBO_LIBS) $(GNOMEUI_LIBS)

hyperbola_SOURCES=		\
	hyperbola-nav-tree.c 	\
	hyperbola-main.c	\
	hyperbola-filefmt.c	\
	hyperbola-filefmt.h

EXTRA_DIST=pages.map

mapdatadir=$(datadir)/hyperbola/maps
mapdata_DATA=pages.map
