include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(GCONF_CFLAGS) 			\
	$(GNOMEUI_CFLAGS)			\
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-content-loser-ui.xml

bin_PROGRAMS =					\
	nautilus-content-loser

nautilus_content_loser_SOURCES =		\
	nautilus-content-loser.h          \
	nautilus-content-loser.c          \
	main.c

nautilus_content_loser_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_content-loser.oaf.in	\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files) $(ui_DATA)
