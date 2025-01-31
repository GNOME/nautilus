test_utils = files('test-utilities.c')

libtestutils = static_library(
  'testutils',
  test_utils,
  dependencies: nautilus_deps,
  include_directories: include_directories('.')
)

libtestutils_dep = declare_dependency(
  link_with: libtestutils,
  include_directories: include_directories('.'),
  dependencies: nautilus_deps,
  sources: resources
)

subdir('displayless')
if get_option('tests') == 'all'
  subdir('display')
endif
