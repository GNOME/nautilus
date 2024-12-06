# Files needed for running Tracker inside the Flatpak sandbox, for systems
# which don't have a suitable version of Tracker in the host OS.
#
# We must export the .service files from the sandbox so they work on the
# session bus. This means the Tracker domain name must correspond with the
# application ID.

domain_ontologies_dir = get_option('datadir') / 'tracker3' / 'domain-ontologies'
dbus_services_dir = get_option('datadir') / 'dbus-1' / 'services'

tracker_domain_config = configuration_data()
tracker_domain_config.set('application_id', application_id)
tracker_domain_config.set('domain_rule', get_option('prefix') / domain_ontologies_dir / application_id + '.domain.rule')

configure_file(
  input: 'org.gnome.Nautilus.domain.rule.in',
  output: application_id + '.domain.rule',
  configuration: tracker_domain_config,
  install_dir: domain_ontologies_dir)

configure_file(
  input: 'org.gnome.Nautilus.Tracker3.Miner.Extract.service.in',
  output: application_id + '.Tracker3.Miner.Extract.service',
  configuration: tracker_domain_config,
  install_dir: dbus_services_dir)

configure_file(
  input: 'org.gnome.Nautilus.Tracker3.Miner.Files.service.in',
  output: application_id + '.Tracker3.Miner.Files.service',
  configuration: tracker_domain_config,
  install_dir: dbus_services_dir)
