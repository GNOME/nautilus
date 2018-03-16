install_subdir(
  join_paths('icons', 'hicolor'),
  install_dir: join_paths(datadir, 'icons')
)

po_dir = join_paths(meson.source_root(), 'po')

line_up_parameters = executable(
  'lineup-parameters',
  'lineup-parameters.c',
  dependencies: [
    gio,
    gio_unix,
  ]
)

desktop_exec = 'nautilus'
if get_option('devel')
  desktop_exec = '@0@ --gapplication-app-id=org.gnome.NautilusDevel'.format(desktop_exec)
endif

desktop_conf = configuration_data()
desktop_conf.set('exec', desktop_exec)

desktop_output = 'org.gnome.Nautilus.desktop'
if get_option('devel')
  desktop_output = 'org.gnome.NautilusDevel.desktop'
endif

desktop = i18n.merge_file(
  'desktop',
  input: configure_file(
    input: 'org.gnome.Nautilus.desktop.in.in',
    output: 'org.gnome.Nautilus.desktop.in',
    configuration: desktop_conf
  ),
  output: desktop_output,
  install: true,
  install_dir: desktopdir,
  po_dir: po_dir,
  type: 'desktop'
)

desktop_autorun_software = i18n.merge_file(
  'desktop-autorun-software',
  input: 'nautilus-autorun-software.desktop.in',
  output: 'nautilus-autorun-software.desktop',
  install: true,
  install_dir: desktopdir,
  po_dir: po_dir,
  type: 'desktop'
)

appdata = i18n.merge_file(
  'appdata',
  input: 'org.gnome.Nautilus.appdata.xml.in',
  output: 'org.gnome.Nautilus.appdata.xml',
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: po_dir
)

service_conf = configuration_data()
service_conf.set('bindir', join_paths(prefix, bindir))

if get_option('devel')
  configure_file(
    input: 'org.gnome.NautilusDevel.service.in',
    output: 'org.gnome.NautilusDevel.service',
    configuration: service_conf,
    install_dir: servicedir
  )
else
  configure_file(
    input: 'org.gnome.Nautilus.service.in',
    output: 'org.gnome.Nautilus.service',
    configuration: service_conf,
    install_dir: servicedir
  )
endif

configure_file(
  input: 'org.freedesktop.FileManager1.service.in',
  output: 'org.freedesktop.FileManager1.service',
  configuration: service_conf,
  install_dir: servicedir
)

install_data(
  'nautilus-search-provider.ini',
  install_dir: join_paths(datadir, 'gnome-shell', 'search-providers')
)

install_data(
  'org.gnome.nautilus.gschema.xml',
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas')
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: [
      desktop.full_path()
    ]
  )
  test(
    'validate-desktop-autorun-software',
    desktop_file_validate,
    args: [
      desktop_autorun_software.full_path()
    ]
  )
endif

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata', appstream_util,
    args: [
      'validate-relax', appdata.full_path()
    ]
  )
endif
