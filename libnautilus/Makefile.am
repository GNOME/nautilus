lib_LTLIBRARIES=libnautilus.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOME_CFLAGS) $(GNORBA_CFLAGS)

nautilus_idl_sources=nautilus-stubs.c nautilus-skels.c nautilus.h nautilus-common.c
BUILT_SOURCES=$(nautilus_idl_sources)

libnautilusincludedir=$(includedir)/libnautilus
libnautilusinclude_HEADERS=			\
	ntl-view-client.h			\
	ntl-meta-view-client.h			\
	nautilus.h

libnautilus_la_SOURCES=$(nautilus_idl_sources)	\
	ntl-view-client.c			\
	ntl-meta-view-client.c

$(nautilus_idl_sources): nautilus_idl_stamp

nautilus_idl_stamp: $(top_srcdir)/idl/nautilus.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/nautilus.idl
	touch nautilus_idl_stamp

CLEANFILES=nautilus_idl_stamp
