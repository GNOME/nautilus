NULL =

INCLUDES=\
	-DG_LOG_DOMAIN=\"Nautilus-Notes\" \
	-I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(COMPONENT_CFLAGS) \
	$(NULL)	\
	-DGTK_ENABLE_BROKEN

lib_LTLIBRARIES = libnautilus-notes-view.la

libnautilus_notes_view_la_SOURCES = nautilus-notes.c

libnautilus_notes_view_la_LIBADD =               \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(COMPONENT_LIBS) \
	$(NULL)

libnautilus_notes_view_la_LDFLAGS = -module -avoid-version


serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_notes.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

EXTRA_DIST= \
	$(server_DATA)\
	$(server_in_files) \
	$(NULL)
