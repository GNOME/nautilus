include $(top_srcdir)/Makefile.shared

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Test\"	\
	-DDATADIR=\"$(datadir)\"		\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\" \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(GCONF_CFLAGS) 			\
	$(GNOMEUI_CFLAGS)			\
	$(NULL)

oafdir = $(datadir)/oaf
oaf_DATA = nautilus-content-loser.oafinfo

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-content-loser-ui.xml
ui_msgs = nautilus-content-loser-ui.xml.h

bin_PROGRAMS =					\
	nautilus-content-loser

nautilus_content_loser_SOURCES =		\
	nautilus-content-loser.h          \
	nautilus-content-loser.c          \
	main.c

nautilus_content_loser_LDADD =			\
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
