
/* nautilus-file-conflict-dialog: dialog that handles file conflicts
   during transfer operations.

   Copyright (C) 2008, Cosimo Cecchi

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
   
   Authors: Cosimo Cecchi <cosimoc@gnome.org>
*/

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FILE_CONFLICT_DIALOG (nautilus_file_conflict_dialog_get_type ())

G_DECLARE_FINAL_TYPE (NautilusFileConflictDialog, nautilus_file_conflict_dialog, NAUTILUS, FILE_CONFLICT_DIALOG, GtkDialog)

NautilusFileConflictDialog* nautilus_file_conflict_dialog_new (GtkWindow *parent);

void nautilus_file_conflict_dialog_set_text (NautilusFileConflictDialog *fcd,
                                             gchar *primary_text,
                                             gchar *secondary_text);
void nautilus_file_conflict_dialog_set_images (NautilusFileConflictDialog *fcd,
                                               GdkPaintable *source_paintable,
                                               GdkPaintable *destination_paintable);
void nautilus_file_conflict_dialog_set_file_labels (NautilusFileConflictDialog *fcd,
                                                    gchar *destination_label,
                                                    gchar *source_label);
void nautilus_file_conflict_dialog_set_conflict_name (NautilusFileConflictDialog *fcd,
                                                      gchar *conflict_name);
void nautilus_file_conflict_dialog_set_suggested_name (NautilusFileConflictDialog *fcd,
                                                       gchar *suggested_name);
void nautilus_file_conflict_dialog_set_replace_button_label (NautilusFileConflictDialog *fcd,
                                                             gchar *label);

void nautilus_file_conflict_dialog_disable_skip (NautilusFileConflictDialog *fcd);
void nautilus_file_conflict_dialog_disable_replace (NautilusFileConflictDialog *fcd);
void nautilus_file_conflict_dialog_disable_apply_to_all (NautilusFileConflictDialog *fcd);

void nautilus_file_conflict_dialog_delay_buttons_activation (NautilusFileConflictDialog *fdc);

char*      nautilus_file_conflict_dialog_get_new_name     (NautilusFileConflictDialog *dialog);
gboolean   nautilus_file_conflict_dialog_get_apply_to_all (NautilusFileConflictDialog *dialog);

G_END_DECLS
