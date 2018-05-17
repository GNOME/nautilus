tests = [
  ['test-file-utilities-get-common-filename-prefix', [
    'test-file-utilities-get-common-filename-prefix.c'
  ]],
  ['test-eel-string-get-common-prefix', [
    'test-eel-string-get-common-prefix.c'
  ]],
  ['test-file-utilities', [
    'test-file-utilities.c'
  ]]
]

foreach t: tests
  test(
    t[0],
    executable(t[0], t[1], dependencies: libnautilus_dep),
    env: test_env
  )
endforeach
