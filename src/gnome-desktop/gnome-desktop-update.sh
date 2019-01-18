#!/bin/sh
#
# gnome-desktop-update.sh
#
# Fetch the latest master gnome-desktop-thumbnail source code from the
# gnome-desktop project https://gitlab.gnome.org/GNOME/gnome-desktop
#
# The Nautilus gnome-desktop thumbnail code directly tracks the gnome-desktop
# repository. There is typically no need to modify the received files.
#
# Usage:
#
# Execute the script within the nautilus/src/gnome-desktop directory.
# For example:
#
# $ cd src/gnome-desktop
# $ ./gnome-desktop-update.sh
#

URL=https://gitlab.gnome.org/GNOME/gnome-desktop/raw/master/libgnome-desktop/
FILES=(
    "gnome-desktop-thumbnail.c"
    "gnome-desktop-thumbnail.h"
    "gnome-desktop-thumbnail-script.c"
    "gnome-desktop-thumbnail-script.h"
)
r=0

for f in ${FILES[@]}; do
  echo "GET: $URL$f"
  if curl -sfO $URL$f; then
    echo " OK: $f"
  else
    echo "ERR: $f download error."
    r=1
  fi;
done

if [ $r -eq 0 ]; then
  echo "SUCCESS: All updates completed successfully."
else
  echo "WARNING: One or more updates encountered an error."
fi;

exit $r