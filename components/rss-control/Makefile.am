NULL = 

SUBDIRS =

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-RSS-Control\"	\
	-DDATADIR=\""$(datadir)"\" 		\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_builddir)/libnautilus		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS) 			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(VFS_CFLAGS)

oafdir = $(datadir)/oaf
oaf_DATA =					\
	nautilus-rss-control.oafinfo


bin_PROGRAMS =					\
	nautilus-rss-control

nautilus_rss_control_SOURCES =		        \
	nautilus-rss-control.c			\
	nautilus-rss-control.h			\
	main.c

nautilus_rss_control_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) 					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)

EXTRA_DIST = 			\
	$(oaf_DATA)		\
	$(NULL)
