tests = [
  ['test-file-utilities-get-common-filename-prefix', [
    'test-file-utilities-get-common-filename-prefix.c'
  ]],
  ['test-file-attributes-cache', [
    'test-file-attributes-cache.c'
  ]],
  ['test-eel-string-get-common-prefix', [
    'test-eel-string-get-common-prefix.c'
  ]]
]

install_data (['test_file_01', 'test_file_02'],
               install_dir: '/tmp')
foreach t: tests
  test(t[0], executable(t[0], t[1],
                        dependencies: [libnautilus_dep, libnautilus_ng_dep]),
       workdir: '/tmp')
endforeach
