
/* nautilus-column-choose.h - A column chooser widget

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Dave Camp <dave@ximian.com>
*/

#pragma once

#include <gtk/gtk.h>
#include "nautilus-file.h"

#include <adwaita.h>

#define NAUTILUS_TYPE_COLUMN_CHOOSER nautilus_column_chooser_get_type()

G_DECLARE_FINAL_TYPE (NautilusColumnChooser, nautilus_column_chooser, NAUTILUS, COLUMN_CHOOSER, AdwDialog);

GtkWidget *nautilus_column_chooser_new             (NautilusFile            *file);
