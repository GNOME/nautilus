#!/bin/bash
#
#  Script for translators not being able to run autogen.sh
#
#  Copyright (C) 2000 Free Software Foundation.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This script is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this library; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Authors: Kenneth Christiansen <kenneth@gnu.org>

PACKAGE=nautilus

DEPENDS=$(which xml-i18n-toolize 2> /dev/null)

if [ "$DEPENDS" = "" -o ! -x "$DEPENDS" ] ;
   then echo "The xml-i18n-tools system is not installed or in path!"
        echo
        echo "The module $PACKAGE requires this inplimentation, which"
        echo "can be found at:"
        echo
        echo "  ftp://ftp.gnome.org/pub/GNOME/stable/xml-i18n-tools "
        echo
        echo "Please install before trying to update the translations"
        echo "again..."
        echo
        exit
fi

XMLDIR=$(which xml-i18n-toolize | sed s@/bin/xml-i18n-toolize@@)
XML_I18N_EXTRACT="$XMLDIR/share/xml-i18n-tools/xml-i18n-extract"
XML_I18N_UPDATE="$XMLDIR/share/xml-i18n-tools/xml-i18n-update"
PACKAGE=$PACKAGE XML_I18N_EXTRACT=$XML_I18N_EXTRACT \
$XML_I18N_UPDATE $1
