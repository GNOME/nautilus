NULL =

bin_PROGRAMS=nautilus-history-view

INCLUDES=\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOMEUI_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(BONOBO_CFLAGS) \
	$(VFS_CFLAGS) \
	$(NULL)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

nautilus_history_view_SOURCES=nautilus-history-view.c

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_history.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files)

