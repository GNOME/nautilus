NULL=

SUBDIRS=icons

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Hardware\"		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)					\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files = Nautilus_View_hardware.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

bin_PROGRAMS =nautilus-hardware-view

nautilus_hardware_view_SOURCES =		        \
	nautilus-hardware-view.c			\
	nautilus-hardware-view.h			\
	main.c

nautilus_hardware_view_LDADD =				\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la	\
	$(COMPONENT_LIBS) 				\
	$(NULL)

EXTRA_DIST = $(server_DATA) $(server_in_files)
