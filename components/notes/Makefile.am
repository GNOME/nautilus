NULL =

bin_PROGRAMS=nautilus-notes

INCLUDES=\
	-DG_LOG_DOMAIN=\"Nautilus-Notes\" \
	-I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

nautilus_notes_SOURCES=nautilus-notes.c

serverdir = $(datadir)/server
server_in_files =					\
	Nautilus_View_notes.server.in		\
	$(NULL)
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST= \
	$(server_DATA)\
	$(server_in_files) \
	$(NULL)
