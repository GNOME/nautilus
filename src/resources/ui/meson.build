
blueprint_names = [
  'nautilus-app-chooser',
  'nautilus-app-chooser-widget',
  'nautilus-batch-rename-dialog',
  'nautilus-column-chooser',
  'nautilus-compress-dialog',
  'nautilus-date-entry-row',
  'nautilus-date-range-dialog',
  'nautilus-file-chooser',
  'nautilus-file-conflict-dialog',
  'nautilus-file-properties-change-permissions',
  'nautilus-files-view-select-items',
  'nautilus-files-view',
  'nautilus-grid-cell',
  'nautilus-grid-view-captions-dialog',
  'nautilus-history-controls',
  'nautilus-name-cell',
  'nautilus-network-address-bar',
  'nautilus-network-cell',
  'nautilus-new-folder-dialog',
  'nautilus-preferences-dialog',
  'nautilus-operations-ui-manager-request-passphrase',
  'nautilus-progress-indicator',
  'nautilus-progress-info-widget',
  'nautilus-properties-widget',
  'nautilus-properties-window',
  'nautilus-rename-file-popover',
  'nautilus-search-popover',
  'nautilus-search-types-dialog',
  'nautilus-sidebar-row',
  'nautilus-toolbar',
  'nautilus-view-controls',
  'nautilus-window',
  'shortcuts-dialog',
]

blueprint_files = []
ui_files = []

foreach name : blueprint_names
  blueprint_files += [name + '.blp']
  ui_files += [name + '.ui']
endforeach

blueprints = custom_target('blueprints',
  input: files(blueprint_files),
  # This is the reason this file exists separately. output doesn't accept path separators
  output: ui_files,
  command: [blueprint_cmd, 'batch-compile', '--minify', meson.current_build_dir(), '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)
