include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	$(COMPONENT_CFLAGS) \
	$(NULL)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = nautilus-sidebar-loser-ui.xml

libexec_PROGRAMS=nautilus-sidebar-loser

nautilus_sidebar_loser_SOURCES =		\
	nautilus-sidebar-loser.h          \
	nautilus-sidebar-loser.c          \
	main.c

nautilus_sidebar_loser_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(COMPONENT_LIBS) \
	$(NULL)

server_in_files = Nautilus_View_sidebar-loser.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(ui_DATA)
