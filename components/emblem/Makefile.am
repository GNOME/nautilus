NULL =

INCLUDES =						\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-DG_LOG_DOMAIN=\"Nautilus-Emblem-Sidebar\"	\
	-DDATADIR=\""$(datadir)"\" \
	-I$(top_srcdir)					\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(EMBLEM_COMPONENT_CFLAGS)			\
	$(NULL)

EMBLEM_VIEW_COMMON_SOURCES =			\
	nautilus-emblem-view.c			\
	nautilus-emblem-view.h			\
	nautilus-emblem-view-iids.h		\
	$(NULL)

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libnautilus-emblem-view.la

libnautilus_emblem_view_la_SOURCES = $(EMBLEM_VIEW_COMMON_SOURCES) \
					libmain.c 

libnautilus_emblem_view_la_LDFLAGS = -module -avoid-version

libnautilus_emblem_view_la_LIBADD =               \
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(EMBLEM_COMPONENT_LIBS)
	$(NULL)

server_in_files = Nautilus_View_emblem.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files)
