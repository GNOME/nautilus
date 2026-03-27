test_env = [
  'GTK_A11Y=none',
  'GSK_DEBUG=fallback',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(join_paths(meson.project_build_root(), 'data')),
]

subdir('util')

subdir('displayless')

if get_option('tests') == 'all'
  subdir('display')
endif
