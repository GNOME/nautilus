NULL =

CPPFLAGS = 								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-eazel-inventory\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/inventory/lib		\
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(NULL)

LDFLAGS = 								\
	$(GLIB_LIBS)							\
	$(OAF_LIBS) 							\
	$(NULL)


oafdir = $(datadir)/oaf

oaf_DATA = trilobite-eazel-inventory-service.oafinfo

bin_PROGRAMS = trilobite-eazel-inventory-service

trilobite_eazel_inventory_service_SOURCES =				\
	trilobite-eazel-inventory-impl.c				\
	main.c								\
	$(NULL)

trilobite_eazel_inventory_service_LDADD =				\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/inventory/lib/libinventory_corba.a	\
	$(NULL)

EXTRA_DIST = 								\
	$(oaf_DATA)							\
	$(NULL)
