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
	-version-info @NAUTILUS_EXTENSION_VERSION_INFO@ \
	-no-undefined \
	$(NULL)

libnautilus_extension_la_LIBADD = $(LIBNAUTILUS_EXTENSION_LIBS)

libnautilus_extension_includedir=$(includedir)/nautilus/libnautilus-extension

libnautilus_extension_include_HEADERS=		\
	nautilus-column-provider.h              \
	nautilus-column.h                       \
	nautilus-extension-types.h              \
	nautilus-file-info.h                    \
	nautilus-info-provider.h                \
	nautilus-location-widget-provider.h     \
	nautilus-menu-item.h                    \
	nautilus-menu-provider.h                \
	nautilus-property-page-provider.h       \
	nautilus-property-page.h                \
	nautilus-menu.h                         \
	$(NULL)

libnautilus_extension_la_SOURCES=			\
	nautilus-column-provider.c                      \
	nautilus-column.c                               \
	nautilus-extension-i18n.h                       \
	nautilus-extension-types.c                      \
	nautilus-file-info.c                            \
	nautilus-info-provider.c                        \
	nautilus-location-widget-provider.c             \
	nautilus-menu-item.c                            \
	nautilus-menu-provider.c                        \
	nautilus-property-page-provider.c               \
	nautilus-property-page.c                        \
	nautilus-menu.c                                 \
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libnautilus-extension.pc

EXTRA_DIST = \
	libnautilus-extension.pc.in \
	$(NULL)
