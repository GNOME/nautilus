bin_PROGRAMS=ntl-history-view

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(BONOBO_CFLAGS) $(WERROR)
LDADD=$(top_builddir)/libnautilus/libnautilus.la $(BONOBO_LIBS) $(GNOMEUI_LIBS)

ntl_history_view_SOURCES=ntl-history-view.c

goaddir=$(sysconfdir)/CORBA/servers
goad_DATA=ntl-history-view.goad
EXTRA_DIST=$(goad_DATA)
