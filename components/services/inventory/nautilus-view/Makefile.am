NULL =

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"					\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-inventory\"		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/inventory/lib		\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"			\
	$(VFS_CFLAGS) 							\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =								\
	nautilus-inventory-view.oafinfo


bin_PROGRAMS =								\
	nautilus-inventory-view

nautilus_inventory_view_SOURCES =					\
	nautilus-inventory-view.h					\
	nautilus-inventory-view.c		    			\
	main.c								\
	$(NULL)

nautilus_inventory_view_LDADD =							\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/libnautilus/libnautilus.la				\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)								\
	$(GNOMEUI_LIBS)								\
	$(GCONF_LIBS)								\
	$(VFS_LIBS)								\
	$(GHTTP_LIBS)								\
	$(OAF_LIBS)								\
	$(top_builddir)/components/services/inventory/lib/libinventory.a	\
	$(RPM_LIBS)								\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
