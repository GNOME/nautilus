bin_PROGRAMS=nautilus-history-view

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(GCONF_CFLAGS) $(BONOBO_CFLAGS) $(WERROR)

LDADD=\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/librsvg/librsvg.la \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

nautilus_history_view_SOURCES=nautilus-history-view.c

oafdir=$(datadir)/oaf
oaf_DATA=nautilus-history-view.oafinfo
EXTRA_DIST=$(oaf_DATA)
