NULL =

if X86_OPTIMIZED
optimized_source = dct64_i386.c decode_i386.c
else
optimized_source = dct64.c decode.c
endif

platform_source = decode.c dct64.c decode_i386.c dct64_i386.c

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Music\"	\
	-I$(top_srcdir)				\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)		\
	$(MUSIC_COMPONENT_CFLAGS)		\
	$(NULL)

bin_PROGRAMS =nautilus-music-view

nautilus_music_view_SOURCES =			\
	common.c				\
	decode_2to1.c				\
	decode_4to1.c				\
	decode_ntom.c				\
	dxhead.c				\
	dxhead.h				\
	esd-audio.c				\
	esd-audio.h				\
	esd-mixer.c				\
	getbits.h				\
	huffman.h				\
	id3.c					\
	id3.h					\
	id3_frame.c				\
	id3_frame_content.c			\
	id3_frame_text.c			\
	id3_frame_url.c				\
	id3_header.h				\
	id3_tag.c				\
	l2tables.h				\
	layer1.c				\
	layer2.c				\
	layer3.c				\
	main.c					\
	mp3head.c				\
	mp3head.h				\
	mpg123.c				\
	mpg123.h				\
	nautilus-music-view.c			\
	nautilus-music-view.h			\
	pixmaps.h				\
	tabinit.c				\
	$(optimized_source)

nautilus_music_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(MUSIC_COMPONENT_LIBS)			\
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_music.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_DATA) $(server_in_files) $(platform_source) \
	configure.c				\
	fileinfo.c				\
	getbits.c				\
	$(NULL)
