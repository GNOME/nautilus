test_utils = files('test-utilities.c')

libtestutils = static_library(
  'testutils',
  test_utils,
  dependencies: libnautilus_dep,
  include_directories: include_directories('.')
)

libtestutils_dep = declare_dependency(
  link_with: libtestutils,
  include_directories: include_directories('.'),
  dependencies: libnautilus_dep,
  sources: resources
)

test_env += [
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'XDG_CONFIG_HOME=@0@'.format(meson.current_build_dir()),
]

subdir('displayless')
if get_option('tests') == 'all'
  subdir('display')
endif
