bin_PROGRAMS=nautilus

INCLUDES=$(GNORBA_CFLAGS) $(GNOMEUI_CFLAGS) -DVERSION="\"$(VERSION)\""
LDADD=$(GNORBA_LIBS) $(GNOMEUI_LIBS) $(BONOBO_LIBS)

nautilus_SOURCES=		\
	ntl-content-view.h	\
	ntl-content-view.c	\
	ntl-main.c 		\
	ntl-meta-view.h		\
	ntl-meta-view.c		\
	ntl-types.h		\
	ntl-uri-map.h		\
	ntl-uri-map.c		\
	ntl-view.h		\
	ntl-view.c		\
	ntl-window.h		\
	ntl-window.c
