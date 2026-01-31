blueprints = custom_target('blueprints',
  input: files(
    'ui/nautilus-app-chooser.blp',
    'ui/nautilus-batch-rename-dialog.blp',
    'ui/nautilus-column-chooser.blp',
    'ui/nautilus-compress-dialog.blp',
    'ui/nautilus-date-entry-row.blp',
    'ui/nautilus-date-range-dialog.blp',
    'ui/nautilus-file-chooser.blp',
    'ui/nautilus-file-conflict-dialog.blp',
    'ui/nautilus-file-properties-change-permissions.blp',
    'ui/nautilus-files-view-select-items.blp',
    'ui/nautilus-files-view.blp',
    'ui/nautilus-grid-cell.blp',
    'ui/nautilus-grid-view-captions-dialog.blp',
    'ui/nautilus-history-controls.blp',
    'ui/nautilus-name-cell.blp',
    'ui/nautilus-network-address-bar.blp',
    'ui/nautilus-network-cell.blp',
    'ui/nautilus-new-file-dialog.blp',
    'ui/nautilus-new-folder-dialog.blp',
    'ui/nautilus-preferences-dialog.blp',
    'ui/nautilus-operations-ui-manager-request-passphrase.blp',
    'ui/nautilus-progress-indicator.blp',
    'ui/nautilus-progress-info-widget.blp',
    'ui/nautilus-properties-window.blp',
    'ui/nautilus-rename-file-popover.blp',
    'ui/nautilus-search-popover.blp',
    'ui/nautilus-search-types-dialog.blp',
    'ui/nautilus-sidebar-row.blp',
    'ui/nautilus-templates-dialog.blp',
    'ui/nautilus-toolbar.blp',
    'ui/nautilus-view-controls.blp',
    'ui/nautilus-window.blp',
    'ui/shortcuts-dialog.blp',
  ),
  output: '.',
  command: [blueprint_cmd, 'batch-compile', '--minify', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

gresource_file = configure_file(
  input: 'nautilus.gresource.xml.in',
  output: 'nautilus.gresource.xml',
  configuration: file_conf
)

resources = gnome.compile_resources(
  'nautilus-resources',
  gresource_file,
  dependencies: [appdata, blueprints, gresource_file],
  source_dir: 'resources',
  c_name: 'nautilus',
  extra_args: '--manual-register'
)
