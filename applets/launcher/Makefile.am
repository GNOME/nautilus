include $(top_srcdir)/Makefile.shared

INCLUDES = -I. -I$(srcdir) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(cflags) \
	$(GNOME_CFLAGS) \
	$(EEL_CFLAGS) \
	$(LIBRSVG_CFLAGS) \
	$(APPLETS_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	$(NULL)

bin_PROGRAMS = nautilus-launcher-applet

nautilus_launcher_applet_SOURCES =\
	nautilus-launcher-applet.c \
	$(NULL)

nautilus_launcher_applet_LDADD =	\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(APPLETS_LIBS) \
	$(NULL)

gnorbadir  = $(sysconfdir)/CORBA/servers
gnorba_DATA = nautilus-launcher-applet.gnorba

EXTRA_DIST =\
	$(gnorba_DATA) \
	$(NULL)
