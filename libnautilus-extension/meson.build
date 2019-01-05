nautilus_extension_version = '1.5.0'

libnautilus_extension_deprecated_headers = [
  'nautilus-extension-types.h',
  'nautilus-menu-item.h'
]

libnautilus_extension_headers = [
  'nautilus-column-provider.h',
  'nautilus-column.h',
  'nautilus-file-info.h',
  'nautilus-info-provider.h',
  'nautilus-location-widget-provider.h',
  'nautilus-menu-provider.h',
  'nautilus-property-page-provider.h',
  'nautilus-property-page.h',
  'nautilus-menu.h'
]

install_headers(
  'nautilus-extension.h',
  subdir: 'nautilus'
)
install_headers(
  libnautilus_extension_headers,
  libnautilus_extension_deprecated_headers,
  subdir: join_paths('nautilus', 'libnautilus-extension')
)

libnautilus_extension_enums = gnome.mkenums_simple(
  'nautilus-extension-enum-types',
  install_header: true,
  install_dir: join_paths(includedir, 'nautilus', 'libnautilus-extension'),
  sources: [
    'nautilus-info-provider.h'
  ]
)

libnautilus_extension_sources = [
  libnautilus_extension_enums,
  libnautilus_extension_headers,
  'nautilus-column-provider.c',
  'nautilus-column.c',
  'nautilus-extension.h',
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

libnautilus_extension_deps = [
  config_h,
  glib,
  gtk
]

libnautilus_extension = shared_library(
  'nautilus-extension', [
    libnautilus_extension_deprecated_headers,
    libnautilus_extension_sources,
  ],
  c_args: [
    '-DNAUTILUS_COMPILATION'
  ],
  dependencies: libnautilus_extension_deps,
  include_directories: nautilus_include_dirs,
  version: nautilus_extension_version,
  install: true
)

if get_option('introspection')
  gnome.generate_gir(
    libnautilus_extension,
    export_packages: [
      'libnautilus-extension'
    ],
    extra_args: [
      '-DNAUTILUS_COMPILATION'
    ],
    sources: libnautilus_extension_sources,
    nsversion: '3.0',
    namespace: 'Nautilus',
    includes: [
      'Gtk-3.0',
      'Gio-2.0',
      'GLib-2.0'
    ],
    header: 'nautilus-extension.h',
    install: true
  )
endif

nautilus_extension = declare_dependency(
  sources: libnautilus_extension_enums[1],
  link_with: libnautilus_extension,
  dependencies: libnautilus_extension_deps,
  include_directories: nautilus_include_dirs
)

pkgconfig.generate(
  description: 'A library to create Nautilus view extensions',
  filebase: 'libnautilus-extension',
  libraries: [
    libnautilus_extension
  ],
  name: 'libnautilus-extension',
  subdirs: [
    'nautilus'
  ],
  requires: [
    'gio-2.0',
    'glib-2.0',
    'gtk+-3.0'
  ],
  variables: [
    'exec_prefix=${prefix}',
    'extensiondir=${libdir}/nautilus/extensions-3.0'
  ],
  version: meson.project_version()
)
