project('nautilus', 'c',
  default_options: ['c_std=c11'],
  version: '3.30.1',
  meson_version: '>= 0.47.0',
  license: 'GPL3+'
)

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
extensiondir = join_paths(libdir, 'nautilus', 'extensions-3.0')
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
  ]),
  '-D_GNU_SOURCE',
  language: 'c'
)

######################
# Host system checks #
######################

host_system = host_machine.system()
host_cpu = host_machine.cpu()
unsupported_cpus = [
  'alpha',
  'ia64',
  'm68k',
  'parisc',
  'parisc64',
  'sh4',
  'sparc64',
]
system_supports_seccomp = host_system == 'linux'
cpu_supports_seccomp = not unsupported_cpus.contains(host_cpu)
seccomp_required = system_supports_seccomp and cpu_supports_seccomp

##########################
# End host system checks #
##########################

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

if get_option('extensions')
  gexiv = dependency('gexiv2', version: '>= 0.10.0')
endif
gio = dependency('gio-2.0', version: glib_ver)
gio_unix = dependency('gio-unix-2.0', version: glib_ver)
glib = dependency('glib-2.0', version: glib_ver)
gmodule = dependency('gmodule-no-export-2.0', version: glib_ver)
gnome_autoar = dependency('gnome-autoar-0', version: '>= 0.2.1')
gsettings_desktop_schemas = dependency('gsettings-desktop-schemas')
gtk = dependency('gtk+-3.0', version: '>= 3.22.27')
if seccomp_required
  message('seccomp required on this platform, make sure bubblewrap is available at runtime as well.')
  seccomp = dependency('libseccomp')
else
  warning('The host does not support seccomp, thumbnailer sandboxing will be disabled. Such setups are not recommended, use at your own risk!')
  seccomp = dependency('', required: false)
endif
selinux = []
if get_option('selinux')
  selinux = dependency('libselinux', version: '>= 2.0')
endif
tracker_sparql = dependency('tracker-sparql-2.0')
x11 = dependency('x11')
xml = dependency('libxml-2.0', version: '>= 2.7.8')

####################
# End dependencies #
####################

#################
# Configuration #
#################
conf = configuration_data()

if get_option('profile') == 'development'
  profile = 'Devel'
  name_suffix = ' (Development Snapshot)'
elif get_option('profile') != ''
  profile = get_option('profile')
  name_suffix = get_option('profile')
else
  profile = ''
  name_suffix = ''
endif

application_id = 'org.gnome.Nautilus' + profile

conf.set_quoted('APPLICATION_ID', application_id)
conf.set_quoted('GETTEXT_PACKAGE', 'nautilus')
conf.set_quoted('INSTALL_PREFIX', prefix)
conf.set_quoted('LOCALEDIR', join_paths(prefix, localedir))
conf.set_quoted('NAME_SUFFIX', name_suffix)
conf.set_quoted('NAUTILUS_DATADIR', join_paths(datadir, 'nautilus'))
conf.set_quoted('NAUTILUS_EXTENSIONDIR', join_paths(prefix, extensiondir))
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PROFILE', profile)
conf.set_quoted('VERSION', '@0@-@VCS_TAG@'.format(meson.project_version()))

###################################################
# gnome-desktop macros for thumbnailer sandboxing #
###################################################

conf.set('ENABLE_SECCOMP', seccomp.found())
conf.set('HAVE_BWRAP', seccomp.found())

conf.set('ENABLE_PACKAGEKIT', get_option('packagekit'))
conf.set('ENABLE_PROFILING', get_option('profiling'))
conf.set('HAVE_SELINUX', get_option('selinux'))

#############################################################
# config.h dependency, add to target dependencies if needed #
#############################################################

config_h = declare_dependency(
  sources: vcs_tag(
    command: ['git', 'rev-parse', '--short', 'HEAD'],
    fallback: get_option('profile') != ''? 'devel' : 'stable',
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

#########################################################
# Compile GSettings schemas when installing from source #
#########################################################
meson.add_install_script('build-aux/meson/postinstall.py')
