resources = gnome.compile_resources(
  'nautilus-audio-video-properties-resources',
  join_paths(
    'resources', 'nautilus-audio-video-properties.gresource.xml'
  ),
  source_dir: 'resources',
  c_name: 'nautilus_audio_video_properties',
)

libtotem_properties_page_sources = files(
  'totem-properties-main.c',
  'totem-properties-view.c',
  'bacon-video-widget-properties.c',
) + resources

libtotem_properties_page_deps = [
  gst_tag_dep,
  gst_pbutils_dep,
]

libtotem_properties_page = shared_module(
  'totem-properties-page',
  sources: libtotem_properties_page_sources,
  dependencies: libtotem_properties_page_deps + [
    nautilus_extension
  ],
  c_args: [
    '-DG_LOG_DOMAIN="TotemPropertiesPage"'
  ],
  install: true,
  install_dir: extensiondir
)

test_properties_page_sources = files(
  'totem-properties-main.c',
  'totem-properties-view.c',
  'bacon-video-widget-properties.c',
  'test-properties-page.c'
) + resources

executable(
  'test-properties-page',
  test_properties_page_sources,
  dependencies: libtotem_properties_page_deps + [
    nautilus_extension
  ],
)

