test_copy = executable ('test-copy',
                        ['test-copy.c',
                         'test.c',
                         'test.h'],
                        dependencies: libnautilus_dep)

test_nautilus_search_engine = executable ('test-nautilus-search-engine',
                                          'test-nautilus-search-engine.c',
                                          dependencies: libnautilus_dep)

test_nautilus_directory_async = executable ('test-nautilus-directory-async',
                                            'test-nautilus-directory-async.c',
                                            dependencies: libnautilus_dep)

test_file_utilities_get_common_filename_prefix = executable ('test-file-utilities-get-common-filename-prefix',
                                                             'test-file-utilities-get-common-filename-prefix.c',
                                                             dependencies: libnautilus_dep)

test_eel_string_get_common_prefix = executable ('test-eel-string-get-common-prefix',
                                                'test-eel-string-get-common-prefix.c',
                                                dependencies: libnautilus_dep)

test ('test-nautilus-search-engine', test_nautilus_search_engine)
test ('test-nautilus-directory-async', test_nautilus_directory_async)
test ('test-file-utilities-get-common-filename-prefix', test_file_utilities_get_common_filename_prefix)
test ('test-eel-string-get-common-prefix', test_eel_string_get_common_prefix)
