subdir ('icons')

po_dir = join_paths (meson.source_root (), 'po')

line_up_parameters = executable ('lineup-parameters',
                                 'lineup-parameters.c',
                                 dependencies: [gio, gio_unix])

i18n.merge_file ('desktop',
                 input: 'org.gnome.Nautilus.desktop.in',
                 output: 'org.gnome.Nautilus.desktop',
                 install: true,
                 install_dir: desktopdir,
                 po_dir: po_dir,
                 type: 'desktop')

i18n.merge_file ('desktop-autorun-software',
                 input: 'nautilus-autorun-software.desktop.in',
                 output: 'nautilus-autorun-software.desktop',
                 install: true,
                 install_dir: desktopdir,
                 po_dir: po_dir,
                 type: 'desktop')

i18n.merge_file ('desktop-autostart',
                 input: 'nautilus-autostart.desktop.in',
                 output: 'nautilus-autostart.desktop',
                 install: true,
                 install_dir: join_paths (sysconfdir, 'xdg', 'autostart'),
                 po_dir: po_dir,
                 type: 'desktop')

if get_option ('enable-desktop')
    i18n.merge_file ('desktop',
                     input: 'nautilus-classic.desktop.in',
                     output: 'nautilus-classic.desktop',
                     install: true,
                     install_dir: desktopdir,
                     po_dir: po_dir,
                     type: 'desktop')
endif

i18n.merge_file ('desktop',
                 input: 'org.gnome.Nautilus.appdata.xml.in',
                 output: 'org.gnome.Nautilus.appdata.xml',
                 install: true,
                 install_dir: appdatadir,
                 po_dir: po_dir,)

service_conf = configuration_data ()
service_conf.set ('bindir', join_paths (prefix, bindir))

configure_file (input: 'org.gnome.Nautilus.service.in',
                output: 'org.gnome.Nautilus.service',
                configuration: service_conf,
                install_dir: servicedir)

configure_file (input: 'org.freedesktop.FileManager1.service.in',
                output: 'org.freedesktop.FileManager1.service',
                configuration: service_conf,
                install_dir: servicedir)

install_data ('nautilus-search-provider.ini',
              install_dir: searchproviderdir)

install_data ('org.gnome.nautilus.gschema.xml',
              install_dir: schemadir)
