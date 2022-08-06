libm = cc.find_library('m')

libtotem_properties_page_sources = files(
  'totem-properties-main.c',
  'totem-properties-view.c',
  'totem-gst-helpers.c',
)

libtotem_properties_page_deps = [
  libm,
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

# test_properties_page_sources = files(
#   'totem-properties-main.c',
#   'totem-properties-view.c',
#   'totem-gst-helpers.c',
#   'test-properties-page.c'
# )

# executable(
#   'test-properties-page',
#   test_properties_page_sources,
#   dependencies: libtotem_properties_page_deps + [
#     nautilus_extension
#   ],
# )

