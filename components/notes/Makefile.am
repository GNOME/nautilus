bin_PROGRAMS=nautilus-notes

INCLUDES=\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/libnautilus	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)	\
	$(GCONF_CFLAGS)		\
	$(BONOBO_CFLAGS)	\
	$(VFS_CFLAGS)

LDADD=\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

nautilus_notes_SOURCES=nautilus-notes.c

oafdir=$(datadir)/oaf
oaf_DATA=nautilus-notes.oafinfo

EXTRA_DIST=$(oaf_DATA)
