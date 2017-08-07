resources = gnome.compile_resources ('nautilus-ng-resources',
                                     join_paths ('res', 'org.gnome.Nautilus.gresource.xml'),
                                     source_dir: 'res')

nautilus_ng_sources = [resources,
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

nautilus_ng_dependencies = [gio, glib, gnome_desktop, gtk]

nautilus_marshallers = gnome.genmarshal ('nautilus-marshallers',
                                         sources: 'nautilus-marshallers.list',
                                         prefix: 'nautilus_cclosure_marshal')

nautilus_ng_sources += nautilus_marshallers

nautilus_ng = executable ('nautilus-ng', nautilus_ng_sources,
                          dependencies: nautilus_ng_dependencies,
                          install: true)
