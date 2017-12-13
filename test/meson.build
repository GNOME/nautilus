test_copy = executable(
  'test-copy', [
    'test-copy.c',
    'test.c',
    'test.h'
  ],
  dependencies: libnautilus_dep
)

tests = [
  ['test-nautilus-search-engine', [
    'test-nautilus-search-engine.c'
  ]],
  ['test-nautilus-directory-async', [
    'test-nautilus-directory-async.c'
  ]],
  ['test-file-utilities-get-common-filename-prefix', [
    'test-file-utilities-get-common-filename-prefix.c'
  ]],
  ['test-eel-string-rtrim-punctuation', [
    'test-eel-string-rtrim-punctuation.c'
  ]],
  ['test-eel-string-get-common-prefix', [
    'test-eel-string-get-common-prefix.c'
  ]]
]

foreach t: tests
  test(t[0], executable(t[0], t[1], dependencies: libnautilus_dep))
endforeach
