bin_PROGRAMS=ntl-history-view

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(BONOBO_CFLAGS) $(WERROR)

LDADD=\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/librsvg/librsvg.la \
	$(BONOBO_LIBS) \
	$(GNOMEUI_LIBS)

ntl_history_view_SOURCES=ntl-history-view.c

oafdir=$(datadir)/oaf
oaf_DATA=ntl-history-view.oafinfo
EXTRA_DIST=$(oaf_DATA)
