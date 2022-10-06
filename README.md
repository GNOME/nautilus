# nautilus
[![Pipeline status](https://gitlab.gnome.org/GNOME/nautilus/badges/master/pipeline.svg)](https://gitlab.gnome.org/GNOME/nautilus/commits/master)

This is the project of the [Files](https://wiki.gnome.org/Apps/Files) app, a file browser for
GNOME, internally known by its historical name `nautilus`.

## Supported version
Only latest version of Files as provided upstream is supported. Try out the [Flatpak nightly](https://wiki.gnome.org/Apps/Nightly) installation before filling issues to ensure the installation is reproducible and doesn't have downstream changes on it. In case you cannot reproduce in the nightly installation, don't hesitate to file an issue in your distribution. This is to ensure the issue is well triaged and reaches the proper people.

## Hacking on nautilus

To build the development version of the Files app and hack on the code
see the [general guide](https://wiki.gnome.org/Newcomers/BuildProject)
for building GNOME apps with Flatpak and GNOME Builder.

Commit messages should follow the expected format [detailed here](https://wiki.gnome.org/Git/CommitMessages).

## Runtime dependencies
- [Bubblewrap](https://github.com/projectatomic/bubblewrap) installed. Used for security reasons.
- [Tracker (including tracker-miners)](https://gitlab.gnome.org/GNOME/tracker) properly set up and with all features enabled. Used for fast search and metadata extraction, starred files and batch renaming.

## Discourse

For more informal discussion we use [GNOME Discourse](https://discourse.gnome.org/tags/nautilus) in the Applications category with the `nautilus` tag. Feel free to open a topic there.

## How to report issues

Report issues to the GNOME [issue tracking system](https://gitlab.gnome.org/GNOME/nautilus/issues).
