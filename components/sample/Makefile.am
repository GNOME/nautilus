# WHAT YOU NEED TO CHANGE: Change all the filenames to refer to your
# component.  Also add whatever libraries, include paths, etc you want,
# and change the G_LOG_DOMAIN.

NULL =

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Sample\"		\
	-DDATADIR=\"$(datadir)\"			\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\"	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(GNOMEUI_CFLAGS)                       	\
	$(BONOBO_CFLAGS)				\
	$(OAF_CFLAGS)

bin_PROGRAMS =						\
	nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =			\
	nautilus-sample-content-view.h          	\
	nautilus-sample-content-view.c          	\
	main.c

nautilus_sample_content_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(BONOBO_LIBS)					\
	$(BONOBOX_LIBS)					\
	$(VFS_LIBS)					\
	$(OAF_LIBS)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-sample-content-view-ui.xml

oafdir = $(datadir)/oaf
oaf_in_files =						\
	Nautilus_View_sample.oaf.in			\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(ui_DATA) 
