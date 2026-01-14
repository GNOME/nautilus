#!/usr/bin/env bash

menu_dirs="src/resources/menu"
srcdirs="eel extensions src libnautilus-extension"
desktopdirs="data"

# Blueprint and C source files that contain gettext keywords
files=$(grep -lR --include='*.blp' --include='*.c' '\(gettext\|[^I_)]_\)(' $srcdirs)

# Menu files
files="$files "$(grep -lRi --include='*.ui' 'translatable="[ty1]' $menu_dirs)

# find .desktop files
files="$files "$(find $desktopdirs -name '*.desktop*')

# filter out excluded files
if [ -f po/POTFILES.skip ]; then
  files=$(for f in $files; do ! grep -q ^$f po/POTFILES.skip && echo $f; done)
fi

# find those that aren't listed in POTFILES.in
missing=$(for f in $files; do ! grep -q ^$f po/POTFILES.in && echo $f; done)

if [ ${#missing} -eq 0 ]; then
  exit 0
fi

cat >&2 <<EOT

The following files are missing from po/POTFILES.in:

EOT
for f in $missing; do
  echo "  $f" >&2
done
echo >&2

exit 1
