icondir = join_paths('icons', 'hicolor', 'scalable', 'apps')
install_data(
  join_paths(icondir, 'org.gnome.Nautilus.svg'),
  install_dir: join_paths(datadir, icondir),
  rename: '@0@.svg'.format(application_id)
)

icondir = join_paths('icons', 'hicolor', 'symbolic', 'apps')

install_data(
  join_paths(icondir, 'org.gnome.Nautilus-symbolic.svg'),
  install_dir: join_paths(datadir, icondir),
  rename: '@0@-symbolic.svg'.format(application_id)
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

desktop_conf = configuration_data()

desktop_conf.set('icon', application_id)

desktop = i18n.merge_file(
  'desktop',
  input: configure_file(
    input: files('org.gnome.Nautilus.desktop.in.in'),
    output: 'org.gnome.Nautilus.desktop.in',
    configuration: desktop_conf
  ),
  output: '@0@.desktop'.format(application_id),
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

appdata_conf = configuration_data()

appdata_conf.set('appid', application_id)

appdata = i18n.merge_file(
  'appdata',
  input: configure_file(
    input: files('org.gnome.Nautilus.appdata.xml.in.in'),
    output: 'org.gnome.Nautilus.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: po_dir
)

service_conf = configuration_data()
service_conf.set('appid', application_id)
service_conf.set('bindir', join_paths(prefix, bindir))

configure_file(
  input: 'org.gnome.Nautilus.service.in',
  output: '@0@.service'.format(application_id),
  configuration: service_conf,
  install_dir: servicedir
)

configure_file(
  input: 'org.freedesktop.FileManager1.service.in',
  output: 'org.freedesktop.FileManager1.service',
  configuration: service_conf,
  install_dir: servicedir
)

search_provider_conf = configuration_data()

search_provider_conf.set('appid', application_id)
search_provider_conf.set('profile', profile)

configure_file(
  configuration: search_provider_conf,
  input: files('org.gnome.Nautilus.search-provider.ini.in'),
  install_dir: join_paths(datadir, 'gnome-shell', 'search-providers'),
  output: '@0@.search-provider.ini'.format(application_id)
)

gnome.compile_schemas(
  build_by_default: true,
  depend_files: 'org.gnome.nautilus.gschema.xml'
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
      'validate-relax', '--nonet', appdata.full_path()
    ]
  )
endif
