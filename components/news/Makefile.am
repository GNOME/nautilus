NULL =

bin_PROGRAMS=nautilus-news

INCLUDES=\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/libnautilus	\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" \
	-DDATADIR=\""$(datadir)"\" 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOMEUI_CFLAGS)	\
	$(GCONF_CFLAGS)		\
	$(BONOBO_CFLAGS)	\
	$(VFS_CFLAGS)
	
LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

nautilus_news_SOURCES=nautilus-news.c

nautilusdir = $(datadir)/nautilus
nautilus_DATA = news_channels.xml

nautiluspixmapdir = $(datadir)/pixmaps/nautilus
nautiluspixmap_DATA = news_bullet.png

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
