#!/bin/sh
#######################################################################
package=nautilus
#######################################################################
echo "Building the $package.pot ..." 
xgettext --default-domain=$package --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f $package.po \
   || ( rm -f ./$package.pot \
    && mv $package.po ./$package.pot )
#######################################################################
