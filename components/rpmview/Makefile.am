NULL =

if EAZEL_SERVICES
INVENTORY_LIBS=$(top_builddir)/components/services/inventory/libtrilobite-inventory-service.la
INVENTORY_CFLAGS=-I$(top_srcdir)/components/services/inventory/
else
INVENTORY_LIBS=
INVENTORY_CFLAGS=
endif

INCLUDES = 							\
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"Nautilus-RPM\" 			\
	-I$(top_srcdir) 					\
	-I$(top_srcdir)/components/services/install/lib  	\
	-I$(top_srcdir)/components/services/trilobite 		\
	-I$(top_builddir)/components/services/install/lib  	\
	-I$(top_builddir)/components/services/trilobite 	\
	-I$(top_builddir) 					\
	-I/usr/include/rpm 					\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GCONF_CFLAGS) 					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	$(GNOMEUI_CFLAGS) 					\
	$(XML_CFLAGS)						\
	$(VFS_CFLAGS) 						\
	$(INVENTORY_CFLAGS) 					\
	$(NULL)

bin_PROGRAMS = nautilus-rpm-view

if EAZEL_SERVICES
extra_libs = \
	$(top_builddir)/components/services/install/lib/libeazelinstall.la \
	$(top_builddir)/components/services/install/lib/libeazelpackagesystem.la \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la 

extra_sources = \
	nautilus-rpm-view-install.h		\
	nautilus-rpm-view-install.c		
endif

nautilus_rpm_view_SOURCES =		        \
	$(extra_sources)			\
	nautilus-rpm-verify-window.c		\
	nautilus-rpm-verify-window.h		\
	nautilus-rpm-view-private.h		\
	nautilus-rpm-view.c			\
	nautilus-rpm-view.h			\
	main.c

nautilus_rpm_view_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(INVENTORY_LIBS)			\
	$(extra_libs)				\
	$(BONOBO_LIBS) 				\
	$(GCONF_LIBS) 				\
	$(GNOMEUI_LIBS) 			\
	$(XML_LIBS)				\
	$(OAF_LIBS) 				\
	$(VFS_LIBS) 				\
	$(RPM_LIBS) 				\
	$(NULL)


oafdir = $(datadir)/oaf

oaf_in_files =					\
	Nautilus_View_rpm.oaf.in		\
	$(NULL)

oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_DATA) $(oaf_in_files)

