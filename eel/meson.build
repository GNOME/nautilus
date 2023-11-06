libeel_2_sources = [
  'eel-stock-dialogs.h',
  'eel-stock-dialogs.c',
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
