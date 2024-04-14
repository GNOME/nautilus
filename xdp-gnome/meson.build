# Desktop Portal D-Bus interfaces
desktop_portal_dbus_interfaces = [
  'org.freedesktop.impl.portal.FileChooser',
  'org.freedesktop.impl.portal.Request',
]

desktop_portal_dbus_interfaces_files = []
foreach intf: desktop_portal_dbus_interfaces
  desktop_portal_dbus_interfaces_files += ['data' / '@0@.xml'.format(intf)]
endforeach

xdp_gnome_generated = gnome.gdbus_codegen(
  'xdg-desktop-portal-dbus',
  sources: desktop_portal_dbus_interfaces_files,
  interface_prefix: 'org.freedesktop.impl.portal.',
  namespace: 'XdpImpl',
)

shell_dbus_interfaces = [
  'data' / 'org.gnome.Mutter.ServiceChannel.xml',
]

# Mutter / GNOME Shell D-Bus interfaces
xdp_gnome_generated += gnome.gdbus_codegen(
  'shell-dbus',
  sources: shell_dbus_interfaces,
)

# Sources

xdp_gnome_deps = [
  declare_dependency(sources: xdp_gnome_generated),
  config_h,
  libm,
  glib,
  gtk,
  gio_unix,
]

xdp_gnome_sources = xdp_gnome_generated + files(
  'externalwindow.c',
  'externalwindow.h',
  'request.c',
  'request.h',
)

if gtk_x11.found()
  xdp_gnome_sources += files('externalwindow-x11.c')
  xdp_gnome_deps += [dependency('x11')]
endif

if gtk_wayland.found()
  wayland_scanner = find_program('wayland-scanner')

  wayland_protocols = [
    'mutter-x11-interop',
  ]

  foreach protocol : wayland_protocols
    client_header = custom_target('@0@ client header'.format(protocol),
      input: 'data' / '@0@.xml'.format(protocol),
      output: '@0@-client-protocol.h'.format(protocol),
      command: [
        wayland_scanner,
        'client-header',
        '@INPUT@', '@OUTPUT@',
      ]
    )
    protocol_code = custom_target('@0@ source'.format(protocol),
      input: 'data' / '@0@.xml'.format(protocol),
      output: '@0@-protocol.c'.format(protocol),
      command: [
        wayland_scanner,
        'private-code',
        '@INPUT@', '@OUTPUT@',
      ]
    )

    xdp_gnome_sources += [client_header, protocol_code]
  endforeach

  xdp_gnome_sources += files('externalwindow-wayland.c')
  xdp_gnome_deps += [
    dependency('wayland-client'),
  ]
endif

libxdp_gnome = static_library(
  'xdp-gnome',
  xdp_gnome_sources,
  dependencies: xdp_gnome_deps,
)

xdp_gnome = declare_dependency(
  link_with: libxdp_gnome,
  include_directories: nautilus_include_dirs,
  dependencies: xdp_gnome_deps
)