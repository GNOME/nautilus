NULL =

bin_PROGRAMS = nautilus-image-view

nautilus_image_view_SOURCES =	\
	nautilus-image-view.c	\
	io-png.c		\
	io-png.h

INCLUDES =	\
	-I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

nautilus_image_view_LDADD = 			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

serverdir = $(libdir)/bonobo/servers

server_in_files =					\
	Nautilus_View_image.server.in		\
	$(NULL)

server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_DATA) $(server_in_files)
