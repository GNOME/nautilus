# When adding new tests, it is useful to set some environment variables.
# You can do it like so:
#
# test(
#   name,
#   executable,
#   env: [
#     test_env,
#     'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
#     'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir())
#   ]
# )
#
# G_TEST_BUILDDIR and G_TEST_SRCDIR cannot be preset, since
# the test sources are scattered.
test_env = [
  'GSETTINGS_SCHEMA_DIR=@0@'.format(join_paths(meson.build_root(), 'data')),
  'RUNNING_TESTS=@0@'.format('TRUE')
]

subdir('automated')
#subdir('interactive')
