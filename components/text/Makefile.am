NULL = 

SUBDIRS = services

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Text\"	\
	-DDATADIR=\""$(datadir)"\" 		\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_builddir)/libnautilus		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(EEL_CFLAGS) \
	$(LIBRSVG_CFLAGS) \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS) 			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(VFS_CFLAGS)

bin_PROGRAMS =					\
	nautilus-text-view

nautilus_text_view_SOURCES =		        \
	nautilus-text-view.c			\
	nautilus-text-view.h			\
	main.c

nautilus_text_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) 					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-text-view-ui.xml

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_text.oaf.in		\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(ui_DATA) 


