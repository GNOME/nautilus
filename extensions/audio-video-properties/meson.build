libtotem_properties_page_sources = files(
  'totem-properties-main.c',
  'totem-properties-view.c'
)

libtotem_properties_page_deps = missing_plugins_deps + [
  gtk_dep,
  dependency('gthread-2.0'),
  libnautilus_ext_dep,
  gst_tag_dep,
  libbacon_video_widget_properties_dep
]

libtotem_properties_page = shared_module(
  'totem-properties-page',
  sources: libtotem_properties_page_sources + gen_sources,
  include_directories: top_inc,
  dependencies: libtotem_properties_page_deps,
  c_args: totem_common_cflags + [
    '-DG_LOG_DOMAIN="TotemPropertiesPage"'
  ],
  install: true,
  install_dir: nautilusdir
)

test_properties_page_sources = files(
  'totem-properties-main.c',
  'totem-properties-view.c',
  'test-properties-page.c'
)

executable(
  'test-properties-page',
  test_properties_page_sources + gen_sources,
  include_directories: top_inc,
  dependencies: libtotem_properties_page_deps + [
    libtotem_player_dep,
    libbacon_video_widget_dep
  ],
  c_args: totem_common_cflags
)

