include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Sample\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(BONOBO_CFLAGS)			\
	$(OAF_CFLAGS)

oafdir = $(datadir)/oaf
oaf_DATA = nautilus-sample-content-view.oafinfo

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-sample-content-view-ui.xml
ui_msgs = nautilus-sample-content-view-ui.xml.h

bin_PROGRAMS =					\
	nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =		\
	nautilus-sample-content-view.h          \
	nautilus-sample-content-view.c          \
	main.c

nautilus_sample_content_view_LDADD =		\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(GNOMEUI_LIBS)				\
	$(BONOBO_LIBS)				\
	$(GCONF_LIBS)				\
	$(VFS_LIBS)				\
	$(OAF_LIBS)

EXTRA_DIST = $(oaf_DATA) $(ui_DATA) $(ui_msgs)
BUILT_SOURCES = $(ui_msgs)
CLEANFILES = $(ui_msgs)
