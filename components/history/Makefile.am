NULL =

INCLUDES=\
	-I$(top_srcdir) \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(COMPONENT_CFLAGS) \
	$(NULL)

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libnautilus-history-view.la

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

server_in_files = Nautilus_View_history.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files)
