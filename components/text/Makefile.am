NULL = 

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-Text\"	\
	-DDATADIR=\""$(datadir)"\" 		\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_builddir)/libnautilus		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS) 			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(VFS_CFLAGS)

oafdir = $(datadir)/oaf
oaf_DATA =					\
	nautilus-text-view.oafinfo

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-text-view-ui.xml

bin_PROGRAMS =					\
	nautilus-text-view

nautilus_text_view_SOURCES =		        \
	nautilus-text-view.c			\
	nautilus-text-view.h			\
	main.c

nautilus_text_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) 					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)

EXTRA_DIST = 			\
	$(oaf_DATA)		\
	$(ui_DATA)		\
	$(NULL)
