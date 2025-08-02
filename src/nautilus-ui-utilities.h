
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

#include <adwaita.h>
#include <gtk/gtk.h>

#include "nautilus-enums.h"


/**
 * nautilus_capitalize_str:
 * @string: input string
 *
 * Returns: a newly allocated copy of @string, with the first letter capitalized.
 * If @string is %NULL, returns %NULL.
 */
char      * nautilus_capitalize_str                 (const char    *string);

void        nautilus_gmenu_set_from_model           (GMenu             *target_menu,
                                                     GMenuModel        *source_model);
gint        nautilus_g_menu_model_find_by_string    (GMenuModel        *model,
                                                     const gchar       *attribute,
                                                     const gchar       *string);

void        nautilus_g_menu_model_set_for_mode      (GMenuModel        *model,
                                                     NautilusMode       mode);

void        nautilus_g_menu_model_set_for_view      (GMenuModel        *model,
                                                     const char        *view_name);

void        nautilus_g_menu_replace_string_in_item  (GMenu             *menu,
                                                     gint               i,
                                                     const gchar       *attribute,
                                                     const gchar       *string);

void        nautilus_ui_frame_video                 (GtkSnapshot       *snapshot,
                                                     gdouble            width,
                                                     gdouble            height);
void        nautilus_ui_draw_icon_dashed_border     (GtkSnapshot     *snapshot,
                                                     graphene_rect_t *rect,
                                                     GdkRGBA          color);
void        nautilus_ui_draw_symbolic_icon          (GtkSnapshot           *snapshot,
                                                     const gchar           *icon_name,
                                                     const graphene_rect_t *rect,
                                                     GdkRGBA                color,
                                                     int                    scale);

gboolean    nautilus_date_time_is_between_dates     (GDateTime         *date,
                                                     GDateTime         *initial_date,
                                                     GDateTime         *end_date);

AdwMessageDialog * show_dialog                      (const gchar       *primary_text,
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
