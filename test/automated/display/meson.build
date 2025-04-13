tests = [
  ['test-nautilus-directory-async', [
    'test-nautilus-directory-async.c'
  ]],
]

tracker_tests = [
  ['test-files-view', [
    'test-files-view.c'
  ]],
]

foreach t: tests
  test(
    t[0],
    executable(t[0], t[1], dependencies: [libnautilus_dep, libtestutils_dep]),
    env: [
      test_env,
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir())
    ],
    timeout: 480,
    suite: ['display'],
  )
endforeach

# Tests that read and write from the Tracker index are run using 'tracker-sandbox'
# script to use a temporary instance of tracker-miner-fs instead of the session one.
foreach t: tracker_tests
  test_exe = executable(t[0], t[1], dependencies: [libnautilus_dep, libtestutils_dep])
  test(
    t[0],
    tracker_sandbox,
    args: ['test-sandbox', '--store-tmpdir', '--index-recursive-tmpdir', test_exe],
    env: [
      test_env,
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir())
    ],
    timeout: 480,
    suite: ['display', 'tracker'],
  )
endforeach
