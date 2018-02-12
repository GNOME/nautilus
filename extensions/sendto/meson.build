libnautilus_sendto_sources = [
  'nautilus-nste.c',
  'nautilus-nste.h',
  'nautilus-sendto-module.c'
]

libnautilus_sendto = shared_module(
  'nautilus-sendto',
  libnautilus_sendto_sources,
  dependencies: nautilus_extension,
  install: true,
  install_dir: extensiondir
)
