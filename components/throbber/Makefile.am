NULL = 

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Throbber\"		\
	-DDATADIR=\""$(datadir)"\" 			\
	-I$(top_srcdir)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

bin_PROGRAMS =					\
	nautilus-throbber

nautilus_throbber_SOURCES =		        \
	nautilus-throbber.c			\
	nautilus-throbber.h			\
	main.c

nautilus_throbber_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(COMPONENT_LIBS)
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_in_files =					\
	Nautilus_Control_throbber.server.in	\
	$(NULL)
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = 			\
	$(server_in_files)		\
	$(server_DATA)		\
	$(NULL)
