/* nautilus-file-conflict-dialog: dialog that handles file conflicts
 *  during transfer operations.
 *
 *  Copyright (C) 2008-2010 Cosimo Cecchi
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <config.h>
#include "nautilus-file-conflict-dialog.h"

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <pango/pango.h>
#include <eel/eel-vfs-extensions.h>

#include "nautilus-file.h"
#include "nautilus-icon-info.h"
#include "nautilus-operations-ui-manager.h"

struct _NautilusFileConflictDialog
{
    GtkDialog parent_instance;

    gchar *conflict_name;
    gchar *suggested_name;

    /* UI objects */
    GtkWidget *primary_label;
    GtkWidget *secondary_label;
    GtkWidget *dest_label;
    GtkWidget *src_label;
    GtkWidget *expander;
    GtkWidget *entry;
    GtkWidget *checkbox;
    GtkWidget *cancel_button;
    GtkWidget *skip_button;
    GtkWidget *rename_button;
    GtkWidget *replace_button;
    GtkWidget *dest_image;
    GtkWidget *src_image;
};

G_DEFINE_TYPE (NautilusFileConflictDialog, nautilus_file_conflict_dialog, GTK_TYPE_DIALOG);

void
nautilus_file_conflict_dialog_set_text (NautilusFileConflictDialog *fcd,
                                        gchar                      *primary_text,
                                        gchar                      *secondary_text)
{
    gtk_label_set_text (GTK_LABEL (fcd->primary_label), primary_text);
    gtk_label_set_text (GTK_LABEL (fcd->secondary_label), secondary_text);
}

void
nautilus_file_conflict_dialog_set_images (NautilusFileConflictDialog *fcd,
                                          GdkPixbuf                  *destination_pixbuf,
                                          GdkPixbuf                  *source_pixbuf)
{
    gtk_image_set_from_pixbuf (GTK_IMAGE (fcd->dest_image), destination_pixbuf);
    gtk_image_set_from_pixbuf (GTK_IMAGE (fcd->src_image), source_pixbuf);
}

void
nautilus_file_conflict_dialog_set_file_labels (NautilusFileConflictDialog *fcd,
                                               gchar                      *destination_label,
                                               gchar                      *source_label)
{
    gtk_label_set_markup (GTK_LABEL (fcd->dest_label), destination_label);
    gtk_label_set_markup (GTK_LABEL (fcd->src_label), source_label);
}

void
nautilus_file_conflict_dialog_set_conflict_name (NautilusFileConflictDialog *fcd,
                                                 gchar                      *conflict_name)
{
    fcd->conflict_name = g_strdup (conflict_name);
}

void
nautilus_file_conflict_dialog_set_suggested_name (NautilusFileConflictDialog *fcd,
                                                  gchar                      *suggested_name)
{
    fcd->suggested_name = g_strdup (suggested_name);
    gtk_entry_set_text (GTK_ENTRY (fcd->entry), suggested_name);
}

void
nautilus_file_conflict_dialog_set_replace_button_label (NautilusFileConflictDialog *fcd,
                                                        gchar                      *label)
{
    gtk_button_set_label (GTK_BUTTON (fcd->replace_button), label);
}

void
nautilus_file_conflict_dialog_disable_skip (NautilusFileConflictDialog *fcd)
{
    gtk_widget_hide (fcd->skip_button);
}

void
nautilus_file_conflict_dialog_disable_replace (NautilusFileConflictDialog *fcd)
{
    gtk_widget_set_sensitive (fcd->replace_button, FALSE);
}

void
nautilus_file_conflict_dialog_disable_apply_to_all (NautilusFileConflictDialog *fcd)
{
    gtk_widget_hide (fcd->checkbox);
}

static void
entry_text_changed_cb (GtkEditable                *entry,
                       NautilusFileConflictDialog *dialog)
{
    if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (entry)), "") != 0 &&
        g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (entry)), dialog->conflict_name) != 0)
    {
        gtk_widget_set_sensitive (dialog->rename_button, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive (dialog->rename_button, FALSE);
    }
}

static int
get_character_position_after_basename (const gchar *filename)
{
    const gchar *extension;

    extension = eel_filename_get_extension_offset (filename);

    if (extension == NULL)
    {
        /* If the filename has got no extension, we want the position of the
         * the terminating null. */
        return (int) g_utf8_strlen (filename, -1);
    }

    return g_utf8_pointer_to_offset (filename, extension);
}

static void
on_expanded_notify (GtkExpander                *w,
                    GParamSpec                 *pspec,
                    NautilusFileConflictDialog *dialog)
{
    if (gtk_expander_get_expanded (w))
    {
        gtk_widget_hide (dialog->replace_button);
        gtk_widget_show (dialog->rename_button);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), CONFLICT_RESPONSE_RENAME);

        gtk_widget_set_sensitive (dialog->checkbox, FALSE);

        gtk_widget_grab_focus (dialog->entry);
        if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (dialog->entry)), dialog->suggested_name) == 0)
        {
            /* The suggested name is in the form "original (1).txt", if the
             * the conflicting name was "original.txt". The user may want to
             * replace the "(1)" bits with with something more meaningful, so
             * select this region for convenience. */

            int start_pos;
            int end_pos;

            start_pos = get_character_position_after_basename (dialog->conflict_name);
            end_pos = get_character_position_after_basename (dialog->suggested_name);

            gtk_editable_select_region (GTK_EDITABLE (dialog->entry), start_pos, end_pos);
        }
    }
    else
    {
        gtk_widget_hide (dialog->rename_button);
        gtk_widget_show (dialog->replace_button);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), CONFLICT_RESPONSE_REPLACE);

        gtk_widget_set_sensitive (dialog->checkbox, TRUE);
    }
}

static void
checkbox_toggled_cb (GtkToggleButton            *t,
                     NautilusFileConflictDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->expander, !gtk_toggle_button_get_active (t));
}

static void
reset_button_clicked_cb (GtkButton                  *w,
                         NautilusFileConflictDialog *dialog)
{
    int start_pos, end_pos;

    gtk_entry_set_text (GTK_ENTRY (dialog->entry), dialog->conflict_name);
    gtk_widget_grab_focus (dialog->entry);
    eel_filename_get_rename_region (dialog->conflict_name, &start_pos, &end_pos);
    gtk_editable_select_region (GTK_EDITABLE (dialog->entry), start_pos, end_pos);
}

static void
nautilus_file_conflict_dialog_init (NautilusFileConflictDialog *fcd)
{
    gtk_widget_init_template (GTK_WIDGET (fcd));
}

static void
do_finalize (GObject *self)
{
    NautilusFileConflictDialog *dialog = NAUTILUS_FILE_CONFLICT_DIALOG (self);

    g_free (dialog->conflict_name);
    g_free (dialog->suggested_name);

    G_OBJECT_CLASS (nautilus_file_conflict_dialog_parent_class)->finalize (self);
}

static void
nautilus_file_conflict_dialog_class_init (NautilusFileConflictDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-file-conflict-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, primary_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, secondary_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, dest_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, src_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, expander);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, checkbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, cancel_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, rename_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, replace_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, skip_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, dest_image);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileConflictDialog, src_image);
    gtk_widget_class_bind_template_callback (widget_class, entry_text_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_expanded_notify);
    gtk_widget_class_bind_template_callback (widget_class, checkbox_toggled_cb);
    gtk_widget_class_bind_template_callback (widget_class, reset_button_clicked_cb);

    G_OBJECT_CLASS (klass)->finalize = do_finalize;
}

static gboolean
activate_buttons (NautilusFileConflictDialog *fcd)
{
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->cancel_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->skip_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->rename_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->replace_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->expander), TRUE);
    return G_SOURCE_REMOVE;
}

void
nautilus_file_conflict_dialog_delay_buttons_activation (NautilusFileConflictDialog *fcd)
{
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->cancel_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->skip_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->rename_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->replace_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->expander), FALSE);

    g_timeout_add_seconds (BUTTON_ACTIVATION_DELAY_IN_SECONDS,
                           G_SOURCE_FUNC (activate_buttons),
                           fcd);
}

char *
nautilus_file_conflict_dialog_get_new_name (NautilusFileConflictDialog *dialog)
{
    return g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->entry)));
}

gboolean
nautilus_file_conflict_dialog_get_apply_to_all (NautilusFileConflictDialog *dialog)
{
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbox));
}

NautilusFileConflictDialog *
nautilus_file_conflict_dialog_new (GtkWindow *parent)
{
    return NAUTILUS_FILE_CONFLICT_DIALOG (g_object_new (NAUTILUS_TYPE_FILE_CONFLICT_DIALOG,
                                                        "transient-for", parent,
                                                        "use-header-bar", TRUE,
                                                        NULL));
}
