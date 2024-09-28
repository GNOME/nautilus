# nautilus
[![Pipeline status](https://gitlab.gnome.org/GNOME/nautilus/badges/main/pipeline.svg)](https://gitlab.gnome.org/GNOME/nautilus/commits/main)

This is the project of the [Files](https://apps.gnome.org/Nautilus/) app, a file browser for
GNOME, internally known by its historical name `nautilus`.

## Supported version
Only the latest version of Files as provided upstream is supported. Try out the [Flatpak nightly](https://welcome.gnome.org/en/app/Nautilus/#installing-a-nightly-build) installation before filling issues to ensure the installation is reproducible and doesn't have downstream changes on it. In case you cannot reproduce in the nightly installation, don't hesitate to file an issue in your distribution. This is to ensure the issue is well triaged and reaches the proper people.

### Update default branch

The default development branch of nautilus has been renamed to `main`. To
update your local checkout, use:
```sh
git checkout master
git branch -m master main
git fetch
git branch --unset-upstream
git branch -u origin/main
git symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/main
```

## Runtime dependencies
- [Bubblewrap](https://github.com/projectatomic/bubblewrap) installed. Used for security reasons.
- [LocalSearch](https://gitlab.gnome.org/GNOME/localsearch) properly set up and with all features enabled. Used for fast search and metadata extraction, starred files and batch renaming.
- [xdg-user-dirs-gtk](https://gitlab.gnome.org/GNOME/xdg-user-dirs-gtk) installed.  Used to create the default bookmarks and update localization.

## Discourse

For more informal discussion we use [GNOME Discourse](https://discourse.gnome.org/tags/nautilus) in the Applications category with the `nautilus` tag. Feel free to open a topic there.

## Extensions

Documentation for the libnautilus-extension API is available [here](https://gnome.pages.gitlab.gnome.org/nautilus/).  Also, if you are interested in developing a Nautilus extension in Python you should refer to the [nautilus-python](https://gnome.pages.gitlab.gnome.org/nautilus-python/) documentation.

## How to report issues

Report issues to the GNOME [issue tracking system](https://gitlab.gnome.org/GNOME/nautilus/issues).
