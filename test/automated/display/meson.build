tests = {
  'test-nautilus-directory-async': {},
  'test-files-view': {
    'suite': ['tracker'],
    'tracker': true,
    'is_parallel' : false,
  },
}

foreach test_name, extra_args : tests
  source = test_name + '.c'
  suite = ['display'] + extra_args.get('suite', [])
  test_exe = executable(test_name, source, dependencies: [libnautilus_dep, libtestutils_dep])
  args = []
  exe = []
  is_parallel = extra_args.get('is_parallel', true)

  if extra_args.get('tracker', false)
    # Tests that read and write from the Tracker index are run using 'tracker-sandbox'
    # script to use a temporary instance of tracker-miner-fs instead of the session one.
    exe = tracker_sandbox
    args = ['test-sandbox', '--store-tmpdir', '--index-recursive-tmpdir', test_exe]
  else
    exe = test_exe
  endif

  test(
    test_name,
    exe,
    args: args,
    env: [
      test_env,
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir())
    ],
    is_parallel: is_parallel,
    timeout: 480,
    suite: suite,
  )
endforeach
