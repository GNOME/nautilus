SUBDIRS=file-manager

bin_PROGRAMS=nautilus

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(BONOBO_CFLAGS) $(GNORBA_CFLAGS) \
	$(GNOMEUI_CFLAGS) $(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	$(WERROR)

LDADD=	file-manager/libntl-file-manager.la \
	../libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNORBA_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS)

nautilus_SOURCES=			\
	explorer-location-bar.c         \
	explorer-location-bar.h		\
	nautilus-bookmark.c		\
	nautilus-bookmark.h		\
	nautilus-bookmarklist.c		\
	nautilus-bookmarklist.h		\
	nautilus-bookmarks-menu.c	\
	nautilus-bookmarks-menu.h	\
	nautilus-bookmarks-window.c	\
	nautilus-bookmarks-window.h	\
        nautilus-self-check-functions.c \
        nautilus-self-check-functions.h \
	ntl-app.c			\
	ntl-app.h			\
	ntl-content-view.c		\
	ntl-index-panel.h		\
	ntl-index-panel.c		\
	ntl-content-view.h		\
	ntl-main.c 			\
	ntl-meta-view.c			\
	ntl-meta-view.h			\
	ntl-miniicon.c			\
	ntl-miniicon.h			\
	ntl-prefs.c			\
	ntl-prefs.h			\
	ntl-types.h			\
	ntl-uri-map.c			\
	ntl-uri-map.h			\
	ntl-view-bonobo-control.c	\
	ntl-view-bonobo-subdoc.c	\
	ntl-view.c			\
	ntl-view-frame-svr.c		\
	ntl-view.h			\
	ntl-view-nautilus.c		\
	ntl-view-private.h		\
	ntl-window.c			\
	ntl-window.h			\
	ntl-window-msgs.c		\
	ntl-window-msgs.h		\
	ntl-window-private.h		\
	ntl-window-state.c		\
	ntl-window-state.h

TESTS=check-nautilus

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA =					\
	nautilus.goad

EXTRA_DIST = nautilus.goad
