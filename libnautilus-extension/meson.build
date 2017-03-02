nautilus_extension_version = '1.4.0'

libnautilus_extension_headers = [
    'nautilus-column-provider.h',
    'nautilus-column.h',
    'nautilus-extension-types.h',
    'nautilus-file-info.h',
    'nautilus-info-provider.h',
    'nautilus-location-widget-provider.h',
    'nautilus-menu-item.h',
    'nautilus-menu-provider.h',
    'nautilus-property-page-provider.h',
    'nautilus-property-page.h',
    'nautilus-menu.h'
]

install_headers (libnautilus_extension_headers,
                 subdir: join_paths ('nautilus', 'libnautilus-extension'))

libnautilus_extension_sources = [
    'nautilus-column-provider.c',
    'nautilus-column.c',
    'nautilus-extension-types.c',
    'nautilus-extension-private.h',
    'nautilus-file-info.c',
    'nautilus-info-provider.c',
    'nautilus-location-widget-provider.c',
    'nautilus-menu-item.c',
    'nautilus-menu-provider.c',
    'nautilus-property-page-provider.c',
    'nautilus-property-page.c',
    'nautilus-menu.c'
]

libnautilus_extension_deps = [glib, gtk]

libnautilus_extension = shared_library ('nautilus-extension',
                                        libnautilus_extension_sources,
                                        dependencies: libnautilus_extension_deps,
                                        include_directories: nautilus_include_dirs,
                                        version: nautilus_extension_version,
                                        install: true)

gnome.generate_gir (libnautilus_extension,
                    sources: libnautilus_extension_headers + libnautilus_extension_sources,
                    nsversion: '3.0',
                    namespace: 'Nautilus',
                    includes: ['Gtk-3.0', 'Gio-2.0', 'GLib-2.0'],
                    install: true)

nautilus_extension = declare_dependency (link_with: libnautilus_extension,
                                         dependencies: libnautilus_extension_deps,
                                         include_directories: nautilus_include_dirs)

pkgconfig_data = configuration_data ()

pkgconfig_data.set ('prefix', prefix)
pkgconfig_data.set ('exec_prefix', '${prefix}')
pkgconfig_data.set ('libdir', join_paths ('${exec_prefix}', libdir))
pkgconfig_data.set ('includedir', join_paths ('${prefix}', includedir))
pkgconfig_data.set ('extensiondir', join_paths ('${exec_prefix}', extensiondir))
pkgconfig_data.set ('VERSION', meson.project_version ())

configure_file (input: 'libnautilus-extension.pc.in',
                output: 'libnautilus-extension.pc',
                configuration: pkgconfig_data,
                install_dir: join_paths (libdir, 'pkgconfig'))
