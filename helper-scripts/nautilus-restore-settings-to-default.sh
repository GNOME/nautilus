#!/bin/sh

nautilus --quit
gconftool --shutdown

nautilus_dir=$HOME/.nautilus
fonts_tmp=$HOME/nautilus-fonts-$$

if [ -d $nautilus_dir/fonts ]
then
    mv $nautilus_dir/fonts $fonts_tmp
fi

rm -rf $nautilus_dir/
rm -rf $HOME/.gconf/apps/nautilus/
rm -rf $HOME/.gconfd

if [ -d $fonts_tmp ]
then
    mkdir -p $nautilus_dir/
    mv $fonts_tmp $nautilus_dir/fonts
fi

