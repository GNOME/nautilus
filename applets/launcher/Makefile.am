include $(top_srcdir)/Makefile.shared

INCLUDES =\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	-I$(top_srcdir) \
	-I$(srcdir) \
	$(APPLETS_CFLAGS) \
	$(NAUTILUS_PRIVATE_CFLAGS) \
	$(NULL)

bin_PROGRAMS = nautilus-launcher-applet

nautilus_launcher_applet_SOURCES =\
	nautilus-launcher-applet.c \
	$(NULL)

nautilus_launcher_applet_LDADD =	\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(APPLETS_LIBS) \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

gnorbadir  = $(sysconfdir)/CORBA/servers
gnorba_DATA = nautilus-launcher-applet.gnorba

EXTRA_DIST =\
	$(gnorba_DATA) \
	$(NULL)
