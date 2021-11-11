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
    GtkWidget *titles_vbox;
    GtkWidget *first_hbox;
    GtkWidget *second_hbox;
    GtkWidget *expander;
    GtkWidget *entry;
    GtkWidget *checkbox;
    GtkWidget *skip_button;
    GtkWidget *rename_button;
    GtkWidget *replace_button;
    GtkWidget *dest_image;
    GtkWidget *src_image;
};

G_DEFINE_TYPE (NautilusFileConflictDialog, nautilus_file_conflict_dialog, GTK_TYPE_DIALOG);

#define MAX_LABEL_WIDTH 50

void
nautilus_file_conflict_dialog_set_text (NautilusFileConflictDialog *fcd,
                                        gchar                      *primary_text,
                                        gchar                      *secondary_text)
{
    GtkWidget *label;
    PangoAttrList *attr_list;

    label = gtk_label_new (primary_text);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_LABEL_WIDTH);
    gtk_box_pack_start (GTK_BOX (fcd->titles_vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    attr_list = pango_attr_list_new ();
    pango_attr_list_insert (attr_list, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    pango_attr_list_insert (attr_list, pango_attr_scale_new (PANGO_SCALE_LARGE));
    g_object_set (label, "attributes", attr_list, NULL);

    pango_attr_list_unref (attr_list);

    label = gtk_label_new (secondary_text);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_LABEL_WIDTH);
    gtk_box_pack_start (GTK_BOX (fcd->titles_vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
}

void
nautilus_file_conflict_dialog_set_images (NautilusFileConflictDialog *fcd,
                                          GdkPixbuf                  *destination_pixbuf,
                                          GdkPixbuf                  *source_pixbuf)
{
    if (fcd->dest_image == NULL)
    {
        fcd->dest_image = gtk_image_new_from_pixbuf (destination_pixbuf);
        gtk_box_pack_start (GTK_BOX (fcd->first_hbox), fcd->dest_image, FALSE, FALSE, 0);
        gtk_widget_show (fcd->dest_image);
    }
    else
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (fcd->dest_image), destination_pixbuf);
    }

    if (fcd->src_image == NULL)
    {
        fcd->src_image = gtk_image_new_from_pixbuf (source_pixbuf);
        gtk_box_pack_start (GTK_BOX (fcd->second_hbox), fcd->src_image, FALSE, FALSE, 0);
        gtk_widget_show (fcd->src_image);
    }
    else
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (fcd->src_image), source_pixbuf);
    }
}

void
nautilus_file_conflict_dialog_set_file_labels (NautilusFileConflictDialog *fcd,
                                               gchar                      *destination_label,
                                               gchar                      *source_label)
{
    GtkWidget *label;

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), destination_label);
    gtk_box_pack_start (GTK_BOX (fcd->first_hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), source_label);
    gtk_box_pack_start (GTK_BOX (fcd->second_hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
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
    GtkWidget *hbox, *vbox, *vbox2;
    GtkWidget *widget, *dialog_area;
    GtkDialog *dialog;

    dialog = GTK_DIALOG (fcd);

    /* Setup the vbox containing the dialog body */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    dialog_area = gtk_dialog_get_content_area (dialog);
    gtk_box_pack_start (GTK_BOX (dialog_area), vbox, FALSE, FALSE, 0);

    /* Setup the vbox for the dialog labels */
    widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
    fcd->titles_vbox = widget;

    /* Setup the hboxes to pack file infos into */
    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign (vbox2, GTK_ALIGN_START);
    gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
    fcd->first_hbox = hbox;

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
    fcd->second_hbox = hbox;

    /* Setup the expander for the rename action */
    fcd->expander = gtk_expander_new_with_mnemonic (_("_Select a new name for the destination"));
    gtk_box_pack_start (GTK_BOX (vbox2), fcd->expander, FALSE, FALSE, 0);
    g_signal_connect (fcd->expander, "notify::expanded",
                      G_CALLBACK (on_expanded_notify), dialog);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add (GTK_CONTAINER (fcd->expander), hbox);

    widget = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 6);
    fcd->entry = widget;
    g_signal_connect (widget, "changed",
                      G_CALLBACK (entry_text_changed_cb), dialog);
    gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);

    widget = gtk_button_new_with_label (_("Reset"));
    gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 6);
    g_signal_connect (widget, "clicked",
                      G_CALLBACK (reset_button_clicked_cb), dialog);

    gtk_widget_show_all (vbox2);

    /* Setup the checkbox to apply the action to all files */
    widget = gtk_check_button_new_with_mnemonic (_("Apply this action to all files and folders"));

    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
    fcd->checkbox = widget;
    g_signal_connect (widget, "toggled",
                      G_CALLBACK (checkbox_toggled_cb), dialog);

    /* Add buttons */
    gtk_dialog_add_button (dialog, _("_Cancel"), GTK_RESPONSE_CANCEL);

    fcd->skip_button = gtk_dialog_add_button (dialog,
                                              _("_Skip"),
                                              CONFLICT_RESPONSE_SKIP);

    fcd->rename_button = gtk_dialog_add_button (dialog,
                                                _("Re_name"),
                                                CONFLICT_RESPONSE_RENAME);
    gtk_widget_hide (fcd->rename_button);
    gtk_widget_set_no_show_all (fcd->rename_button, TRUE);

    fcd->replace_button = gtk_dialog_add_button (dialog,
                                                 _("Re_place"),
                                                 CONFLICT_RESPONSE_REPLACE);
    gtk_widget_grab_focus (fcd->replace_button);

    /* Setup HIG properties */
    g_object_set (dialog_area,
                  "margin-top", 18,
                  "margin-bottom", 18,
                  "margin-start", 18,
                  "margin-end", 18,
                  NULL);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    gtk_widget_show_all (dialog_area);
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
    G_OBJECT_CLASS (klass)->finalize = do_finalize;
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
    NautilusFileConflictDialog *dialog;

    dialog = NAUTILUS_FILE_CONFLICT_DIALOG (g_object_new (NAUTILUS_TYPE_FILE_CONFLICT_DIALOG,
                                                          "use-header-bar", TRUE,
                                                          "modal", TRUE,
                                                          NULL));

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

    return dialog;
}
