resources = gnome.compile_resources ('nautilus-ng-resources',
                                     join_paths ('res', 'org.gnome.Nautilus.gresource.xml'),
                                     source_dir: 'res')

nautilus_ng_sources = [
  resources,
  'nautilus-task.c',
  'nautilus-task.h',
  'nautilus-file.c',
  'nautilus-file.h',
  'nautilus-directory.c',
  'nautilus-directory.h',
  'nautilus-cache.c',
  'nautilus-cache.h',
  'nautilus-file-changes.c',
  'nautilus-file-changes.h',
  'nautilus-signal-utilities.c',
  'nautilus-signal-utilities.h',
  'nautilus-file-table.c',
  'nautilus-file-table.h',
  'nautilus-attribute.c',
  'nautilus-attribute.h',
  'nautilus-tasks.c',
  'nautilus-tasks.h',
  'nautilus-scheduler.c',
  'nautilus-scheduler.h',
  'nautilus-context-scheduler.c',
  'nautilus-context-scheduler.h',
  'nautilus-scheduler-private.h',
  'nautilus-main.c'
]

nautilus_ng_dependencies = [gio, glib, gnome_desktop, gtk]

nautilus_marshallers = gnome.genmarshal ('nautilus-marshallers',
                                         sources: 'nautilus-marshallers.list',
                                         prefix: 'nautilus_cclosure_marshal')

nautilus_ng_sources += nautilus_marshallers

nautilus_ng = executable ('nautilus-ng', nautilus_ng_sources,
                          dependencies: nautilus_ng_dependencies,
                          install: true)
