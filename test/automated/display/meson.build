tests = [
  ['test-nautilus-search-engine', [
    'test-nautilus-search-engine.c'
  ]],
  ['test-nautilus-directory-async', [
    'test-nautilus-directory-async.c'
  ]],
  ['test-nautilus-file-utilities', [
    'test-nautilus-file-utilities.c']]
]

foreach t: tests
  test(t[0], executable(t[0], t[1], dependencies: libnautilus_dep))
endforeach
