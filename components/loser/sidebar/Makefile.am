CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Test\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GCONF_CFLAGS) 			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(GNOMEUI_CFLAGS)

oafdir = $(datadir)/oaf

oaf_DATA =					\
	nautilus-sidebar-loser.oafinfo


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

EXTRA_DIST = $(oaf_DATA)
