bin_PROGRAMS=ntl-web-search

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(BONOBO_CFLAGS) $(WERROR)
LDADD=$(top_builddir)/libnautilus/libnautilus.la $(BONOBO_LIBS) $(GNOMEUI_LIBS)

ntl_web_search_SOURCES=ntl-web-search.c

goaddir=$(sysconfdir)/CORBA/servers
goad_DATA=ntl-web-search.goad

enginesdir=$(datadir)/ntl-web-search
engines_DATA=standard.eng

EXTRA_DIST=$(goad_DATA) $(engines_DATA)
