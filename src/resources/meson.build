subdir('ui')

gresource_file = configure_file(
  input: 'nautilus.gresource.xml.in',
  output: 'nautilus.gresource.xml',
  configuration: file_conf
)

resources = gnome.compile_resources(
  'nautilus-resources',
  gresource_file,
  dependencies: [appdata, blueprints, gresource_file],
  source_dir: 'resources',
  c_name: 'nautilus',
  extra_args: '--manual-register'
)

resources_header = []

foreach res : resources
  filename = res.full_path().split('/')[-1]
  if filename == 'nautilus-resources.h'
    resources_header += res
  endif
endforeach
