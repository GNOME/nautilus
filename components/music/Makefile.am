CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Music\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-music-view.oafinfo


bin_PROGRAMS =					\
	nautilus-music-view

nautilus_music_view_SOURCES =		        \
	nautilus-music-view.c			\
	nautilus-music-view.h			\
	main.c

nautilus_music_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/librsvg/librsvg.la		\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

EXTRA_DIST = $(oaf_DATA)
