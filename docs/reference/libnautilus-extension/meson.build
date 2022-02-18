docs_conf = configuration_data()
docs_conf.set('NAUTILUS_EXTENSION_VERSION_INFO', nautilus_extension_version)

configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: docs_conf
)

gnome.gtkdoc(
  'libnautilus-extension',
  main_xml: 'libnautilus-extension-docs.xml',
  src_dir: join_paths(meson.project_source_root(), 'libnautilus-extension'),
  dependencies: nautilus_extension,
  install: true,
  scan_args: [
    '--ignore-headers=nautilus-extension-private.h',
    '--rebuild-types'
  ],
)
