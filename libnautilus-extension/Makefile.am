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
	nautilus-column.h                       \
	nautilus-column-provider.h              \
	nautilus-extension-types.h              \
	nautilus-file-info.h                    \
	nautilus-menu-item.h                    \
	nautilus-menu-provider.h                \
	nautilus-property-page.h                \
	nautilus-property-page-provider.h       \
	nautilus-info-provider.h                \
	$(NULL)

libnautilus_extension_la_SOURCES=			\
	nautilus-column.c                               \
	nautilus-column-provider.c                      \
	nautilus-extension-i18n.h                       \
	nautilus-extension-types.c                      \
	nautilus-file-info.c                            \
	nautilus-menu-item.c                            \
	nautilus-menu-provider.c                        \
	nautilus-property-page.c                        \
	nautilus-property-page-provider.c               \
	nautilus-info-provider.c                        \
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libnautilus-extension.pc

EXTRA_DIST = \
	libnautilus-extension.pc.in \
	$(NULL)
