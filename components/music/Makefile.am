NULL =

if X86_OPTIMIZED
decode_source = decode_i386.c
dct64_source  = dct64_i386.c
else
decode_source = decode.c
dct64_source = dct64.c
endif

platform_source = decode.c dct64.c decode_i386.c dct64_i386.c

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Music\"	\
	-I$(top_srcdir)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

bin_PROGRAMS =					\
	nautilus-music-view

nautilus_music_view_SOURCES =			\
	esd-audio.h				\
	getbits.h				\
	huffman.h				\
	mp3head.c				\
	mp3head.h				\
	nautilus-music-view.c			\
	nautilus-music-view.h			\
	main.c					\
	tabinit.c				\
	decode_2to1.c				\
	decode_4to1.c				\
	decode_ntom.c				\
	dxhead.c				\
	dxhead.h				\
	mpg123.c				\
	mpg123.h				\
	common.c				\
	layer1.c				\
	layer2.c				\
	layer3.c				\
	l2tables.h				\
	id3.c					\
	id3.h					\
	id3_header.h				\
	id3_frame.c				\
	id3_frame_content.c			\
	id3_frame_text.c			\
	id3_frame_url.c				\
	id3_tag.c				\
	esd-audio.c				\
	esd-mixer.c				\
	pixmaps.h				\
	$(dct64_source)				\
	$(decode_source)

nautilus_music_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_music.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files) $(platform_source) \
	configure.c \
	fileinfo.c \
	getbits.c
