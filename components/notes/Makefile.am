bin_PROGRAMS=ntl-notes

INCLUDES=\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)	\
	$(GCONF_CFLAGS)		\
	$(BONOBO_CFLAGS)	\
	$(WERROR) 

LDADD=\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(top_builddir)/librsvg/librsvg.la		\
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS)

ntl_notes_SOURCES=ntl-notes.c

oafdir=$(datadir)/oaf
oaf_DATA=ntl-notes.oafinfo

EXTRA_DIST=$(oaf_DATA)
