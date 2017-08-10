subdir ('icons')

po_dir = join_paths (meson.source_root (), 'po')

line_up_parameters = executable ('lineup-parameters',
                                 'lineup-parameters.c',
                                 dependencies: [gio, gio_unix])

desktop = i18n.merge_file ('desktop',
                           input: 'org.gnome.Nautilus.desktop.in',
                           output: 'org.gnome.Nautilus.desktop',
                           install: true,
                           install_dir: desktopdir,
                           po_dir: po_dir,
                           type: 'desktop')

desktop_autorun_software = i18n.merge_file ('desktop-autorun-software',
                                            input: 'nautilus-autorun-software.desktop.in',
                                            output: 'nautilus-autorun-software.desktop',
                                            install: true,
                                            install_dir: desktopdir,
                                            po_dir: po_dir,
                                            type: 'desktop')

desktop_autostart = i18n.merge_file ('desktop-autostart',
                                     input: 'nautilus-autostart.desktop.in',
                                     output: 'nautilus-autostart.desktop',
                                     install: true,
                                     install_dir: join_paths (sysconfdir, 'xdg', 'autostart'),
                                     po_dir: po_dir,
                                     type: 'desktop')

if get_option ('enable-desktop')
    desktop_classic = i18n.merge_file ('desktop-classic',
                                       input: 'nautilus-classic.desktop.in',
                                       output: 'nautilus-classic.desktop',
                                       install: true,
                                       install_dir: desktopdir,
                                       po_dir: po_dir,
                                       type: 'desktop')
endif

appdata = i18n.merge_file ('appdata',
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

desktop_file_validate = find_program ('desktop-file-validate', required: false)
if desktop_file_validate.found ()
    test ('validate-desktop', desktop_file_validate,
          args: [desktop.full_path ()])
    test ('validate-desktop-autorun-software', desktop_file_validate,
          args: [desktop_autorun_software.full_path ()])
    test ('validate-desktop-autostart', desktop_file_validate,
          args: [desktop_autostart.full_path ()])
    if get_option ('enable-desktop')
        test ('validate-desktop-classic', desktop_file_validate,
          args: [desktop_classic.full_path ()])
    endif
endif

appstream_util = find_program ('appstream-util', required: false)
if appstream_util.found ()
    test ('validate-appdata', appstream_util,
          args: ['validate-relax', appdata.full_path ()])
endif
