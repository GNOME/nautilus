include $(top_srcdir)/Makefile.shared

INCLUDES = -I. -I$(srcdir) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)					\
	$(APPLETS_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DBINDIR=\""$(bindir)"\" 			\
	$(NULL)

bin_PROGRAMS = nautilus-launcher-applet

nautilus_launcher_applet_SOURCES =\
	nautilus-launcher-applet.c \
	$(NULL)

nautilus_launcher_applet_LDADD =	\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(APPLETS_LIBS) \
	$(NULL)

EXTRA_DIST =\
	nautilus-launcher-applet.gnorba \
	$(NULL)
