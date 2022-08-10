toml_data = configuration_data()
toml_data.set('VERSION', meson.project_version())

nautilus_toml = configure_file(
  input: 'nautilus.toml.in',
  output: 'nautilus.toml',
  configuration: toml_data,
)

custom_target(
  'nautilus-doc',
  input: [
    nautilus_toml,
    nautilus_extension_gir[0],
  ],
  output: 'nautilus',
  command: [
    gi_docgen,
    'generate',
    '--quiet',
    '--add-include-path=@0@'.format(meson.project_source_root() / 'libnautilus-extension'),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: docdir,
)
