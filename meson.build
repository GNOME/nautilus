project('nautilus', 'c',
  default_options: ['c_std=c11'],

  # Do not forget when releasing:
  #  * Update version in data/org.gnome.Nautilus.appdata.xml.in.in
  version: '45.rc',

  meson_version: '>= 0.59.0',
  license: 'GPL-3.0-or-later'
)

nautilus_extension_version = '4'

###############
# Directories #
###############

prefix = get_option('prefix')

bindir = get_option('bindir')
datadir = get_option('datadir')
desktopdir = join_paths(datadir, 'applications')
includedir = get_option('includedir')
libdir = get_option('libdir')
localedir = get_option('localedir')
docdir = datadir / 'doc'
extensiondir = join_paths(libdir, 'nautilus', 'extensions-' + nautilus_extension_version)
servicedir = join_paths(datadir, 'dbus-1', 'services')

###################
# End directories #
###################

#############
# Compilers #
#############

cc = meson.get_compiler('c')

#################
# End compilers #
#################

add_project_arguments(
  cc.get_supported_arguments([
    '-Wall',
    '-Wduplicated-branches',
    '-Wduplicated-cond',
    '-Wlogical-op',
    '-Werror=declaration-after-statement',
    '-Werror=empty-body',
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=incompatible-pointer-types',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Werror=sequence-point',
    '-Werror=shadow',
    '-Werror=strict-prototypes',
    '-Werror=undef',
    '-Werror=uninitialized',
    # There are currently too many warnings caused by deprecations, hindering the usefulness
    # of any other warnings. Disable them until
    # https://gitlab.gnome.org/GNOME/nautilus/-/issues/2722 is resolved.
    '-Wno-deprecated-declarations',
    # Context: https://gitlab.com/freedesktop-sdk/freedesktop-sdk/commit/ce54a2527555e51e4ebf4cce9cbb6259cafa89a4
    # -O2 and -fexceptions unmask some cases of -Wmaybe-uninitialized, which
    # become errors for some reason (I’m guessing because of the
    # -Werror=uninitialized above, but no idea why that implies this).
    # This being an error is nice and all, but not when its appearance is
    # dependent on optimization level and other flags.
    '-Wno-error=maybe-uninitialized',
  ]),
  '-D_GNU_SOURCE',
  language: 'c'
)

##################
# Module imports #
##################

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')

######################
# End module imports #
######################

################
# Dependencies #
################
glib_ver = '>= 2.77.0'

libm = cc.find_library('m')

if get_option('extensions')
  gexiv = dependency('gexiv2', version: '>= 0.14.0')
  gdkpixbuf = dependency('gdk-pixbuf-2.0', version: '>= 2.30.0')
  gst_tag_dep = dependency('gstreamer-tag-1.0')
  gst_pbutils_dep = dependency('gstreamer-pbutils-1.0')
endif
gio = dependency('gio-2.0', version: glib_ver)
gio_unix = dependency('gio-unix-2.0', version: glib_ver)
glib = dependency('glib-2.0', version: glib_ver)
gmodule = dependency('gmodule-no-export-2.0', version: glib_ver)
gnome_autoar = dependency('gnome-autoar-0', version: '>= 0.4.4')
gnome_desktop = dependency('gnome-desktop-4', version: '>= 43')
gtk = dependency('gtk4', version: '>= 4.11.2')
libadwaita = dependency('libadwaita-1', version: '>= 1.4.alpha')
libportal = dependency('libportal', version: '>= 0.5')
libportal_gtk4 = dependency('libportal-gtk4', version: '>= 0.5')
selinux = []
if get_option('selinux')
  selinux = dependency('libselinux', version: '>= 2.0')
endif
tracker_sparql = dependency('tracker-sparql-3.0')
xml = dependency('libxml-2.0', version: '>= 2.7.8')
cloudproviders = []
if get_option('cloudproviders')
  cloudproviders = dependency('cloudproviders', version: '>= 0.3.1')
endif
gi_docgen = find_program('gi-docgen', required: get_option('docs'))

####################
# End dependencies #
####################

#################
# Configuration #
#################
conf = configuration_data()

profile = get_option('profile')
name_suffix = ''

if get_option('profile') == 'Devel'
  name_suffix = ' (Development Snapshot)'
endif

application_id = 'org.gnome.Nautilus' + profile

if profile == ''
  # Example: 40.0
  version_string = meson.project_version()
else
  # Examples: 40.alpha-787a835f
  version_string = '@0@-@VCS_TAG@'.format(meson.project_version())
endif

conf.set_quoted('APPLICATION_ID', application_id)
conf.set_quoted('GETTEXT_PACKAGE', 'nautilus')
conf.set_quoted('LOCALEDIR', join_paths(prefix, localedir))
conf.set_quoted('NAME_SUFFIX', name_suffix)
conf.set_quoted('NAUTILUS_DATADIR', join_paths(prefix, datadir, 'nautilus'))
conf.set_quoted('NAUTILUS_EXTENSIONDIR', join_paths(prefix, extensiondir))
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PROFILE', profile)
conf.set_quoted('VERSION', version_string)

conf.set('ENABLE_PACKAGEKIT', get_option('packagekit'))
conf.set('ENABLE_PROFILING', get_option('profiling'))
conf.set('HAVE_SELINUX', get_option('selinux'))
conf.set('HAVE_CLOUDPROVIDERS', get_option('cloudproviders'))

#############################################################
# config.h dependency, add to target dependencies if needed #
#############################################################

config_h = declare_dependency(
  sources: vcs_tag(
    command: ['git', 'rev-parse', '--short', 'HEAD'],
    fallback: profile != '' ? 'devel' : '',
    input: configure_file(
      output: 'config.h.in',
      configuration: conf
    ),
    output: 'config.h'
  )
)

#####################
# End configuration #
#####################

nautilus_include_dirs = include_directories(
  '.',
  'libnautilus-extension'
)

#########
# Build #
#########

subdirs = [
  'data',
  'eel',
  'libnautilus-extension',
  'po',
  'src',
]

########################
# Conditional building #
########################

if get_option('docs')
  subdirs += 'docs'
endif
if get_option('tests') != 'none'
  subdirs += 'test'
endif
if get_option('extensions')
  subdirs += 'extensions'
endif

foreach dir : subdirs
  subdir(dir)
endforeach

#############
# End build #
#############

gnome.post_install(
  gtk_update_icon_cache: true,
  glib_compile_schemas: true,
  update_desktop_database: true,
)
