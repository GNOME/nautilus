test_env = [
  'GTK_A11Y=none',
  'GSK_DEBUG=fallback',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(join_paths(meson.project_build_root(), 'data')),
  'LSAN_OPTIONS=suppressions=@0@'.format(
    meson.project_source_root() / 'build-aux' / 'leak-suppress.txt'
  ),
]

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
  sources: resources_header
)

subdir('displayless')

if get_option('tests') == 'all'
  subdir('display')
endif
