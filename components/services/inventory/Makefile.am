NULL =

bin_PROGRAMS = trilobite-inventory-service eazel-inventory-client


lib_LTLIBRARIES = libtrilobite-inventory-service.la

INCLUDES =								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-inventory-service\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/install/lib			\
	-I$(top_builddir)/components/services/install/lib		\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_builddir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/components/services/trilobite/libtrilobite	\
	-I$(top_builddir)/libnautilus-extensions	 		\
	-I$(top_srcdir)/libnautilus-extensions	 			\
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(AMMONITE_CFLAGS)						\
	$(GHTTP_CFLAGS)							\
	$(NULL)

trilobite_inventory_service_SOURCES =		\
	eazel-inventory-service.h		\
	eazel-inventory-service.c		\
	eazel-inventory-utils.h			\
	eazel-inventory-utils.c			\
	eazel-inventory-collect-hardware.h	\
	eazel-inventory-collect-hardware.c	\
	eazel-inventory-collect-software.h	\
	eazel-inventory-collect-software.c	\
	main.c					\
	$(NULL)

trilobite_inventory_service_LDADD =								\
	$(RPM_LIBS)										\
	libtrilobite-inventory-service.la							\
	$(top_builddir)/components/services/install/lib/libeazelinstall.la 			\
	$(top_builddir)/components/services/install/lib/libeazelpackagesystem.la		\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la		\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite-service.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la			\
	$(top_builddir)/libnautilus/libnautilus.la						\
	$(BONOBO_LIBS)										\
	$(AMMONITE_LIBS)									\
	$(GHTTP_LIBS)										\
	$(NULL)

eazel_inventory_client_SOURCES =		\
	eazel-inventory-client.c		\
	$(NULL)

eazel_inventory_client_LDADD =								\
	libtrilobite-inventory-service.la							\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la		\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite-service.la	\
	$(BONOBO_LIBS)										\
	$(NULL)



## CORBA magic

CORBA_GENERATED =					\
	eazel-inventory-service-interface.h		\
	eazel-inventory-service-interface-common.c	\
	eazel-inventory-service-interface-skels.c	\
	eazel-inventory-service-interface-stubs.c	\
	$(NULL)

$(CORBA_GENERATED): idl_stamp

IDL = eazel-inventory-service-interface.idl

idl_stamp: $(IDL)
	$(ORBIT_IDL) $(IDL_CFLAGS) $(IDL)
	touch idl_stamp

CLEANFILES += $(CORBA_GENERATED) idl_stamp

trilobiteincludedir = $(includedir)/trilobite

trilobiteinclude_HEADERS =			\
	eazel-inventory-service-interface.h	\
	$(NULL)

libtrilobite_inventory_service_la_SOURCES =	\
	$(CORBA_GENERATED)			\
	eazel-inventory.c			\
	eazel-inventory.h			\
	eazel-inventory-upload-callback.c	\
	eazel-inventory-upload-callback.h	\
	$(NULL)


oafdir = $(datadir)/oaf
oaf_in_files = Trilobite_Service_inventory.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST =					\
	$(oaf_in_files)				\
	$(oaf_DATA)				\
	$(IDL)					\
	$(NULL)
