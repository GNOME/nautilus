CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GCONF_CFLAGS) 			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(GNOMEUI_CFLAGS)

oafdir = $(datadir)/oaf
oaf_DATA = nautilus-sidebar-loser.oafinfo

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-sidebar-loser-ui.xml
ui_msgs = nautilus-sidebar-loser-ui.xml.h

nautilus-sidebar-loser-ui.xml.h: $(srcdir)/nautilus-sidebar-loser-ui.xml
	$(BONOBO_UI_EXTRACT) $(srcdir)/nautilus-sidebar-loser-ui.xml >	\
	nautilus-sidebar-loser-ui.xml.h

bin_PROGRAMS =					\
	nautilus-sidebar-loser

nautilus_sidebar_loser_SOURCES =		\
	nautilus-sidebar-loser.h          \
	nautilus-sidebar-loser.c          \
	main.c

nautilus_sidebar_loser_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(GNOMEUI_LIBS)					\
	$(BONOBO_LIBS)					\
	$(GCONF_LIBS) \
	$(VFS_LIBS)					\
	$(OAF_LIBS)


EXTRA_DIST = $(oaf_DATA) $(ui_DATA) $(ui_msgs)

CLEANFILES = $(ui_msgs)

BUILT_SOURCES = $(ui_msgs)


