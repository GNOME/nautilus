include $(top_srcdir)/Makefile.shared

if BUILD_RPM_COMPONENTS
INVENTORY_LIBS=$(top_builddir)/components/services/inventory/libtrilobite-inventory-service.la
else
INVENTORY_LIBS=
endif

SUBDIRS = icons

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-summary\"			\
	-DDATADIR=\"$(datadir)\"					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/nautilus-dependent-shared	\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_builddir)						\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	$(VFS_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA = nautilus-summary-view-ui.xml

bin_PROGRAMS =								\
	nautilus-summary-view

nautilus_summary_view_SOURCES =			\
	eazel-summary-shared.c			\
	eazel-summary-shared.h			\
	main.c					\
	nautilus-summary-callbacks.c		\
	nautilus-summary-callbacks.h		\
	nautilus-summary-dialogs.c		\
	nautilus-summary-dialogs.h		\
	nautilus-summary-footer.c		\
	nautilus-summary-footer.h		\
	nautilus-summary-menu-items.c		\
	nautilus-summary-menu-items.h		\
	nautilus-summary-view-private.h		\
	nautilus-summary-view.c			\
	nautilus-summary-view.h			\
	$(NULL)

nautilus_summary_view_LDADD =									\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la		\
	$(top_builddir)/components/services/nautilus-dependent-shared/libnautilusdepshared.a	\
	$(INVENTORY_LIBS)									\
	$(top_builddir)/libnautilus/libnautilus.la						\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la			\
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS)										\
	$(GNOMEUI_LIBS)										\
	$(GCONF_LIBS)										\
	$(VFS_LIBS)										\
	$(GHTTP_LIBS)										\
	$(OAF_LIBS)										\
	$(AMMONITE_LIBS)									\
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_View_services-summary.oaf.in	\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA) $(ui_DATA) 
