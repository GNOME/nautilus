CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Music\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(WERROR)

gnorbadir = $(sysconfdir)/CORBA/servers

gnorba_DATA =					\
	nautilus-music-view.goad


bin_PROGRAMS =					\
	nautilus-music-view

nautilus_music_view_SOURCES =		        \
	nautilus-music-view.c			\
	nautilus-music-view.h			\
	main.c

nautilus_music_view_LDFLAGS =			\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

EXTRA_DIST = nautilus-music-view.goad
