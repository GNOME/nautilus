docs_conf = configuration_data ()
docs_conf.set ('NAUTILUS_EXTENSION_VERSION_INFO', nautilus_extension_version)

docs_version = configure_file (input: 'version.xml.in',
                               output: 'version.xml',
                               configuration: docs_conf)

gnome.gtkdoc ('libnautilus-extension',
              main_xml: 'libnautilus-extension-docs.xml',
              src_dir: join_paths (meson.source_root (), 'libnautilus-extension'),
              dependencies: nautilus_extension,
              gobject_typesfile: 'libnautilus-extension.types',
              install: true,
              scan_args: ['--ignore-headers=nautilus-extension-private.h',
                          '--rebuild-types'])
