bin_PROGRAMS=ntl-notes

INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GNOMEUI_CFLAGS) $(BONOBO_CFLAGS) $(WERROR)

LDADD=\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNOMEUI_LIBS)

ntl_notes_SOURCES=ntl-notes.c

goaddir=$(sysconfdir)/CORBA/servers
goad_DATA=ntl-notes.goad

EXTRA_DIST=$(goad_DATA)
