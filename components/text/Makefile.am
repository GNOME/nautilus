NULL = 

SUBDIRS = services

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Text\"	\
	-DDATADIR=\""$(datadir)"\" 		\
	-I$(top_srcdir)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

bin_PROGRAMS =					\
	nautilus-text-view

nautilus_text_view_SOURCES =		        \
	nautilus-text-view.c			\
	nautilus-text-view.h			\
	main.c

nautilus_text_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-text-view-ui.xml

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_text.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(ui_DATA) 


