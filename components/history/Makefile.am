NULL =

INCLUDES=\
	-I$(top_srcdir) \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(COMPONENT_CFLAGS) \
	$(NULL)

lib_LTLIBRARIES = libnautilus-history-view.la

libnautilus_history_view_la_SOURCES = \
	libmain.c		\
	nautilus-history-view.c	\
	nautilus-history-view.h	\
	$(NULL)

libnautilus_history_view_la_LDFLAGS = -module -avoid-version

libnautilus_history_view_la_LIBADD =               \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(COMPONENT_LIBS)
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_history.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_DATA) $(server_in_files)
