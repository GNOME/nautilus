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
	$(COMPONENT_CFLAGS)

bin_PROGRAMS =						\
	nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =			\
	nautilus-sample-content-view.h          	\
	nautilus-sample-content-view.c          	\
	main.c

nautilus_sample_content_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(COMPONENT_LIBS)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = nautilus-sample-content-view-ui.xml

serverdir = $(libdir)/bonobo/servers
server_in_files =					\
	Nautilus_View_sample.server.in			\
	$(NULL)
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA) $(ui_DATA) 
