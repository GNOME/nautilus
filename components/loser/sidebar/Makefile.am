include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-sidebar-loser-ui.xml

bin_PROGRAMS =					\
	nautilus-sidebar-loser

nautilus_sidebar_loser_SOURCES =		\
	nautilus-sidebar-loser.h          \
	nautilus-sidebar-loser.c          \
	main.c

nautilus_sidebar_loser_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

serverdir = $(datadir)/server

server_in_files =					\
	Nautilus_View_sidebar-loser.server.in	\
	$(NULL)
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_DATA) $(server_in_files) $(ui_DATA)
