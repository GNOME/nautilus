include $(top_srcdir)/Makefile.shared

INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DG_LOG_DOMAIN=\"Nautilus-Inventory\"		\
	-DDATADIR=\"$(datadir)\"			\
	-DICON_DIR=\"$(datadir)/pixmaps/nautilus\"	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/components/services/inventory	\
	-I$(top_builddir)/components/services/inventory	\
	-I$(top_srcdir)/components/services/nautilus-dependent-shared   \
	$(GNOMEUI_CFLAGS)				\
	$(BONOBO_CFLAGS)				\
	$(OAF_CFLAGS)

oafdir = $(datadir)/oaf
oaf_in_files = Nautilus_View_inventory.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

bin_PROGRAMS =					\
	nautilus-inventory-view

nautilus_inventory_view_SOURCES =		\
	nautilus-inventory-view.h		\
	nautilus-inventory-view.c		\
	nautilus-inventory-view-private.h	\
	nautilus-inventory-enable-page.h	\
	nautilus-inventory-enable-page.c	\
	nautilus-inventory-disable-page.h	\
	nautilus-inventory-disable-page.c	\
	main.c

nautilus_inventory_view_LDADD =								\
	$(top_builddir)/libnautilus/libnautilus.la					\
	$(top_builddir)/components/services/inventory/libtrilobite-inventory-service.la	\
	$(top_builddir)/components/services/nautilus-dependent-shared/libnautilusdepshared.a \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la 		\
	$(GNOMEUI_LIBS)									\
	$(BONOBO_LIBS)									\
	$(GCONF_LIBS)									\
	$(VFS_LIBS)									\
	$(OAF_LIBS)

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)
