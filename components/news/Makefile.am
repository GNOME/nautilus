NULL =

bin_PROGRAMS=nautilus-news

INCLUDES=\
	-I$(top_srcdir)		\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" \
	-DDATADIR=\""$(datadir)"\" 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

nautilus_news_SOURCES=nautilus-news.c nautilus-news-pixmaps.h

nautilusdir = $(datadir)/nautilus
nautilus_DATA = news_channels.xml

nautiluspixmapdir = $(datadir)/pixmaps/nautilus
nautiluspixmap_DATA = news_bullet.png		\
		      changed_bullet.png

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_news.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST= \
	$(nautilus_DATA) \
	$(nautiluspixmap_DATA) \
	$(oaf_DATA)\
	$(oaf_in_files) \
	$(NULL)
