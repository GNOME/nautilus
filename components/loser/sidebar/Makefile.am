include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-sidebar-loser-ui.xml

bin_PROGRAMS =					\
	nautilus-sidebar-loser

nautilus_sidebar_loser_SOURCES =		\
	nautilus-sidebar-loser.h          \
	nautilus-sidebar-loser.c          \
	main.c

nautilus_sidebar_loser_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_sidebar-loser.oaf.in	\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files) $(ui_DATA)
