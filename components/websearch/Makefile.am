bin_PROGRAMS=ntl-web-search

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(BONOBO_CFLAGS) $(WERROR)

LDADD=\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/librsvg/librsvg.la		\
	$(BONOBO_LIBS) \
	$(GNOMEUI_LIBS)

ntl_web_search_SOURCES=ntl-web-search.c

goaddir=$(sysconfdir)/CORBA/servers
goad_DATA=ntl-web-search.goad

enginesdir=$(datadir)/ntl-web-search
engines_DATA=standard.eng

EXTRA_DIST=$(goad_DATA) $(engines_DATA)
