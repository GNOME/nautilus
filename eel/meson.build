libeel_2_sources = [
  'eel-art-extensions.h',
  'eel-art-extensions.c',
  'eel-debug.h',
  'eel-debug.c',
  'eel-glib-extensions.h',
  'eel-graphic-effects.h',
  'eel-graphic-effects.c',
  'eel-self-checks.h',
  'eel-self-checks.c',
  'eel-stock-dialogs.h',
  'eel-stock-dialogs.c',
  'eel-string.h',
  'eel-string.c',
  'eel-vfs-extensions.h',
  'eel-vfs-extensions.c',
  'eel.h',
  'eel-lib-self-check-functions.h',
  'eel-lib-self-check-functions.c',
]

libeel_2_deps = [
  config_h,
  glib,
  gtk,
  libadwaita,
  libm
]

libeel_2 = static_library(
  'eel-2',
  libeel_2_sources,
  dependencies: libeel_2_deps,
  include_directories: nautilus_include_dirs
)

eel_2 = declare_dependency(
  link_with: libeel_2,
  include_directories: nautilus_include_dirs,
  dependencies: libeel_2_deps
)

check_eel = executable(
  'check-eel',
  'check-program.c',
  dependencies: [
    eel_2,
    xml
  ]
)

if get_option('tests') == 'all'
  test('check-eel', check_eel)
endif
