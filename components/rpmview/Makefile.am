NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-RPM\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/components/services/install/lib  \
	-I$(top_srcdir)/components/services/trilobite \
	-I$(top_builddir)/components/services/install/lib  \
	-I$(top_builddir)/components/services/trilobite \
	-I$(top_builddir) \
	-I/usr/include/rpm \
	$(GCONF_CFLAGS) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS) \
	$(VFS_CFLAGS) \
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA = nautilus-rpm-view.oafinfo

bin_PROGRAMS = nautilus-rpm-view

if EAZEL_SERVICES
extra_libs = \
	$(top_builddir)/components/services/install/lib/libeazelinstall.la \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la 
extra_sources = \
	nautilus-rpm-view-install.h		\
	nautilus-rpm-view-install.c		
endif

nautilus_rpm_view_SOURCES =		        \
	$(extra_sources)			\
	nautilus-rpm-view.h			\
	nautilus-rpm-view.c			\
	main.c

nautilus_rpm_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(extra_libs)				\
	$(BONOBO_LIBS) 				\
	$(GCONF_LIBS) 				\
	$(GNOMEUI_LIBS) 			\
	$(OAF_LIBS) 				\
	$(VFS_LIBS) 				\
	$(RPM_LIBS) 				\
	$(NULL)

EXTRA_DIST = \
	$(oaf_DATA) \
	nautilus-rpm-view-install.h		\
	nautilus-rpm-view-install.c		 \
	$(NULL)
