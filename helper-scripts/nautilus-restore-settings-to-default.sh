#!/bin/sh

nautilus --quit
gconftool --shutdown

rm -rf $HOME/.nautilus
rm -rf $HOME/.gconf
rm -rf $HOME/.gconfd