# WHAT YOU NEED TO CHANGE: Change all the filenames to refer to your
# component.  Also add whatever libraries, include paths, etc you want,
# and change the G_LOG_DOMAIN.

INCLUDES =						\
	-DG_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED			\
	-DGNOME_DISABLE_DEPRECATED			\
	-DG_LOG_DOMAIN=\"Nautilus-Image-Properties\"	\
	-DDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS)

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libnautilus-image-properties-view.la

libnautilus_image_properties_view_la_SOURCES =		\
	nautilus-image-properties-view.h          	\
	nautilus-image-properties-view.c          	\
	libmain.c

libnautilus_image_properties_view_la_LIBADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(COMPONENT_LIBS)

libnautilus_image_properties_view_la_LDFLAGS = -module -avoid-version

server_in_files = Nautilus_View_image_properties.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files)
CLEANFILES = $(server_DATA) $(server_DATA).in
