NULL =

bin_PROGRAMS=nautilus-notes

INCLUDES=\
	-DG_LOG_DOMAIN=\"Nautilus-Notes\" \
	-I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(COMPONENT_CFLAGS) \
	$(NULL)	\
	-DGTK_ENABLE_BROKEN

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(COMPONENT_LIBS) \
	$(NULL)

nautilus_notes_SOURCES=nautilus-notes.c

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_notes.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

EXTRA_DIST= \
	$(server_DATA)\
	$(server_in_files) \
	$(NULL)
