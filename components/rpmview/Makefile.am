NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-RPM\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I/usr/include/rpm \
	$(GCONF_CFLAGS) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS) \
	$(WERROR) \
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA = nautilus-rpm-view.oafinfo

bin_PROGRAMS = nautilus-rpm-view

nautilus_rpm_view_SOURCES =		        \
	nautilus-rpm-view.h			\
	nautilus-rpm-view.c			\
	main.c

nautilus_rpm_view_LDADD = \
	$(RPM_LIBS) 

nautilus_rpm_view_LDFLAGS =			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la \
	$(top_builddir)/librsvg/librsvg.la \
	$(BONOBO_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(OAF_LIBS) \
	$(VFS_LIBS) \
	$(RPM_LIBS) \
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
