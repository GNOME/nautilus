include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	$(COMPONENT_CFLAGS) \
	$(NULL)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = nautilus-content-loser-ui.xml

libexec_PROGRAMS=nautilus-content-loser

nautilus_content_loser_SOURCES =		\
	nautilus-content-loser.h          \
	nautilus-content-loser.c          \
	main.c

nautilus_content_loser_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(COMPONENT_LIBS) \
	$(NULL)

server_in_files = Nautilus_View_content-loser.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(ui_DATA)
CLEANFILES = $(server_DATA) $(server_DATA).in
