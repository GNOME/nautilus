########## EVIL EVIL EVIL
# See https://github.com/mesonbuild/meson/issues/1487
# and https://github.com/mesonbuild/meson/pull/3223
#
# This is primarily for the “build profiles” feature,
# to be able to have icons even if we change the application ID.
#
# https://gitlab.gnome.org/GNOME/nautilus/merge_requests/144
##########
foreach icon_size: ['16x16', '22x22', '24x24', '32x32', '48x48', '512x512']
  configure_file(
    command: [
      'cp', '@INPUT@', '@OUTPUT@'
    ],
    input: files(
      join_paths('icons', 'hicolor', icon_size, 'apps', 'org.gnome.Nautilus.png')
    ),
    install_dir: join_paths(datadir, 'icons', 'hicolor', icon_size, 'apps'),
    output: '@0@.png'.format(application_id)
  )
endforeach

configure_file(
  command: [
    'cp', '@INPUT@', '@OUTPUT@'
  ],
  input: files(
    join_paths('icons', 'hicolor', 'symbolic', 'apps', 'org.gnome.Nautilus-symbolic.svg')
  ),
  install_dir: join_paths(datadir, 'icons', 'hicolor', 'symbolic', 'apps'),
  output: '@0@-symbolic.svg'.format(application_id)
)
##########

po_dir = join_paths(meson.source_root(), 'po')

line_up_parameters = executable(
  'lineup-parameters',
  'lineup-parameters.c',
  dependencies: [
    gio,
    gio_unix,
  ]
)

desktop = i18n.merge_file(
  'desktop',
  input: 'org.gnome.Nautilus.desktop.in',
  output: 'org.gnome.Nautilus.desktop',
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

install_data(
  'org.gnome.Nautilus.search-provider.ini',
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
