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

bin_PROGRAMS = nautilus-preferences-applet

nautilus_preferences_applet_SOURCES =\
	nautilus-preferences-applet.c \
	$(NULL)

nautilus_preferences_applet_LDADD =	\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(APPLETS_LIBS) \
	$(NULL)

gnorbadir  = $(sysconfdir)/CORBA/servers
gnorba_DATA = nautilus-preferences-applet.gnorba

EXTRA_DIST =\
	$(gnorba_DATA) \
	$(NULL)
