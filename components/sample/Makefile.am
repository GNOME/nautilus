# WHAT YOU NEED TO CHANGE: Change all the filenames to refer to your
# component.  Also add whatever libraries, include paths, etc you want,
# and change the G_LOG_DOMAIN.

INCLUDES =						\
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DGNOME_DISABLE_DEPRECATED			\
	-DG_LOG_DOMAIN=\"Nautilus-Sample\"		\
	-DDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS)

libexec_PROGRAMS=nautilus-sample-content-view

nautilus_sample_content_view_SOURCES =			\
	nautilus-sample-content-view.h          	\
	nautilus-sample-content-view.c          	\
	main.c

nautilus_sample_content_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(COMPONENT_LIBS)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = nautilus-sample-content-view-ui.xml

server_in_files = Nautilus_View_sample.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(ui_DATA) 
CLEANFILES = $(server_DATA) $(server_DATA).in
