NULL =

INCLUDES=\
	-DG_LOG_DOMAIN=\"Nautilus-Notes\" \
	-I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(COMPONENT_CFLAGS) \
	$(NULL)	\
	-DGTK_ENABLE_BROKEN

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libnautilus-notes-view.la

libnautilus_notes_view_la_SOURCES = nautilus-notes.c

libnautilus_notes_view_la_LIBADD =               \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(COMPONENT_LIBS) \
	$(NULL)

libnautilus_notes_view_la_LDFLAGS = -module -avoid-version -no-undefined

server_in_files = Nautilus_View_notes.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST= $(server_in_files)
CLEANFILES = $(server_DATA) $(server_DATA).in
