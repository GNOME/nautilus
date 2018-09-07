tests = [
  ['test-file-utilities-get-common-filename-prefix', [
    'test-file-utilities-get-common-filename-prefix.c'
  ]],
  ['test-eel-string-get-common-prefix', [
    'test-eel-string-get-common-prefix.c'
  ]],
  ['test-file-utilities', [
    'test-file-utilities.c'
  ]],
  ['test-file-operations-dir-has-files', [
    'test-file-operations-dir-has-files.c'
  ]],
  ['test-file-operations-move-files', [
    'test-file-operations-move-files.c'
  ]],
  ['test-nautilus-search-engine', [
    'test-nautilus-search-engine.c'
  ]],
  ['test-nautilus-search-engine-simple', [
    'test-nautilus-search-engine-simple.c'
  ]],
  ['test-nautilus-search-engine-model', [
    'test-nautilus-search-engine-model.c'
  ]],
  ['test-nautilus-search-engine-tracker', [
    'test-nautilus-search-engine-tracker.c'
  ]],
  ['test-file-operations-copy-files', [
    'test-file-operations-copy-files.c'
  ]],
  ['test-file-operations-trash-or-delete', [
    'test-file-operations-trash-or-delete.c'
  ]]
]

foreach t: tests
  test(
    t[0],
    executable(t[0], t[1], files('test-utilities.c'), dependencies: libnautilus_dep),
    env: [
      test_env,
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir())
    ],
    timeout: 120
  )
endforeach
