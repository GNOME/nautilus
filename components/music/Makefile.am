INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Music\"	\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GCONF_CFLAGS) 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)                       \
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(VFS_CFLAGS)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-music-view.oafinfo


bin_PROGRAMS =					\
	nautilus-music-view

nautilus_music_view_SOURCES =		        \
	mp3head.c				\
	mp3head.h				\
	mpg123_handler.c			\
	mpg123_handler.h			\
	nautilus-music-view.c			\
	nautilus-music-view.h			\
	main.c					\
	pixmaps.h

nautilus_music_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

EXTRA_DIST = $(oaf_DATA)

install-exec-hook:
	MPG123_PATH=`which mpg123`; \
	if test -x /usr/bin/mpg123; then \
	ln -sf /usr/bin/mpg123 $(DESTDIR)/$(bindir)/nautilus-mpg123; \
	elif test "x$${MPG123_PATH}" != "x"; then \
	ln -sf "$${MPG123_PATH}" $(DESTDIR)/$(bindir)/nautilus-mpg123; \
	fi

