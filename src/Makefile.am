SUBDIRS=file-manager

bin_PROGRAMS=nautilus

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(BONOBO_CFLAGS) $(GNORBA_CFLAGS) \
	$(GNOMEUI_CFLAGS) $(GDK_PIXBUF_CFLAGS) \
	$(VFS_CFLAGS)	\
	-DVERSION="\"$(VERSION)\""

LDADD=	file-manager/libntl-file-manager.la \
	../libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNORBA_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(VFS_LIBS)

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
	ntl-window.c		\
	ntl-window-msgs.c	\
	ntl-window-msgs.h	\
	ntl-window-private.h	\
	ntl-window-state.h	\
	ntl-window-state.c	\
	explorer-location-bar.h	\
	explorer-location-bar.c

goaddir=$(sysconfdir)/CORBA/servers
goad_DATA=nautilus.goad
EXTRA_DIST=$(goad_DATA)
