# Pass this in the env argument to all tests.
test_env = [
  'GSETTINGS_SCHEMA_DIR=@0@'.format(join_paths(meson.build_root(), 'data')),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir())
]

subdir('automated')
subdir('interactive')
