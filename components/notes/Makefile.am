NULL =

bin_PROGRAMS=nautilus-notes

INCLUDES=\
	-DG_LOG_DOMAIN=\"Nautilus-Notes\" \
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/libnautilus	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(EEL_CFLAGS) \
	$(LIBRSVG_CFLAGS) \
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

nautilus_notes_SOURCES=nautilus-notes.c


oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_notes.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST= \
	$(oaf_DATA)\
	$(oaf_in_files) \
	$(NULL)
