include $(top_srcdir)/Makefile.shared

INCLUDES =\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	-I$(top_srcdir) \
	$(APPLETS_CFLAGS) \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

bin_PROGRAMS = nautilus-preferences-applet

nautilus_preferences_applet_SOURCES =\
	nautilus-preferences-applet.c \
	$(NULL)

nautilus_preferences_applet_LDADD =	\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(APPLETS_LIBS) \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

gnorbadir  = $(sysconfdir)/CORBA/servers
gnorba_DATA = nautilus-preferences-applet.gnorba

EXTRA_DIST =\
	$(gnorba_DATA) \
	$(NULL)
