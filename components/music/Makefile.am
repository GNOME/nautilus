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
	esd-audio.h				\
	getbits.h				\
	huffman.h				\
	mp3head.c				\
	mp3head.h				\
	nautilus-music-view.c			\
	nautilus-music-view.h			\
	main.c					\
	tabinit.c				\
	dct64_i386.c   				\
	decode_2to1.c				\
	decode_4to1.c  				\
	decode_i386.c  				\
	decode_ntom.c  				\
	dxhead.c  				\
	dxhead.h  				\
	mpg123.c				\
	mpg123.h				\
	common.c				\
	layer1.c				\
	layer2.c				\
	layer3.c				\
	l2tables.h				\
	id3.c		   			\
	id3.h					\
	id3_header.h				\
	id3_frame.c	   			\
	id3_frame_content.c  			\
	id3_frame_text.c			\
	id3_frame_url.c	   			\
	id3_tag.c				\
	esd-audio.c				\
	esd-mixer.c				\
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
