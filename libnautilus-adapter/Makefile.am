NULL=

lib_LTLIBRARIES=libnautilus-adapter.la


INCLUDES=\
	-I$(top_srcdir) \
	$(CORE_CFLAGS) \
	$(NULL)

libnautilus_adapter_la_LDFLAGS= \
	-version-info 2:0:0	\
	$(NULL)

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
nautilus_adapter_factory_idl_stamp: nautilus-adapter-factory.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I$(top_srcdir) $(CORE_IDL_INCLUDES) $<
	touch $@

CLEANFILES = \
	$(nautilus_adapter_factory_idl_sources)	        \
	nautilus_adapter_factory_idl_stamp

EXTRA_DIST = \
	nautilus-adapter-factory.idl

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

