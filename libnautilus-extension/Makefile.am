include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnautilus-extension.la

INCLUDES=\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBNAUTILUS_EXTENSION_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

libnautilus_extension_la_LDFLAGS=\
	-version-info 1:0:0 \
	$(LIBNAUTILUS_EXTENSION_LIBS) \
	-no-undefined \
	$(NULL)

libnautilus_extension_includedir=$(includedir)/nautilus/libnautilus-extension

libnautilus_extension_include_HEADERS=		\
	nautilus-extension-types.h              \
	nautilus-file-info.h                    \
	nautilus-info-provider.h                \
	$(NULL)

libnautilus_extension_la_SOURCES=			\
	nautilus-extension-types.c                      \
	nautilus-file-info.c                            \
	nautilus-info-provider.c                        \
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libnautilus-extension.pc

EXTRA_DIST = \
	libnautilus-extension.pc.in \
	$(NULL)
