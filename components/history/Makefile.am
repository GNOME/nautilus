NULL =

bin_PROGRAMS=nautilus-history-view

INCLUDES=\
	-I$(top_srcdir) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(COMPONENT_CFLAGS)
	$(NULL)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(COMPONENT_LIBS)
	$(NULL)

nautilus_history_view_SOURCES=nautilus-history-view.c

serverdir = $(libdir)/bonobo/servers

server_in_files =					\
	Nautilus_View_history.server.in		\
	$(NULL)

server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_DATA) $(server_in_files)

