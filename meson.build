project('nautilus', 'c',
  version: '3.28.1',
  meson_version: '>= 0.41.0',
  license: 'GPL3+'
)

# Directory variables
bindir = get_option('bindir')
datadir = get_option('datadir')
desktopdir = join_paths(datadir, 'applications')
includedir = get_option('includedir')
libdir = get_option('libdir')
extensiondir = join_paths(libdir, 'nautilus', 'extensions-3.0')
prefix = get_option('prefix')
servicedir = join_paths(datadir, 'dbus-1', 'services')
#

# Compiler variables
cc = meson.get_compiler('c')
#

add_global_arguments(
  cc.get_supported_arguments([
    '-Wno-deprecated-declarations',
  ]),
  language: 'c'
)
add_project_arguments(
  cc.get_supported_arguments([
    '-D_XOPEN_SOURCE=500',
    '-Wall',
    '-Wduplicated-branches',
    '-Wlogical-op',
    '-Werror=declaration-after-statement',
    '-Werror=empty-body',
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Werror=sequence-point',
    '-Werror=shadow',
    '-Werror=strict-prototypes',
    '-Werror=undef',
    '-Werror=uninitialized',
  ]),
  language: 'c'
)

# Imports
gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')
#

# Dependencies
glib_ver = '>= 2.55.1'

libgd = subproject(
  'libgd',
  default_options: [
    'static=true',
    'with-gtk-hacks=true',
    'with-main-view=true',
    'with-tagged-entry=true'
  ]
)
libgd_dep = libgd.get_variable('libgd_dep')

libm = cc.find_library('m')

gail = dependency('gail-3.0')
if get_option('extensions')
  gexiv = dependency('gexiv2', version: '>= 0.10.0')
endif
gio = dependency('gio-2.0', version: glib_ver)
gio_unix = dependency('gio-unix-2.0', version: glib_ver)
glib = dependency('glib-2.0', version: glib_ver)
gmodule = dependency('gmodule-no-export-2.0', version: glib_ver)
gnome_autoar = dependency('gnome-autoar-0', version: '>= 0.2.1')
gnome_desktop = dependency('gnome-desktop-3.0', version: '>= 3.0.0')
gtk = dependency('gtk+-3.0', version: '>= 3.22.26')
selinux = []
if get_option('selinux')
  selinux = dependency('libselinux', version: '>= 2.0')
endif
tracker_sparql = dependency('tracker-sparql-2.0')
x11 = dependency('x11')
xml = dependency('libxml-2.0', version: '>= 2.7.8')
#

# Configuration
conf = configuration_data()

if get_option('profile') == 'development'
  profile = 'Devel'
  name_suffix = ' (Development Snapshot)'
elif get_option('profile') == 'stable-flatpak'
  profile = 'StableFlatpak'
  version_array = meson.project_version().split('.')
  name_suffix = ' (@0@.@1@ Flatpak)'.format(version_array[0], version_array[1])
else
  profile = ''
  name_suffix = ''
endif

application_id = 'org.gnome.Nautilus@0@'.format(profile)

conf.set_quoted('VERSION', '@0@-@VCS_TAG@'.format(meson.project_version()))
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', 'nautilus')
conf.set_quoted('LOCALEDIR', join_paths(prefix, get_option('localedir')))
conf.set_quoted('NAUTILUS_DATADIR', join_paths(datadir, 'nautilus'))
conf.set_quoted('NAUTILUS_EXTENSIONDIR', join_paths(prefix, extensiondir))
conf.set_quoted('APPLICATION_ID', application_id)
conf.set_quoted('PROFILE', profile)
conf.set_quoted('NAME_SUFFIX', name_suffix)

if get_option('packagekit')
  conf.set10('ENABLE_PACKAGEKIT', true)
endif
if get_option('profiling')
  conf.set10('ENABLE_PROFILING', true)
endif
if get_option('selinux')
  conf.set10('HAVE_SELINUX', true)
endif

config_h = declare_dependency(
  sources: vcs_tag(
    command: ['git', 'rev-parse', '--short', 'HEAD'],
    fallback: get_option('profile') != 'default'? 'devel' : 'stable',
    input: configure_file(
      output: 'config.h.in',
      configuration: conf
    ),
    output: 'config.h'
  )
)

nautilus_include_dirs = include_directories(
  '.',
  'libnautilus-extension'
)

subdir('eel')
subdir('libnautilus-extension')

subdir('src')
if get_option('tests') != 'none'
  subdir('test')
endif
subdir('data')
subdir('po')
if get_option('docs')
  subdir('docs')
endif
if get_option('extensions')
  subdir('extensions')
endif

# Compile GSettings schemas when installing from source.
meson.add_install_script('build-aux/meson/postinstall.py')
