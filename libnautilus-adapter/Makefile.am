NULL=

lib_LTLIBRARIES=libnautilus-adapter.la


INCLUDES=-I$(top_srcdir) -I$(top_builddir) -I. \
	$(GNOME_CFLAGS) \
	$(BONOBOX_CFLAGS) \
	$(OAF_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-D_REENTRANT

libnautilus_adapter_la_LDFLAGS= \
	$(GNOME_LIBS) \
	$(OAF_LIBS) \
	$(BONOBOX_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(LIBPNG)


nautilus_adapter_factory_idl_sources =	        \
	nautilus-adapter-factory-stubs.c	\
	nautilus-adapter-factory-skels.c	\
	nautilus-adapter-factory.h		\
	nautilus-adapter-factory-common.c


BUILT_SOURCES = \
	$(nautilus_adapter_factory_idl_sources)


libnautilus_adapter_la_SOURCES = 		\
	$(nautilus_adapter_factory_idl_sources) \
	$(NULL)


$(nautilus_adapter_factory_idl_sources): nautilus_adapter_factory_idl_stamp
nautilus_adapter_factory_idl_stamp: nautilus-adapter-factory.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) $(srcdir)/nautilus-adapter-factory.idl
	touch nautilus_adapter_factory_idl_stamp


CLEANFILES = \
	$(nautilus_adapter_factory_idl_sources)	        \
	nautilus_adapter_factory_idl_stamp

EXTRA_DIST = \
	nautilus-adapter-factory.idl

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
