NULL =

bin_PROGRAMS=nautilus-news

INCLUDES=\
	-I$(top_srcdir)	\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(COMPONENT_CFLAGS) \
	$(NULL)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(COMPONENT_LIBS) \
	$(NULL)

nautilus_news_SOURCES= \
	nautilus-cell-renderer-news.c \
	nautilus-cell-renderer-news.h \
	nautilus-news.c

nautilusdir = $(datadir)/nautilus
nautilus_DATA = news_channels.xml

nautiluspixmapdir = $(datadir)/pixmaps/nautilus
nautiluspixmap_DATA = news_bullet.png		\
		      changed_bullet.png

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_news.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

EXTRA_DIST= \
	$(nautilus_DATA) \
	$(nautiluspixmap_DATA) \
	$(server_DATA)\
	$(server_in_files) \
	$(NULL)
