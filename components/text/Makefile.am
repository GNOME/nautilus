NULL = 

SUBDIRS = services

INCLUDES =					\
	-DG_LOG_DOMAIN=\"Nautilus-Text\"	\
	-DDATADIR=\""$(datadir)"\" 		\
	-I$(top_srcdir)				\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)		\
	$(COMPONENT_CFLAGS)			\
	-DGTK_ENABLE_BROKEN			\
	$(NULL)

libexec_PROGRAMS=nautilus-text-view

nautilus_text_view_SOURCES =		        \
	nautilus-text-view.c

nautilus_text_view_LDADD =			\
	../../libnautilus/libnautilus.la        \
	$(COMPONENT_LIBS)			\
	$(NULL)

#uidir = $(datadir)/gnome-2.0/ui
#ui_DATA = nautilus-text-view-ui.xml

server_in_files = Nautilus_View_text.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(nautilus_text_view_SOURCES)
