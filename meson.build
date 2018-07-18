project ('nautilus', 'c',
         version: '3.26.4',
         meson_version: '>=0.40.0',
         license: 'GPL3+')

add_global_arguments ('-Wno-deprecated-declarations', language:'c')

bindir = get_option ('bindir')
datadir = get_option ('datadir')
appdatadir = join_paths (datadir, 'metainfo')
desktopdir = join_paths (datadir, 'applications')
icondir = join_paths (datadir, 'icons')
includedir = get_option ('includedir')
libdir = get_option ('libdir')
extensiondir = join_paths (libdir, 'nautilus', 'extensions-3.0')
localedir = get_option ('localedir')
prefix = get_option ('prefix')
schemadir = join_paths (datadir, 'glib-2.0', 'schemas')
searchproviderdir = join_paths (datadir, 'gnome-shell', 'search-providers')
servicedir = join_paths (datadir, 'dbus-1', 'services')
sysconfdir = get_option ('sysconfdir')

glib_ver = '>=2.51.2'
gnome_desktop_ver = '>=3.0.0'
pango_ver = '1.28.3'
gtk_ver = '>=3.22.6'
xml_ver = '>=2.7.8'
exif_ver = '>=0.6.20'
exempi_ver = '>=2.1.0'
notify_ver = '0.7.0'
schemas_ver = '>=3.8.0'
autoar_ver = '>=0.2.1'
selinux_ver = '>=2.0'

cc = meson.get_compiler ('c')

gnome = import ('gnome')
i18n = import ('i18n')

glib = dependency ('glib-2.0', version: glib_ver)
gtk = dependency ('gtk+-3.0', version: gtk_ver)
autoar = dependency ('gnome-autoar-0', version: autoar_ver)

gail = dependency ('gail-3.0')
gnome_desktop = dependency ('gnome-desktop-3.0', version: gnome_desktop_ver)
xml = dependency ('libxml-2.0', version: xml_ver)
x11 = dependency ('x11')

gthread = dependency ('gthread-2.0', version: glib_ver)
gmodule_no_export = dependency ('gmodule-no-export-2.0', version: glib_ver)
gio = dependency ('gio-2.0', version: glib_ver)
gio_unix = dependency ('gio-unix-2.0', version: glib_ver)
gsettings_desktop_schemas = dependency ('gsettings-desktop-schemas',
                                        version: schemas_ver)

libm = cc.find_library ('libm')

conf = configuration_data ()
conf.set_quoted ('VERSION', meson.project_version ())
conf.set_quoted ('PACKAGE_VERSION', meson.project_version ())
conf.set_quoted ('GETTEXT_PACKAGE', 'nautilus')
conf.set_quoted ('LOCALEDIR', join_paths (prefix, localedir))
conf.set_quoted ('NAUTILUS_DATADIR', join_paths (datadir, 'nautilus'))
conf.set_quoted ('NAUTILUS_EXTENSIONDIR', join_paths (prefix, extensiondir))

if get_option ('enable-desktop')
    conf.set10 ('ENABLE_DESKTOP', true)
endif

if get_option ('enable-exif')
    exif = dependency ('libexif', version: exif_ver)
    conf.set10 ('HAVE_EXIF', true)
endif

if get_option ('enable-packagekit')
    conf.set10 ('ENABLE_PACKAGEKIT', true)
endif

if get_option ('enable-selinux')
    selinux = dependency ('libselinux', version: selinux_ver)
    conf.set10 ('HAVE_SELINUX', true)
endif

tracker_sparql = dependency ('tracker-sparql-2.0', required: false)
if not tracker_sparql.found()
    tracker_sparql = dependency ('tracker-sparql-1.0')
endif

if get_option ('enable-xmp')
    exempi = dependency ('exempi-2.0', version: exempi_ver)
    conf.set10 ('HAVE_EXEMPI', true)
endif

configure_file (input: 'config.h.meson',
                output: 'config.h',
                configuration: conf)

nautilus_include_dirs = include_directories ('.')

subdir ('eel')
subdir ('libnautilus-extension')

libgd_options = ['static=true',
                 'with-gtk-hacks=true',
                 'with-main-view=true',
                 'with-tagged-entry=true']
libgd = subproject('libgd', default_options: libgd_options)
libgd_dep = libgd.get_variable ('libgd_dep')

subdir ('src')
subdir ('test')
subdir ('data')
subdir ('po')

if get_option ('enable-gtk-doc')
    subdir ('docs')
endif

if get_option ('enable-desktop')
    subdir ('nautilus-desktop')
endif

if get_option ('enable-nst-extension')
    subdir ('nautilus-sendto-extension')
endif

# Compile GSettings schemas when installing from source.
meson.add_install_script ('build-aux/meson/postinstall.py')
