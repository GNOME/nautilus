NULL=

SUBDIRS=icons

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Hardware\"		\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-I$(top_srcdir)					\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_hardware.server.in
server_output = $(server_in_files:.server.in=.server)

if ENABLE_HARDWARE_VIEW
bin_PROGRAMS=nautilus-hardware-view
server_DATA = $(server_output)
endif

@INTLTOOL_SERVER_RULE@

nautilus_hardware_view_SOURCES =		        \
	nautilus-hardware-view.c			\
	nautilus-hardware-view.h			\
	main.c

nautilus_hardware_view_LDADD =				\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(COMPONENT_LIBS) 				\
	$(NULL)

EXTRA_DIST = $(server_output) $(server_in_files) $(nautilus_hardware_view_SOURCES)

