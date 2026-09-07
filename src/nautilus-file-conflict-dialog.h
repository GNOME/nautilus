
/*
 * nautilus-file-conflict-dialog: dialog that handles file conflicts
 * during transfer operations.
 *
 * SPDX-FileCopyrightText: 2008, Cosimo Cecchi
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#include <adwaita.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <nautilus-file.h>

G_BEGIN_DECLS

typedef enum
{
    CONFLICT_RESPONSE_CANCEL,
    CONFLICT_RESPONSE_SKIP,
    CONFLICT_RESPONSE_REPLACE,
    CONFLICT_RESPONSE_RENAME,
} ConflictResponse;

#define NAUTILUS_TYPE_FILE_CONFLICT_DIALOG (nautilus_file_conflict_dialog_get_type ())

G_DECLARE_FINAL_TYPE (NautilusFileConflictDialog, nautilus_file_conflict_dialog, NAUTILUS, FILE_CONFLICT_DIALOG, AdwWindow)

NautilusFileConflictDialog* nautilus_file_conflict_dialog_new (GtkWindow *parent);

void nautilus_file_conflict_dialog_set_text (NautilusFileConflictDialog *fcd,
                                             gchar *primary_text,
                                             gchar *secondary_text);
void nautilus_file_conflict_dialog_set_images (NautilusFileConflictDialog *fcd,
                                               NautilusFile               *destination_file,
                                               NautilusFile               *source_file);
void nautilus_file_conflict_dialog_set_file_labels (NautilusFileConflictDialog *fcd,
                                                    gchar *destination_label,
                                                    gchar *source_label);
void nautilus_file_conflict_dialog_set_conflict_name (NautilusFileConflictDialog *fcd,
                                                      const char *conflict_name);
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

ConflictResponse nautilus_file_conflict_dialog_get_response (NautilusFileConflictDialog *dialog);

G_END_DECLS
