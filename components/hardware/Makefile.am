NULL=

SUBDIRS=icons

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Hardware\"		\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-I$(top_srcdir)					\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

server_in_files = Nautilus_View_hardware.server.in

serverdir = $(libdir)/bonobo/servers
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

if ENABLE_HARDWARE_VIEW
libexec_PROGRAMS=nautilus-hardware-view
server_DATA = $(server_in_files:.server.in.in=.server)
endif

nautilus_hardware_view_SOURCES =		        \
	nautilus-hardware-view.c			\
	nautilus-hardware-view.h			\
	main.c

nautilus_hardware_view_LDADD =				\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(COMPONENT_LIBS) 				\
	$(NULL)

EXTRA_DIST = $(server_in_files) $(nautilus_hardware_view_SOURCES)

