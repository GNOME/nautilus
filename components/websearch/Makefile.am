bin_PROGRAMS=ntl-web-search

INCLUDES=\
	-I$(top_srcdir) 			\
	-I$(top_builddir)			\
	-DWEB_SEARCH_DATADIR=\""$(datadir)/websearch"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)			\
	$(GCONF_CFLAGS)				\
	$(BONOBO_CFLAGS)

LDADD=\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

ntl_web_search_SOURCES=\
	ntl-web-search.c

oafdir=$(datadir)/oaf
oaf_DATA=ntl-web-search.oafinfo

enginesdir=$(datadir)/websearch
engines_DATA=standard.eng

EXTRA_DIST=$(oaf_DATA) $(engines_DATA) $(ntl_web_search_SOURCES)
