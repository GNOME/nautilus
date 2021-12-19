/* nautilus-batch-rename-dialog.c
 *
 * Copyright (C) 2016 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "nautilus-batch-rename-dialog.h"
#include "nautilus-file.h"
#include "nautilus-error-reporting.h"
#include "nautilus-batch-rename-utilities.h"

#include <glib/gprintf.h>
#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>

#define ROW_MARGIN_START 6
#define ROW_MARGIN_TOP_BOTTOM 4

struct _NautilusBatchRenameDialog
{
    GtkDialog parent;

    GtkWidget *grid;
    NautilusWindow *window;

    GtkWidget *cancel_button;
    GtkWidget *original_name_listbox;
    GtkWidget *arrow_listbox;
    GtkWidget *result_listbox;
    GtkWidget *name_entry;
    GtkWidget *rename_button;
    GtkWidget *find_entry;
    GtkWidget *mode_stack;
    GtkWidget *replace_entry;
    GtkWidget *format_mode_button;
    GtkWidget *replace_mode_button;
    GtkWidget *numbering_order_label;
    GtkWidget *numbering_label;
    GtkWidget *scrolled_window;
    GtkWidget *numbering_revealer;
    GtkWidget *conflict_box;
    GtkWidget *conflict_label;
    GtkWidget *conflict_down;
    GtkWidget *conflict_up;

    GList *listbox_labels_new;
    GList *listbox_labels_old;
    GList *listbox_icons;
    GtkSizeGroup *size_group;

    GList *motion_controllers;

    GList *selection;
    GList *new_names;
    NautilusBatchRenameDialogMode mode;
    NautilusDirectory *directory;

    GActionGroup *action_group;

    GMenu *numbering_order_menu;

    GHashTable *create_date;
    GList *selection_metadata;

    /* the index of the currently selected conflict */
    gint selected_conflict;
    /* total conflicts number */
    gint conflicts_number;

    GList *duplicates;
    GList *distinct_parent_directories;
    GList *directories_pending_conflict_check;

    /* this hash table has information about the status
     * of all tags: availability, if it's currently used
     * and position */
    GHashTable *tag_info_table;

    GtkWidget *preselected_row1;
    GtkWidget *preselected_row2;

    gint row_height;
    gboolean rename_clicked;

    GCancellable *metadata_cancellable;
};

typedef struct
{
    gboolean available;
    gboolean set;
    gint position;
    /* if the tag was just added, then we shouldn't update it's position */
    gboolean just_added;
    TagConstants tag_constants;
} TagData;


static void     update_display_text (NautilusBatchRenameDialog *dialog);
static void     cancel_conflict_check (NautilusBatchRenameDialog *self);

G_DEFINE_TYPE (NautilusBatchRenameDialog, nautilus_batch_rename_dialog, GTK_TYPE_DIALOG);

static void
change_numbering_order (GSimpleAction *action,
                        GVariant      *value,
                        gpointer       user_data)
{
    NautilusBatchRenameDialog *dialog;
    const gchar *target_name;
    guint i;

    dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

    target_name = g_variant_get_string (value, NULL);

    for (i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (g_strcmp0 (sorts_constants[i].action_target_name, target_name) == 0)
        {
            gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                 gettext (sorts_constants[i].label));
            dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                   sorts_constants[i].sort_mode,
                                                                   dialog->create_date);
            break;
        }
    }

    g_simple_action_set_state (action, value);

    update_display_text (dialog);
}

static void
enable_action (NautilusBatchRenameDialog *self,
               const gchar               *action_name)
{
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group),
                                         action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static void
disable_action (NautilusBatchRenameDialog *self,
                const gchar               *action_name)
{
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group),
                                         action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
add_tag (NautilusBatchRenameDialog *self,
         TagConstants               tag_constants)
{
    g_autofree gchar *tag_text_representation = NULL;
    gint cursor_position;
    TagData *tag_data;

    g_object_get (self->name_entry, "cursor-position", &cursor_position, NULL);

    tag_text_representation = batch_rename_get_tag_text_representation (tag_constants);
    tag_data = g_hash_table_lookup (self->tag_info_table, tag_text_representation);
    tag_data->available = TRUE;
    tag_data->set = TRUE;
    tag_data->just_added = TRUE;
    tag_data->position = cursor_position;

    /* FIXME: We can add a tag when the cursor is inside a tag, which breaks this.
     * We need to check the cursor movement and update the actions acordingly or
     * even better add the tag at the end of the previous tag if this happens.
     */
    gtk_editable_insert_text (GTK_EDITABLE (self->name_entry),
                              tag_text_representation,
                              strlen (tag_text_representation),
                              &cursor_position);
    tag_data->just_added = FALSE;
    gtk_editable_set_position (GTK_EDITABLE (self->name_entry), cursor_position);

    gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->name_entry));
}

static void
add_metadata_tag (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       user_data)
{
    NautilusBatchRenameDialog *self;
    const gchar *action_name;
    guint i;

    self = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
    action_name = g_action_get_name (G_ACTION (action));

    for (i = 0; i < G_N_ELEMENTS (metadata_tags_constants); i++)
    {
        if (g_strcmp0 (metadata_tags_constants[i].action_name, action_name) == 0)
        {
            add_tag (self, metadata_tags_constants[i]);
            disable_action (self, metadata_tags_constants[i].action_name);

            break;
        }
    }
}

static void
add_numbering_tag (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       user_data)
{
    NautilusBatchRenameDialog *self;
    const gchar *action_name;
    guint i;

    self = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
    action_name = g_action_get_name (G_ACTION (action));

    for (i = 0; i < G_N_ELEMENTS (numbering_tags_constants); i++)
    {
        if (g_strcmp0 (numbering_tags_constants[i].action_name, action_name) == 0)
        {
            add_tag (self, numbering_tags_constants[i]);
        }
        /* We want to allow only one tag of numbering type, so we disable all
         * of them */
        disable_action (self, numbering_tags_constants[i].action_name);
    }
}

const GActionEntry dialog_entries[] =
{
    { "numbering-order-changed", NULL, "s", "'name-ascending'", change_numbering_order },
    { "add-numbering-no-zero-pad-tag", add_numbering_tag },
    { "add-numbering-one-zero-pad-tag", add_numbering_tag },
    { "add-numbering-two-zero-pad-tag", add_numbering_tag },
    { "add-original-file-name-tag", add_metadata_tag },
    { "add-creation-date-tag", add_metadata_tag },
    { "add-equipment-tag", add_metadata_tag },
    { "add-season-number-tag", add_metadata_tag },
    { "add-episode-number-tag", add_metadata_tag },
    { "add-video-album-tag", add_metadata_tag },
    { "add-track-number-tag", add_metadata_tag },
    { "add-artist-name-tag", add_metadata_tag },
    { "add-title-tag", add_metadata_tag },
    { "add-album-name-tag", add_metadata_tag },
};

static void
row_selected (GtkListBox    *box,
              GtkListBoxRow *listbox_row,
              gpointer       user_data)
{
    NautilusBatchRenameDialog *dialog;
    GtkListBoxRow *row;
    gint index;

    if (!GTK_IS_LIST_BOX_ROW (listbox_row))
    {
        return;
    }

    dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
    index = gtk_list_box_row_get_index (listbox_row);

    if (GTK_WIDGET (box) == dialog->original_name_listbox)
    {
        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->arrow_listbox), index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->arrow_listbox),
                                 row);
        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->result_listbox), index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->result_listbox),
                                 row);
    }

    if (GTK_WIDGET (box) == dialog->arrow_listbox)
    {
        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->original_name_listbox), index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->original_name_listbox),
                                 row);
        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->result_listbox), index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->result_listbox),
                                 row);
    }

    if (GTK_WIDGET (box) == dialog->result_listbox)
    {
        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->arrow_listbox), index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->arrow_listbox),
                                 row);
        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->original_name_listbox), index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->original_name_listbox),
                                 row);
    }
}

static gint
compare_int (gconstpointer a,
             gconstpointer b)
{
    int *number1 = (int *) a;
    int *number2 = (int *) b;

    return *number1 - *number2;
}

/* This function splits the entry text into a list of regular text and tags.
 * For instance, "[1, 2, 3]Paris[Creation date]" would result in:
 * "[1, 2, 3]", "Paris", "[Creation date]" */
static GList *
split_entry_text (NautilusBatchRenameDialog *self,
                  gchar                     *entry_text)
{
    GString *normal_text;
    GString *tag;
    GArray *tag_positions;
    g_autoptr (GList) tag_info_keys = NULL;
    GList *l;
    gint tags;
    gint i;
    gchar *substring;
    gint tag_end_position;
    GList *result = NULL;
    TagData *tag_data;

    tags = 0;
    tag_end_position = 0;
    tag_positions = g_array_new (FALSE, FALSE, sizeof (gint));

    tag_info_keys = g_hash_table_get_keys (self->tag_info_table);

    for (l = tag_info_keys; l != NULL; l = l->next)
    {
        tag_data = g_hash_table_lookup (self->tag_info_table, l->data);
        if (tag_data->set)
        {
            g_array_append_val (tag_positions, tag_data->position);
            tags++;
        }
    }

    g_array_sort (tag_positions, compare_int);

    for (i = 0; i < tags; i++)
    {
        tag = g_string_new ("");

        substring = g_utf8_substring (entry_text, tag_end_position,
                                      g_array_index (tag_positions, gint, i));
        normal_text = g_string_new (substring);
        g_free (substring);

        if (g_strcmp0 (normal_text->str, ""))
        {
            result = g_list_prepend (result, normal_text);
        }
        else
        {
            g_string_free (normal_text, TRUE);
        }

        for (l = tag_info_keys; l != NULL; l = l->next)
        {
            g_autofree gchar *tag_text_representation = NULL;

            tag_data = g_hash_table_lookup (self->tag_info_table, l->data);
            if (tag_data->set && g_array_index (tag_positions, gint, i) == tag_data->position)
            {
                tag_text_representation = batch_rename_get_tag_text_representation (tag_data->tag_constants);
                tag_end_position = g_array_index (tag_positions, gint, i) +
                                   g_utf8_strlen (tag_text_representation, -1);
                tag = g_string_append (tag, tag_text_representation);

                break;
            }
        }

        result = g_list_prepend (result, tag);
    }

    normal_text = g_string_new (g_utf8_offset_to_pointer (entry_text, tag_end_position));

    if (g_strcmp0 (normal_text->str, "") != 0)
    {
        result = g_list_prepend (result, normal_text);
    }
    else
    {
        g_string_free (normal_text, TRUE);
    }

    result = g_list_reverse (result);

    g_array_free (tag_positions, TRUE);
    return result;
}

static GList *
batch_rename_dialog_get_new_names (NautilusBatchRenameDialog *dialog)
{
    GList *result = NULL;
    GList *selection;
    GList *text_chunks;
    g_autofree gchar *entry_text = NULL;
    g_autofree gchar *replace_text = NULL;

    selection = dialog->selection;
    text_chunks = NULL;

    if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
    {
        entry_text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (dialog->find_entry)));
    }
    else
    {
        entry_text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (dialog->name_entry)));
    }

    replace_text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (dialog->replace_entry)));

    if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
    {
        result = batch_rename_dialog_get_new_names_list (dialog->mode,
                                                         selection,
                                                         NULL,
                                                         NULL,
                                                         entry_text,
                                                         replace_text);
    }
    else
    {
        text_chunks = split_entry_text (dialog, entry_text);

        result = batch_rename_dialog_get_new_names_list (dialog->mode,
                                                         selection,
                                                         text_chunks,
                                                         dialog->selection_metadata,
                                                         entry_text,
                                                         replace_text);
        g_list_free_full (text_chunks, string_free);
    }

    result = g_list_reverse (result);

    return result;
}

static void
begin_batch_rename (NautilusBatchRenameDialog *dialog,
                    GList                     *new_names)
{
    batch_rename_sort_lists_for_rename (&dialog->selection, &new_names, NULL, NULL, NULL, FALSE);

    /* do the actual rename here */
    nautilus_file_batch_rename (dialog->selection, new_names, NULL, NULL);

    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog->window)), NULL);
}

static void
listbox_header_func (GtkListBoxRow             *row,
                     GtkListBoxRow             *before,
                     NautilusBatchRenameDialog *dialog)
{
    GtkWidget *separator;

    if (before == NULL)
    {
        /* First row needs no separator */
        gtk_list_box_row_set_header (row, NULL);
        return;
    }

    separator = gtk_list_box_row_get_header (row);
    if (separator == NULL)
    {
        separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_show (separator);

        gtk_list_box_row_set_header (row, separator);
    }
}

/* This is manually done instead of using GtkSizeGroup because of the computational
 * complexity of the later.*/
static void
update_rows_height (NautilusBatchRenameDialog *dialog)
{
    GList *l;
    GtkRequisition current_row_natural_size;
    gint maximum_height;

    maximum_height = -1;

    /* check if maximum height has changed */
    for (l = dialog->listbox_labels_new; l != NULL; l = l->next)
    {
        gtk_widget_get_preferred_size (GTK_WIDGET (l->data),
                                       NULL,
                                       &current_row_natural_size);

        if (current_row_natural_size.height > maximum_height)
        {
            maximum_height = current_row_natural_size.height;
        }
    }

    for (l = dialog->listbox_labels_old; l != NULL; l = l->next)
    {
        gtk_widget_get_preferred_size (GTK_WIDGET (l->data),
                                       NULL,
                                       &current_row_natural_size);

        if (current_row_natural_size.height > maximum_height)
        {
            maximum_height = current_row_natural_size.height;
        }
    }

    for (l = dialog->listbox_icons; l != NULL; l = l->next)
    {
        gtk_widget_get_preferred_size (GTK_WIDGET (l->data),
                                       NULL,
                                       &current_row_natural_size);

        if (current_row_natural_size.height > maximum_height)
        {
            maximum_height = current_row_natural_size.height;
        }
    }

    if (maximum_height != dialog->row_height)
    {
        dialog->row_height = maximum_height + ROW_MARGIN_TOP_BOTTOM * 2;

        for (l = dialog->listbox_icons; l != NULL; l = l->next)
        {
            g_object_set (G_OBJECT (l->data), "height-request", dialog->row_height, NULL);
        }

        for (l = dialog->listbox_labels_new; l != NULL; l = l->next)
        {
            g_object_set (G_OBJECT (l->data), "height-request", dialog->row_height, NULL);
        }

        for (l = dialog->listbox_labels_old; l != NULL; l = l->next)
        {
            g_object_set (G_OBJECT (l->data), "height-request", dialog->row_height, NULL);
        }
    }
}

static GtkWidget *
create_original_name_label (NautilusBatchRenameDialog *dialog,
                            const gchar               *old_text)
{
    GtkWidget *label_old;

    label_old = gtk_label_new (old_text);
    gtk_label_set_xalign (GTK_LABEL (label_old), 0.0);
    gtk_widget_set_hexpand (label_old, TRUE);
    gtk_widget_set_margin_start (label_old, ROW_MARGIN_START);
    gtk_label_set_ellipsize (GTK_LABEL (label_old), PANGO_ELLIPSIZE_END);

    dialog->listbox_labels_old = g_list_prepend (dialog->listbox_labels_old, label_old);

    gtk_widget_show_all (label_old);

    return label_old;
}

static GtkWidget *
create_result_label (NautilusBatchRenameDialog *dialog,
                     const gchar               *new_text)
{
    GtkWidget *label_new;

    label_new = gtk_label_new (new_text);
    gtk_label_set_xalign (GTK_LABEL (label_new), 0.0);
    gtk_widget_set_hexpand (label_new, TRUE);
    gtk_widget_set_margin_start (label_new, ROW_MARGIN_START);
    gtk_label_set_ellipsize (GTK_LABEL (label_new), PANGO_ELLIPSIZE_END);

    dialog->listbox_labels_new = g_list_prepend (dialog->listbox_labels_new, label_new);

    gtk_widget_show_all (label_new);

    return label_new;
}

static GtkWidget *
create_arrow (NautilusBatchRenameDialog *dialog,
              GtkTextDirection           text_direction)
{
    GtkWidget *icon;

    if (text_direction == GTK_TEXT_DIR_RTL)
    {
        icon = gtk_label_new ("←");
    }
    else
    {
        icon = gtk_label_new ("→");
    }

    gtk_label_set_xalign (GTK_LABEL (icon), 1.0);
    gtk_widget_set_hexpand (icon, FALSE);
    gtk_widget_set_margin_start (icon, ROW_MARGIN_START);

    dialog->listbox_icons = g_list_prepend (dialog->listbox_icons, icon);

    gtk_widget_show_all (icon);

    return icon;
}

static void
prepare_batch_rename (NautilusBatchRenameDialog *dialog)
{
    GdkCursor *cursor;
    GdkDisplay *display;

    /* wait for checking conflicts to finish, to be sure that
     * the rename can actually take place */
    if (dialog->directories_pending_conflict_check != NULL)
    {
        dialog->rename_clicked = TRUE;
        return;
    }

    if (!gtk_widget_is_sensitive (dialog->rename_button))
    {
        return;
    }

    display = gtk_widget_get_display (GTK_WIDGET (dialog->window));
    cursor = gdk_cursor_new_from_name (display, "progress");
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog->window)),
                           cursor);
    g_object_unref (cursor);

    display = gtk_widget_get_display (GTK_WIDGET (dialog));
    cursor = gdk_cursor_new_from_name (display, "progress");
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog)),
                           cursor);
    g_object_unref (cursor);

    gtk_widget_hide (GTK_WIDGET (dialog));
    begin_batch_rename (dialog, dialog->new_names);

    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
batch_rename_dialog_on_response (NautilusBatchRenameDialog *dialog,
                                 gint                       response_id,
                                 gpointer                   user_data)
{
    if (response_id == GTK_RESPONSE_OK)
    {
        prepare_batch_rename (dialog);
    }
    else
    {
        if (dialog->directories_pending_conflict_check != NULL)
        {
            cancel_conflict_check (dialog);
        }

        gtk_window_destroy (GTK_WINDOW (dialog));
    }
}

static void
fill_display_listbox (NautilusBatchRenameDialog *dialog)
{
    GtkWidget *row_child;
    GList *l1;
    GList *l2;
    NautilusFile *file;
    GString *new_name;
    gchar *name;
    GtkTextDirection text_direction;

    gtk_size_group_add_widget (dialog->size_group, dialog->result_listbox);
    gtk_size_group_add_widget (dialog->size_group, dialog->original_name_listbox);

    text_direction = gtk_widget_get_direction (GTK_WIDGET (dialog));

    for (l1 = dialog->new_names, l2 = dialog->selection; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        file = NAUTILUS_FILE (l2->data);
        new_name = l1->data;

        name = nautilus_file_get_name (file);
        row_child = create_original_name_label (dialog, name);
        gtk_list_box_insert (GTK_LIST_BOX (dialog->original_name_listbox), row_child, -1);

        row_child = create_arrow (dialog, text_direction);
        gtk_list_box_insert (GTK_LIST_BOX (dialog->arrow_listbox), row_child, -1);

        row_child = create_result_label (dialog, new_name->str);
        gtk_list_box_insert (GTK_LIST_BOX (dialog->result_listbox), row_child, -1);

        g_free (name);
    }

    dialog->listbox_labels_old = g_list_reverse (dialog->listbox_labels_old);
    dialog->listbox_labels_new = g_list_reverse (dialog->listbox_labels_new);
    dialog->listbox_icons = g_list_reverse (dialog->listbox_icons);
}

static void
select_nth_conflict (NautilusBatchRenameDialog *dialog)
{
    GList *l;
    GString *conflict_file_name;
    GString *display_text;
    GString *new_name;
    gint nth_conflict_index;
    gint nth_conflict;
    gint name_occurences;
    GtkAdjustment *adjustment;
    GtkAllocation allocation;
    ConflictData *conflict_data;
    GtkListBoxRow *list_box_row;

    nth_conflict = dialog->selected_conflict;
    l = g_list_nth (dialog->duplicates, nth_conflict);
    conflict_data = l->data;

    /* the conflict that has to be selected */
    conflict_file_name = g_string_new (conflict_data->name);
    display_text = g_string_new ("");

    nth_conflict_index = conflict_data->index;

    l = g_list_nth (dialog->listbox_labels_new, nth_conflict_index);
    list_box_row = GTK_LIST_BOX_ROW (gtk_widget_get_parent (l->data));
    gtk_list_box_select_row (GTK_LIST_BOX (dialog->original_name_listbox),
                             list_box_row);

    l = g_list_nth (dialog->listbox_labels_old, nth_conflict_index);
    list_box_row = GTK_LIST_BOX_ROW (gtk_widget_get_parent (l->data));
    gtk_list_box_select_row (GTK_LIST_BOX (dialog->arrow_listbox),
                             list_box_row);

    l = g_list_nth (dialog->listbox_icons, nth_conflict_index);
    list_box_row = GTK_LIST_BOX_ROW (gtk_widget_get_parent (l->data));
    gtk_list_box_select_row (GTK_LIST_BOX (dialog->result_listbox),
                             list_box_row);

    /* scroll to the selected row */
    adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (dialog->scrolled_window));
    gtk_widget_get_allocation (GTK_WIDGET (l->data), &allocation);
    gtk_adjustment_set_value (adjustment, (allocation.height + 1) * nth_conflict_index);

    name_occurences = 0;
    for (l = dialog->new_names; l != NULL; l = l->next)
    {
        new_name = l->data;
        if (g_string_equal (new_name, conflict_file_name))
        {
            name_occurences++;
        }
    }
    if (name_occurences > 1)
    {
        g_string_append_printf (display_text,
                                _("“%s” would not be a unique new name."),
                                conflict_file_name->str);
    }
    else
    {
        g_string_append_printf (display_text,
                                _("“%s” would conflict with an existing file."),
                                conflict_file_name->str);
    }

    gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                         display_text->str);

    g_string_free (conflict_file_name, TRUE);
    g_string_free (display_text, TRUE);
}

static void
select_next_conflict_down (NautilusBatchRenameDialog *dialog)
{
    dialog->selected_conflict++;

    if (dialog->selected_conflict == 1)
    {
        gtk_widget_set_sensitive (dialog->conflict_up, TRUE);
    }

    if (dialog->selected_conflict == dialog->conflicts_number - 1)
    {
        gtk_widget_set_sensitive (dialog->conflict_down, FALSE);
    }

    select_nth_conflict (dialog);
}

static void
select_next_conflict_up (NautilusBatchRenameDialog *dialog)
{
    dialog->selected_conflict--;

    if (dialog->selected_conflict == 0)
    {
        gtk_widget_set_sensitive (dialog->conflict_up, FALSE);
    }

    if (dialog->selected_conflict == dialog->conflicts_number - 2)
    {
        gtk_widget_set_sensitive (dialog->conflict_down, TRUE);
    }

    select_nth_conflict (dialog);
}

static void
update_conflict_row_background (NautilusBatchRenameDialog *dialog)
{
    GList *l1;
    GList *l2;
    GList *l3;
    GList *duplicates;
    gint index;
    GtkStyleContext *context;
    ConflictData *conflict_data;

    index = 0;

    duplicates = dialog->duplicates;

    for (l1 = dialog->listbox_labels_new,
         l2 = dialog->listbox_labels_old,
         l3 = dialog->listbox_icons;
         l1 != NULL && l2 != NULL && l3 != NULL;
         l1 = l1->next, l2 = l2->next, l3 = l3->next)
    {
        GtkWidget *row1 = gtk_widget_get_parent (l1->data);
        GtkWidget *row2 = gtk_widget_get_parent (l2->data);
        GtkWidget *row3 = gtk_widget_get_parent (l3->data);

        context = gtk_widget_get_style_context (row1);

        if (gtk_style_context_has_class (context, "conflict-row"))
        {
            gtk_style_context_remove_class (context, "conflict-row");

            context = gtk_widget_get_style_context (row2);
            gtk_style_context_remove_class (context, "conflict-row");

            context = gtk_widget_get_style_context (row3);
            gtk_style_context_remove_class (context, "conflict-row");
        }

        if (duplicates != NULL)
        {
            conflict_data = duplicates->data;
            if (conflict_data->index == index)
            {
                context = gtk_widget_get_style_context (row1);
                gtk_style_context_add_class (context, "conflict-row");

                context = gtk_widget_get_style_context (row2);
                gtk_style_context_add_class (context, "conflict-row");

                context = gtk_widget_get_style_context (row3);
                gtk_style_context_add_class (context, "conflict-row");

                duplicates = duplicates->next;
            }
        }
        index++;
    }
}

static void
update_listbox (NautilusBatchRenameDialog *dialog)
{
    GList *l1;
    GList *l2;
    NautilusFile *file;
    gchar *old_name;
    GtkLabel *label;
    GString *new_name;
    gboolean empty_name = FALSE;

    for (l1 = dialog->new_names, l2 = dialog->listbox_labels_new; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        label = GTK_LABEL (l2->data);
        new_name = l1->data;

        gtk_label_set_label (label, new_name->str);
        gtk_widget_set_tooltip_text (GTK_WIDGET (label), new_name->str);

        if (g_strcmp0 (new_name->str, "") == 0)
        {
            empty_name = TRUE;
        }
    }

    for (l1 = dialog->selection, l2 = dialog->listbox_labels_old; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        label = GTK_LABEL (l2->data);
        file = NAUTILUS_FILE (l1->data);

        old_name = nautilus_file_get_name (file);
        gtk_widget_set_tooltip_text (GTK_WIDGET (label), old_name);

        if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT)
        {
            gtk_label_set_label (label, old_name);
        }
        else
        {
            new_name = batch_rename_replace_label_text (old_name,
                                                        gtk_editable_get_text (GTK_EDITABLE (dialog->find_entry)));
            gtk_label_set_markup (GTK_LABEL (label), new_name->str);

            g_string_free (new_name, TRUE);
        }

        g_free (old_name);
    }

    update_rows_height (dialog);

    if (empty_name)
    {
        gtk_widget_set_sensitive (dialog->rename_button, FALSE);

        return;
    }

    /* check if there are name conflicts and display them if they exist */
    if (dialog->duplicates != NULL)
    {
        update_conflict_row_background (dialog);

        gtk_widget_set_sensitive (dialog->rename_button, FALSE);

        gtk_widget_show (dialog->conflict_box);

        dialog->selected_conflict = 0;
        dialog->conflicts_number = g_list_length (dialog->duplicates);

        select_nth_conflict (dialog);

        gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

        if (g_list_length (dialog->duplicates) == 1)
        {
            gtk_widget_set_sensitive (dialog->conflict_down, FALSE);
        }
        else
        {
            gtk_widget_set_sensitive (dialog->conflict_down, TRUE);
        }
    }
    else
    {
        gtk_widget_hide (dialog->conflict_box);

        /* re-enable the rename button if there are no more name conflicts */
        if (dialog->duplicates == NULL && !gtk_widget_is_sensitive (dialog->rename_button))
        {
            update_conflict_row_background (dialog);
            gtk_widget_set_sensitive (dialog->rename_button, TRUE);
        }
    }

    /* if the rename button was clicked and there's no conflict, then start renaming */
    if (dialog->rename_clicked && dialog->duplicates == NULL)
    {
        prepare_batch_rename (dialog);
    }

    if (dialog->rename_clicked && dialog->duplicates != NULL)
    {
        dialog->rename_clicked = FALSE;
    }
}

static void
check_conflict_for_files (NautilusBatchRenameDialog *dialog,
                          NautilusDirectory         *directory,
                          GList                     *files)
{
    gchar *current_directory;
    gchar *parent_uri;
    gchar *name;
    NautilusFile *file;
    GString *new_name;
    GString *file_name;
    GList *l1, *l2;
    GHashTable *directory_files_table;
    GHashTable *new_names_table;
    GHashTable *names_conflicts_table;
    gboolean exists;
    gboolean have_conflict;
    gboolean tag_present;
    gboolean same_parent_directory;
    ConflictData *conflict_data;

    current_directory = nautilus_directory_get_uri (directory);

    directory_files_table = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   (GDestroyNotify) g_free,
                                                   NULL);
    new_names_table = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             (GDestroyNotify) g_free,
                                             (GDestroyNotify) g_free);
    names_conflicts_table = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   (GDestroyNotify) g_free,
                                                   (GDestroyNotify) g_free);

    /* names_conflicts_table is used for knowing which names from the list are not unique,
     * so that they can easily be reached when needed */
    for (l1 = dialog->new_names, l2 = dialog->selection;
         l1 != NULL && l2 != NULL;
         l1 = l1->next, l2 = l2->next)
    {
        new_name = l1->data;
        file = NAUTILUS_FILE (l2->data);
        parent_uri = nautilus_file_get_parent_uri (file);

        tag_present = g_hash_table_lookup (new_names_table, new_name->str) != NULL;
        same_parent_directory = g_strcmp0 (parent_uri, current_directory) == 0;

        if (same_parent_directory)
        {
            if (!tag_present)
            {
                g_hash_table_insert (new_names_table,
                                     g_strdup (new_name->str),
                                     nautilus_file_get_parent_uri (file));
            }
            else
            {
                g_hash_table_insert (names_conflicts_table,
                                     g_strdup (new_name->str),
                                     nautilus_file_get_parent_uri (file));
            }
        }

        g_free (parent_uri);
    }

    for (l1 = files; l1 != NULL; l1 = l1->next)
    {
        file = NAUTILUS_FILE (l1->data);
        g_hash_table_insert (directory_files_table,
                             nautilus_file_get_name (file),
                             GINT_TO_POINTER (TRUE));
    }

    for (l1 = dialog->selection, l2 = dialog->new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        file = NAUTILUS_FILE (l1->data);

        name = nautilus_file_get_name (file);
        file_name = g_string_new (name);
        g_free (name);

        parent_uri = nautilus_file_get_parent_uri (file);

        new_name = l2->data;

        have_conflict = FALSE;

        /* check for duplicate only if the parent of the current file is
         * the current directory and the name of the file has changed */
        if (g_strcmp0 (parent_uri, current_directory) == 0 &&
            !g_string_equal (new_name, file_name))
        {
            exists = GPOINTER_TO_INT (g_hash_table_lookup (directory_files_table, new_name->str));

            if (exists == TRUE &&
                !file_name_conflicts_with_results (dialog->selection, dialog->new_names, new_name, parent_uri))
            {
                conflict_data = g_new (ConflictData, 1);
                conflict_data->name = g_strdup (new_name->str);
                conflict_data->index = g_list_index (dialog->selection, l1->data);
                dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                     conflict_data);

                have_conflict = TRUE;
            }
        }

        if (!have_conflict)
        {
            tag_present = g_hash_table_lookup (names_conflicts_table, new_name->str) != NULL;
            same_parent_directory = g_strcmp0 (parent_uri, current_directory) == 0;

            if (tag_present && same_parent_directory)
            {
                conflict_data = g_new (ConflictData, 1);
                conflict_data->name = g_strdup (new_name->str);
                conflict_data->index = g_list_index (dialog->selection, l1->data);
                dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                     conflict_data);

                have_conflict = TRUE;
            }
        }

        g_string_free (file_name, TRUE);
        g_free (parent_uri);
    }

    g_free (current_directory);
    g_hash_table_destroy (directory_files_table);
    g_hash_table_destroy (new_names_table);
    g_hash_table_destroy (names_conflicts_table);
}

static void
on_directory_attributes_ready_for_conflicts_check (NautilusDirectory *conflict_directory,
                                                   GList             *files,
                                                   gpointer           callback_data)
{
    NautilusBatchRenameDialog *self;

    self = NAUTILUS_BATCH_RENAME_DIALOG (callback_data);

    check_conflict_for_files (self, conflict_directory, files);

    g_assert (g_list_find (self->directories_pending_conflict_check, conflict_directory) != NULL);

    self->directories_pending_conflict_check = g_list_remove (self->directories_pending_conflict_check, conflict_directory);

    nautilus_directory_unref (conflict_directory);

    if (self->directories_pending_conflict_check == NULL)
    {
        self->duplicates = g_list_reverse (self->duplicates);

        update_listbox (self);
    }
}

static void
cancel_conflict_check (NautilusBatchRenameDialog *self)
{
    GList *l;
    NautilusDirectory *directory;

    for (l = self->directories_pending_conflict_check; l != NULL; l = l->next)
    {
        directory = l->data;

        nautilus_directory_cancel_callback (directory,
                                            on_directory_attributes_ready_for_conflicts_check,
                                            self);
    }

    g_clear_list (&self->directories_pending_conflict_check, g_object_unref);
}

static void
file_names_list_has_duplicates_async (NautilusBatchRenameDialog *self)
{
    GList *l;

    if (self->directories_pending_conflict_check != NULL)
    {
        cancel_conflict_check (self);
    }

    self->directories_pending_conflict_check = nautilus_directory_list_copy (self->distinct_parent_directories);
    self->duplicates = NULL;

    for (l = self->distinct_parent_directories; l != NULL; l = l->next)
    {
        nautilus_directory_call_when_ready (l->data,
                                            NAUTILUS_FILE_ATTRIBUTE_INFO,
                                            TRUE,
                                            on_directory_attributes_ready_for_conflicts_check,
                                            self);
    }
}

static gboolean
have_unallowed_character (NautilusBatchRenameDialog *dialog)
{
    GList *names;
    GString *new_name;
    const gchar *entry_text;
    gboolean have_empty_name;
    gboolean have_unallowed_character_slash;
    gboolean have_unallowed_character_dot;
    gboolean have_unallowed_character_dotdot;

    have_empty_name = FALSE;
    have_unallowed_character_slash = FALSE;
    have_unallowed_character_dot = FALSE;
    have_unallowed_character_dotdot = FALSE;

    if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT)
    {
        entry_text = gtk_editable_get_text (GTK_EDITABLE (dialog->name_entry));
    }
    else
    {
        entry_text = gtk_editable_get_text (GTK_EDITABLE (dialog->replace_entry));
    }

    if (strstr (entry_text, "/") != NULL)
    {
        have_unallowed_character_slash = TRUE;
    }

    if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT && g_strcmp0 (entry_text, ".") == 0)
    {
        have_unallowed_character_dot = TRUE;
    }
    else if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
    {
        for (names = dialog->new_names; names != NULL; names = names->next)
        {
            new_name = names->data;

            if (g_strcmp0 (new_name->str, ".") == 0)
            {
                have_unallowed_character_dot = TRUE;
                break;
            }
        }
    }

    if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT && g_strcmp0 (entry_text, "..") == 0)
    {
        have_unallowed_character_dotdot = TRUE;
    }
    else if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
    {
        for (names = dialog->new_names; names != NULL; names = names->next)
        {
            new_name = names->data;

            if (g_strcmp0 (new_name->str, "") == 0)
            {
                have_empty_name = TRUE;
                break;
            }

            if (g_strcmp0 (new_name->str, "..") == 0)
            {
                have_unallowed_character_dotdot = TRUE;
                break;
            }
        }
    }

    if (have_empty_name)
    {
        gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                             _("Name cannot be empty."));
    }

    if (have_unallowed_character_slash)
    {
        gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                             _("Name cannot contain “/”."));
    }

    if (have_unallowed_character_dot)
    {
        gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                             _("“.” is not a valid name."));
    }

    if (have_unallowed_character_dotdot)
    {
        gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                             _("“..” is not a valid name."));
    }

    if (have_unallowed_character_slash || have_unallowed_character_dot || have_unallowed_character_dotdot
        || have_empty_name)
    {
        gtk_widget_set_sensitive (dialog->rename_button, FALSE);
        gtk_widget_set_sensitive (dialog->conflict_down, FALSE);
        gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

        gtk_widget_show (dialog->conflict_box);

        return TRUE;
    }
    else
    {
        gtk_widget_hide (dialog->conflict_box);

        return FALSE;
    }
}

static gboolean
numbering_tag_is_some_added (NautilusBatchRenameDialog *self)
{
    guint i;
    TagData *tag_data;

    for (i = 0; i < G_N_ELEMENTS (numbering_tags_constants); i++)
    {
        g_autofree gchar *tag_text_representation = NULL;

        tag_text_representation = batch_rename_get_tag_text_representation (numbering_tags_constants[i]);
        tag_data = g_hash_table_lookup (self->tag_info_table, tag_text_representation);
        if (tag_data->set)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
update_display_text (NautilusBatchRenameDialog *dialog)
{
    if (dialog->selection == NULL)
    {
        return;
    }

    if (dialog->duplicates != NULL)
    {
        g_list_free_full (dialog->duplicates, conflict_data_free);
        dialog->duplicates = NULL;
    }

    if (dialog->new_names != NULL)
    {
        g_list_free_full (dialog->new_names, string_free);
    }

    if (!numbering_tag_is_some_added (dialog))
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (dialog->numbering_revealer), FALSE);
    }
    else
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (dialog->numbering_revealer), TRUE);
    }

    dialog->new_names = batch_rename_dialog_get_new_names (dialog);

    if (have_unallowed_character (dialog))
    {
        return;
    }

    file_names_list_has_duplicates_async (dialog);
}

static void
batch_rename_dialog_mode_changed (NautilusBatchRenameDialog *dialog)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->format_mode_button)))
    {
        gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "format");

        dialog->mode = NAUTILUS_BATCH_RENAME_DIALOG_FORMAT;

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));
    }
    else
    {
        gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "replace");

        dialog->mode = NAUTILUS_BATCH_RENAME_DIALOG_REPLACE;

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->find_entry));
    }

    update_display_text (dialog);
}

void
nautilus_batch_rename_dialog_query_finished (NautilusBatchRenameDialog *dialog,
                                             GHashTable                *hash_table,
                                             GList                     *selection_metadata)
{
    GMenuItem *first_created;
    GMenuItem *last_created;
    FileMetadata *file_metadata;
    MetadataType metadata_type;
    gboolean is_metadata;
    TagData *tag_data;
    g_autoptr (GList) tag_info_keys = NULL;
    GList *l;

    /* for files with no metadata */
    if (hash_table != NULL && g_hash_table_size (hash_table) == 0)
    {
        g_hash_table_destroy (hash_table);

        hash_table = NULL;
    }

    if (hash_table == NULL)
    {
        dialog->create_date = NULL;
    }
    else
    {
        dialog->create_date = hash_table;
    }

    if (dialog->create_date != NULL)
    {
        first_created = g_menu_item_new ("First Created",
                                         "dialog.numbering-order-changed('first-created')");

        g_menu_append_item (dialog->numbering_order_menu, first_created);

        last_created = g_menu_item_new ("Last Created",
                                        "dialog.numbering-order-changed('last-created')");

        g_menu_append_item (dialog->numbering_order_menu, last_created);
    }

    dialog->selection_metadata = selection_metadata;
    file_metadata = selection_metadata->data;
    tag_info_keys = g_hash_table_get_keys (dialog->tag_info_table);
    for (l = tag_info_keys; l != NULL; l = l->next)
    {
        /* Only metadata has to be handled here. */
        tag_data = g_hash_table_lookup (dialog->tag_info_table, l->data);
        is_metadata = tag_data->tag_constants.is_metadata;
        if (!is_metadata)
        {
            continue;
        }

        metadata_type = tag_data->tag_constants.metadata_type;
        if (file_metadata->metadata[metadata_type] == NULL ||
            file_metadata->metadata[metadata_type]->len <= 0)
        {
            disable_action (dialog, tag_data->tag_constants.action_name);
            tag_data->available = FALSE;
        }
    }
}

static void
update_row_shadowing (GtkWidget *row,
                      gboolean   shown)
{
    GtkStyleContext *context;
    GtkStateFlags flags;

    if (!GTK_IS_LIST_BOX_ROW (row))
    {
        return;
    }

    context = gtk_widget_get_style_context (row);
    flags = gtk_style_context_get_state (context);

    if (shown)
    {
        flags |= GTK_STATE_FLAG_PRELIGHT;
    }
    else
    {
        flags &= ~GTK_STATE_FLAG_PRELIGHT;
    }

    gtk_style_context_set_state (context, flags);
}

static void
on_event_controller_motion_motion (GtkEventControllerMotion *controller,
                                   double                    x,
                                   double                    y,
                                   gpointer                  user_data)
{
    GtkWidget *widget;
    NautilusBatchRenameDialog *dialog;
    GtkListBoxRow *row;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

    if (dialog->preselected_row1 && dialog->preselected_row2)
    {
        update_row_shadowing (dialog->preselected_row1, FALSE);
        update_row_shadowing (dialog->preselected_row2, FALSE);
    }

    if (widget == dialog->result_listbox)
    {
        row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->original_name_listbox), y);
        update_row_shadowing (GTK_WIDGET (row), TRUE);
        dialog->preselected_row1 = GTK_WIDGET (row);

        row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->arrow_listbox), y);
        update_row_shadowing (GTK_WIDGET (row), TRUE);
        dialog->preselected_row2 = GTK_WIDGET (row);
    }

    if (widget == dialog->arrow_listbox)
    {
        row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->original_name_listbox), y);
        update_row_shadowing (GTK_WIDGET (row), TRUE);
        dialog->preselected_row1 = GTK_WIDGET (row);

        row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->result_listbox), y);
        update_row_shadowing (GTK_WIDGET (row), TRUE);
        dialog->preselected_row2 = GTK_WIDGET (row);
    }

    if (widget == dialog->original_name_listbox)
    {
        row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->result_listbox), y);
        update_row_shadowing (GTK_WIDGET (row), TRUE);
        dialog->preselected_row1 = GTK_WIDGET (row);

        row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->arrow_listbox), y);
        update_row_shadowing (GTK_WIDGET (row), TRUE);
        dialog->preselected_row2 = GTK_WIDGET (row);
    }
}

static void
on_event_controller_motion_leave (GtkEventControllerMotion *controller,
                                  gpointer                  user_data)
{
    NautilusBatchRenameDialog *dialog;

    dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

    update_row_shadowing (dialog->preselected_row1, FALSE);
    update_row_shadowing (dialog->preselected_row2, FALSE);

    dialog->preselected_row1 = NULL;
    dialog->preselected_row2 = NULL;
}

static void
nautilus_batch_rename_dialog_initialize_actions (NautilusBatchRenameDialog *dialog)
{
    GAction *action;

    dialog->action_group = G_ACTION_GROUP (g_simple_action_group_new ());

    g_action_map_add_action_entries (G_ACTION_MAP (dialog->action_group),
                                     dialog_entries,
                                     G_N_ELEMENTS (dialog_entries),
                                     dialog);
    gtk_widget_insert_action_group (GTK_WIDGET (dialog),
                                    "dialog",
                                    G_ACTION_GROUP (dialog->action_group));

    action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                         metadata_tags_constants[ORIGINAL_FILE_NAME].action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    check_metadata_for_selection (dialog, dialog->selection,
                                  dialog->metadata_cancellable);
}

static void
file_names_widget_on_activate (NautilusBatchRenameDialog *dialog)
{
    prepare_batch_rename (dialog);
}

static void
remove_tag (NautilusBatchRenameDialog *dialog,
            TagData                   *tag_data)
{
    GAction *action;

    if (!tag_data->set)
    {
        g_warning ("Trying to remove an already removed tag");

        return;
    }

    tag_data->set = FALSE;
    tag_data->position = -1;
    action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                         tag_data->tag_constants.action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static gint
compare_tag_position (gconstpointer a,
                      gconstpointer b)
{
    const TagData *tag_data1 = a;
    const TagData *tag_data2 = b;

    return tag_data1->position - tag_data2->position;
}

typedef enum
{
    TEXT_WAS_DELETED,
    TEXT_WAS_INSERTED
} TextChangedMode;

static GList *
get_tags_intersecting_sorted (NautilusBatchRenameDialog *self,
                              gint                       start_position,
                              gint                       end_position,
                              TextChangedMode            text_changed_mode)
{
    g_autoptr (GList) tag_info_keys = NULL;
    TagData *tag_data;
    GList *l;
    GList *intersecting_tags = NULL;
    gint tag_end_position;

    tag_info_keys = g_hash_table_get_keys (self->tag_info_table);
    for (l = tag_info_keys; l != NULL; l = l->next)
    {
        g_autofree gchar *tag_text_representation = NULL;

        tag_data = g_hash_table_lookup (self->tag_info_table, l->data);
        tag_text_representation = batch_rename_get_tag_text_representation (tag_data->tag_constants);
        tag_end_position = tag_data->position + g_utf8_strlen (tag_text_representation, -1);
        if (tag_data->set && !tag_data->just_added)
        {
            gboolean selection_intersects_tag_start;
            gboolean selection_intersects_tag_end;
            gboolean tag_is_contained_in_selection;

            if (text_changed_mode == TEXT_WAS_DELETED)
            {
                selection_intersects_tag_start = end_position > tag_data->position &&
                                                 end_position <= tag_end_position;
                selection_intersects_tag_end = start_position >= tag_data->position &&
                                               start_position < tag_end_position;
                tag_is_contained_in_selection = start_position <= tag_data->position &&
                                                end_position >= tag_end_position;
            }
            else
            {
                selection_intersects_tag_start = start_position > tag_data->position &&
                                                 start_position < tag_end_position;
                selection_intersects_tag_end = FALSE;
                tag_is_contained_in_selection = FALSE;
            }
            if (selection_intersects_tag_end || selection_intersects_tag_start || tag_is_contained_in_selection)
            {
                intersecting_tags = g_list_prepend (intersecting_tags, tag_data);
            }
        }
    }

    return g_list_sort (intersecting_tags, compare_tag_position);
}

static void
update_tags_positions (NautilusBatchRenameDialog *self,
                       gint                       start_position,
                       gint                       end_position,
                       TextChangedMode            text_changed_mode)
{
    g_autoptr (GList) tag_info_keys = NULL;
    TagData *tag_data;
    GList *l;

    tag_info_keys = g_hash_table_get_keys (self->tag_info_table);
    for (l = tag_info_keys; l != NULL; l = l->next)
    {
        tag_data = g_hash_table_lookup (self->tag_info_table, l->data);
        if (tag_data->set && !tag_data->just_added && tag_data->position >= start_position)
        {
            if (text_changed_mode == TEXT_WAS_DELETED)
            {
                tag_data->position -= end_position - start_position;
            }
            else
            {
                tag_data->position += end_position - start_position;
            }
        }
    }
}

static void
on_delete_text (GtkEditable *editable,
                gint         start_position,
                gint         end_position,
                gpointer     user_data)
{
    NautilusBatchRenameDialog *self;
    g_autoptr (GList) intersecting_tags = NULL;
    gint final_start_position;
    gint final_end_position;
    GList *l;

    self = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
    intersecting_tags = get_tags_intersecting_sorted (self, start_position,
                                                      end_position, TEXT_WAS_DELETED);
    if (intersecting_tags)
    {
        gint last_tag_end_position;
        g_autofree gchar *tag_text_representation = NULL;
        TagData *first_tag = g_list_first (intersecting_tags)->data;
        TagData *last_tag = g_list_last (intersecting_tags)->data;

        tag_text_representation = batch_rename_get_tag_text_representation (last_tag->tag_constants);
        last_tag_end_position = last_tag->position +
                                g_utf8_strlen (tag_text_representation, -1);
        final_start_position = MIN (start_position, first_tag->position);
        final_end_position = MAX (end_position, last_tag_end_position);
    }
    else
    {
        final_start_position = start_position;
        final_end_position = end_position;
    }

    g_signal_handlers_block_by_func (editable, (gpointer) on_delete_text, user_data);
    gtk_editable_delete_text (editable, final_start_position, final_end_position);
    g_signal_handlers_unblock_by_func (editable, (gpointer) on_delete_text, user_data);

    /* Mark the tags as removed */
    for (l = intersecting_tags; l != NULL; l = l->next)
    {
        remove_tag (self, l->data);
    }

    /* If we removed the numbering tag, we want to enable all numbering actions */
    if (!numbering_tag_is_some_added (self))
    {
        guint i;

        for (i = 0; i < G_N_ELEMENTS (numbering_tags_constants); i++)
        {
            enable_action (self, numbering_tags_constants[i].action_name);
        }
    }

    update_tags_positions (self, final_start_position,
                           final_end_position, TEXT_WAS_DELETED);
    update_display_text (self);

    g_signal_stop_emission_by_name (editable, "delete-text");
}

static void
on_insert_text (GtkEditable *editable,
                const gchar *new_text,
                gint         new_text_length,
                gpointer     position,
                gpointer     user_data)
{
    NautilusBatchRenameDialog *self;
    gint start_position;
    gint end_position;
    g_autoptr (GList) intersecting_tags = NULL;

    self = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
    start_position = *(int *) position;
    end_position = start_position + g_utf8_strlen (new_text, -1);
    intersecting_tags = get_tags_intersecting_sorted (self, start_position,
                                                      end_position, TEXT_WAS_INSERTED);
    if (!intersecting_tags)
    {
        g_signal_handlers_block_by_func (editable, (gpointer) on_insert_text, user_data);
        gtk_editable_insert_text (editable, new_text, new_text_length, position);
        g_signal_handlers_unblock_by_func (editable, (gpointer) on_insert_text, user_data);

        update_tags_positions (self, start_position, end_position, TEXT_WAS_INSERTED);
        update_display_text (self);
    }

    g_signal_stop_emission_by_name (editable, "insert-text");
}

static void
file_names_widget_entry_on_changed (NautilusBatchRenameDialog *self)
{
    update_display_text (self);
}

static void
nautilus_batch_rename_dialog_finalize (GObject *object)
{
    NautilusBatchRenameDialog *dialog;
    GList *l;
    guint i;

    dialog = NAUTILUS_BATCH_RENAME_DIALOG (object);

    if (dialog->directories_pending_conflict_check != NULL)
    {
        cancel_conflict_check (dialog);
    }

    g_list_free (dialog->listbox_labels_new);
    g_list_free (dialog->listbox_labels_old);
    g_list_free (dialog->listbox_icons);

    for (l = dialog->selection_metadata; l != NULL; l = l->next)
    {
        FileMetadata *file_metadata;

        file_metadata = l->data;
        for (i = 0; i < G_N_ELEMENTS (file_metadata->metadata); i++)
        {
            if (file_metadata->metadata[i])
            {
                g_string_free (file_metadata->metadata[i], TRUE);
            }
        }

        g_string_free (file_metadata->file_name, TRUE);
        g_free (file_metadata);
    }

    if (dialog->create_date != NULL)
    {
        g_hash_table_destroy (dialog->create_date);
    }

    g_list_free_full (dialog->new_names, string_free);
    g_list_free_full (dialog->duplicates, conflict_data_free);

    nautilus_file_list_free (dialog->selection);
    nautilus_directory_unref (dialog->directory);
    nautilus_directory_list_free (dialog->distinct_parent_directories);

    g_object_unref (dialog->size_group);
    g_clear_list (&dialog->motion_controllers, g_object_unref);

    g_hash_table_destroy (dialog->tag_info_table);

    g_cancellable_cancel (dialog->metadata_cancellable);
    g_clear_object (&dialog->metadata_cancellable);

    G_OBJECT_CLASS (nautilus_batch_rename_dialog_parent_class)->finalize (object);
}

static void
nautilus_batch_rename_dialog_class_init (NautilusBatchRenameDialogClass *klass)
{
    GtkWidgetClass *widget_class;
    GObjectClass *oclass;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_batch_rename_dialog_finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-batch-rename-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, grid);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, cancel_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, original_name_listbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, arrow_listbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, result_listbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, name_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, rename_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, find_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, replace_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, mode_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, replace_mode_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, format_mode_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, scrolled_window);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_up);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_down);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_label);

    gtk_widget_class_bind_template_callback (widget_class, file_names_widget_on_activate);
    gtk_widget_class_bind_template_callback (widget_class, file_names_widget_entry_on_changed);
    gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_mode_changed);
    gtk_widget_class_bind_template_callback (widget_class, select_next_conflict_up);
    gtk_widget_class_bind_template_callback (widget_class, select_next_conflict_down);
    gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_on_response);
    gtk_widget_class_bind_template_callback (widget_class, on_insert_text);
    gtk_widget_class_bind_template_callback (widget_class, on_delete_text);
}

GtkWidget *
nautilus_batch_rename_dialog_new (GList             *selection,
                                  NautilusDirectory *directory,
                                  NautilusWindow    *window)
{
    NautilusBatchRenameDialog *dialog;
    GString *dialog_title;
    GList *l;
    gboolean all_targets_are_folders;
    gboolean all_targets_are_regular_files;

    dialog = g_object_new (NAUTILUS_TYPE_BATCH_RENAME_DIALOG, "use-header-bar", TRUE, NULL);

    dialog->selection = nautilus_file_list_copy (selection);
    dialog->directory = nautilus_directory_ref (directory);
    dialog->window = window;

    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (window));

    all_targets_are_folders = TRUE;
    for (l = selection; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_directory (NAUTILUS_FILE (l->data)))
        {
            all_targets_are_folders = FALSE;
            break;
        }
    }

    all_targets_are_regular_files = TRUE;
    for (l = selection; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_regular_file (NAUTILUS_FILE (l->data)))
        {
            all_targets_are_regular_files = FALSE;
            break;
        }
    }

    dialog_title = g_string_new ("");
    if (all_targets_are_folders)
    {
        g_string_append_printf (dialog_title,
                                ngettext ("Rename %d Folder",
                                          "Rename %d Folders",
                                          g_list_length (selection)),
                                g_list_length (selection));
    }
    else if (all_targets_are_regular_files)
    {
        g_string_append_printf (dialog_title,
                                ngettext ("Rename %d File",
                                          "Rename %d Files",
                                          g_list_length (selection)),
                                g_list_length (selection));
    }
    else
    {
        g_string_append_printf (dialog_title,
                                /* To translators: %d is the total number of files and folders.
                                 * Singular case of the string is never used */
                                ngettext ("Rename %d File and Folder",
                                          "Rename %d Files and Folders",
                                          g_list_length (selection)),
                                g_list_length (selection));
    }

    gtk_window_set_title (GTK_WINDOW (dialog), dialog_title->str);

    dialog->distinct_parent_directories = batch_rename_files_get_distinct_parents (selection);

    add_tag (dialog, metadata_tags_constants[ORIGINAL_FILE_NAME]);

    nautilus_batch_rename_dialog_initialize_actions (dialog);

    update_display_text (dialog);

    fill_display_listbox (dialog);

    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);

    g_string_free (dialog_title, TRUE);

    return GTK_WIDGET (dialog);
}

static void
connect_to_pointer_motion_events (NautilusBatchRenameDialog *self,
                                  GtkWidget                 *listbox)
{
    GtkEventController *controller;

    controller = gtk_event_controller_motion_new (listbox);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    g_signal_connect (controller, "leave",
                      G_CALLBACK (on_event_controller_motion_leave), self);
    g_signal_connect (controller, "motion",
                      G_CALLBACK (on_event_controller_motion_motion), self);

    self->motion_controllers = g_list_prepend (self->motion_controllers,
                                               controller);
}

static void
nautilus_batch_rename_dialog_init (NautilusBatchRenameDialog *self)
{
    TagData *tag_data;
    guint i;

    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_list_box_set_header_func (GTK_LIST_BOX (self->original_name_listbox),
                                  (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                  self,
                                  NULL);
    gtk_list_box_set_header_func (GTK_LIST_BOX (self->arrow_listbox),
                                  (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                  self,
                                  NULL);
    gtk_list_box_set_header_func (GTK_LIST_BOX (self->result_listbox),
                                  (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                  self,
                                  NULL);


    self->mode = NAUTILUS_BATCH_RENAME_DIALOG_FORMAT;

    gtk_label_set_ellipsize (GTK_LABEL (self->conflict_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (self->conflict_label), 1);

    self->duplicates = NULL;
    self->distinct_parent_directories = NULL;
    self->directories_pending_conflict_check = NULL;
    self->new_names = NULL;
    self->rename_clicked = FALSE;

    self->tag_info_table = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) g_free);

    for (i = 0; i < G_N_ELEMENTS (numbering_tags_constants); i++)
    {
        g_autofree gchar *tag_text_representation = NULL;

        tag_text_representation = batch_rename_get_tag_text_representation (numbering_tags_constants[i]);
        tag_data = g_new (TagData, 1);
        tag_data->available = TRUE;
        tag_data->set = FALSE;
        tag_data->position = -1;
        tag_data->tag_constants = numbering_tags_constants[i];
        g_hash_table_insert (self->tag_info_table, g_strdup (tag_text_representation), tag_data);
    }

    for (i = 0; i < G_N_ELEMENTS (metadata_tags_constants); i++)
    {
        g_autofree gchar *tag_text_representation = NULL;

        /* Only the original name is available and set at the start */
        tag_text_representation = batch_rename_get_tag_text_representation (metadata_tags_constants[i]);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = -1;
        tag_data->tag_constants = metadata_tags_constants[i];
        g_hash_table_insert (self->tag_info_table, g_strdup (tag_text_representation), tag_data);
    }

    self->row_height = -1;

    g_signal_connect (self->original_name_listbox, "row-selected", G_CALLBACK (row_selected), self);
    g_signal_connect (self->arrow_listbox, "row-selected", G_CALLBACK (row_selected), self);
    g_signal_connect (self->result_listbox, "row-selected", G_CALLBACK (row_selected), self);

    self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    connect_to_pointer_motion_events (self, self->original_name_listbox);
    connect_to_pointer_motion_events (self, self->result_listbox);
    connect_to_pointer_motion_events (self, self->arrow_listbox);

    self->metadata_cancellable = g_cancellable_new ();
}
