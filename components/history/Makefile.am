NULL =

bin_PROGRAMS=nautilus-history-view

INCLUDES=\
	-I$(top_srcdir) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

nautilus_history_view_SOURCES=nautilus-history-view.c

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_history.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files)

