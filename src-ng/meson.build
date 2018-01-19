resources = gnome.compile_resources ('nautilus-ng-resources',
                                     join_paths ('res', 'org.gnome.Nautilus.gresource.xml'),
                                     source_dir: 'res')

libnautilus_ng_sources = [resources,
                       'nautilus-task.c',
                       'nautilus-task.h',
                       'nautilus-task-private.h',
                       'nautilus-task-manager.c',
                       'nautilus-task-manager.h',
                       'nautilus-file.c',
                       'nautilus-file.h',
                       'tasks/nautilus-attribute-task.c',
                       'tasks/nautilus-attribute-task.h',
                       'nautilus-directory.c',
                       'nautilus-directory.h',
                       'tasks/nautilus-enumerate-children-task.c',
                       'tasks/nautilus-enumerate-children-task.h',
                       'nautilus-cache.c',
                       'nautilus-cache.h',
                       'tasks/nautilus-rename-task.c',
                       'tasks/nautilus-rename-task.h',
                       'nautilus-file-changes.c',
                       'nautilus-file-changes.h',
                       'nautilus-signal-utilities.c',
                       'nautilus-signal-utilities.h',
                       'nautilus-file-table.c',
                       'nautilus-file-table.h',
                       'tasks/nautilus-thumbnail-task.c',
                       'tasks/nautilus-thumbnail-task.h',
                       'main.c']

libnautilus_ng_deps = [gio, glib, gnome_desktop, gtk]

libnautilus_ng_marshallers = gnome.genmarshal ('nautilus-marshallers',
                                               sources: 'nautilus-marshallers.list',
                                               prefix: 'nautilus_cclosure_marshal')

libnautilus_ng_sources += libnautilus_ng_marshallers

libnautilus_ng_include_dirs = include_directories('.')

libnautilus_ng = static_library(
  'nautilus-ng',
  libnautilus_ng_sources,
  dependencies: libnautilus_ng_deps,
  include_directories: libnautilus_ng_include_dirs
)

libnautilus_ng_dep = declare_dependency(
  link_with: libnautilus_ng,
  include_directories: [
    libnautilus_ng_include_dirs
  ],
  dependencies: libnautilus_ng_deps
)

nautilus = executable(
  'nautilus-ng',
  'main.c',
  dependencies: libnautilus_ng_dep,
  install: true
)
