
/* nautilus-ui-utilities.h - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Alexander Larsson <alexl@redhat.com>
*/

#pragma once

#include <gtk/gtk.h>


void        nautilus_gmenu_set_from_model           (GMenu             *target_menu,
                                                     GMenuModel        *source_model);

void        nautilus_ui_frame_video                 (GdkPixbuf        **pixbuf);

gboolean    nautilus_file_date_in_between           (guint64            file_unix_time,
                                                     GDateTime         *initial_date,
                                                     GDateTime         *end_date);
gchar     * get_text_for_date_range                 (GPtrArray         *date_range,
                                                     gboolean           prefix_with_since);

GtkDialog * show_dialog                             (const gchar       *primary_text,
                                                     const gchar       *secondary_text,
                                                     GtkWindow         *parent,
                                                     GtkMessageType     type);

void        show_unmount_progress_cb                (GMountOperation   *op,
                                                     const gchar       *message,
                                                     gint64             time_left,
                                                     gint64             bytes_left,
                                                     gpointer           user_data);
void        show_unmount_progress_aborted_cb        (GMountOperation   *op,
                                                     gpointer           user_data);
