NULL = 

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Throbber\"		\
	-DDATADIR=\""$(datadir)"\" 			\
	-I$(top_srcdir)					\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(COMPONENT_CFLAGS)				\
	$(NULL)

libexec_PROGRAMS=nautilus-throbber

nautilus_throbber_SOURCES =		        \
	nautilus-throbber.c			\
	nautilus-throbber.h			\
	main.c

nautilus_throbber_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(COMPONENT_LIBS)
	$(NULL)

server_in_files = Nautilus_Control_throbber.server.in.in

serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)
$(server_in_files:.server.in.in=.server.in): $(server_in_files)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files)
CLEANFILES = $(server_DATA) $(server_DATA).in
