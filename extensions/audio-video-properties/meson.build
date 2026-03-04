libm = cc.find_library('m')

shared_module (
  'nautilus-image-properties',
  sources: [
    'audio-video-properties-module.c',
    'audio-video-properties-model.c',
    'audio-video-properties-model.h',
  ],
  dependencies: [
    libm,
    gst_tag_dep,
    gst_pbutils_dep,
    nautilus_extension
  ],
  c_args: [
    '-DG_LOG_DOMAIN="av-properties"'
  ],
  install: true,
  install_dir: extensiondir
)
