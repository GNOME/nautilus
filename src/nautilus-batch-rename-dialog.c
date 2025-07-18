/* nautilus-batch-rename-dialog.c
 *
 * Copyright (C) 2016 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 * Copyright (C) 2024–2025 Markus Göllnitz <camelcasenick@bewares.it>
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
#include "nautilus-batch-rename-item.h"
#include "nautilus-directory.h"
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
    AdwDialog parent;

    GtkRoot *window;

    AdwBreakpoint *narrow_breakpoint;

    AdwToolbarView *toolbar_view;
    GtkWidget *batch_listview;
    GtkWidget *name_entry;
    GtkWidget *rename_button;
    GtkWidget *find_entry;
    GtkWidget *mode_stack;
    GtkWidget *replace_entry;
    GtkWidget *format_mode_button;
    GtkWidget *numbering_order_button;
    GtkWidget *numbering_revealer;
    GtkWidget *conflict_label;
    GtkWidget *conflict_down;
    GtkWidget *conflict_up;

    GListStore *batch_listmodel;

    GList *selection;
    GList *new_names;
    NautilusBatchRenameDialogMode mode;

    GActionGroup *action_group;

    GMenu *numbering_order_menu;

    GHashTable *create_date;
    GHashTable *selection_metadata;

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

G_DEFINE_TYPE (NautilusBatchRenameDialog, nautilus_batch_rename_dialog, ADW_TYPE_DIALOG);

static void
change_numbering_order (GSimpleAction *action,
                        GVariant      *value,
                        gpointer       user_data)
{
    NautilusBatchRenameDialog *dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
    g_autoptr (GVariant) current_state = g_action_get_state (G_ACTION (action));
    const gchar *current_target_name = g_variant_get_string (current_state, NULL);
    const gchar *target_name = g_variant_get_string (value, NULL);

    if (g_str_equal (current_target_name, target_name))
    {
        return;
    }

    for (guint i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (g_strcmp0 (sorts_constants[i].action_target_name, target_name) == 0)
        {
            gtk_menu_button_set_label (GTK_MENU_BUTTON (dialog->numbering_order_button),
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
    const gchar *tag_text_representation;
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
     * We need to check the cursor movement and update the actions accordingly or
     * even better add the tag at the end of the previous tag if this happens.
     */
    gtk_editable_insert_text (GTK_EDITABLE (self->name_entry),
                              tag_text_representation,
                              strlen (tag_text_representation),
                              &cursor_position);
    tag_data->just_added = FALSE;
    gtk_editable_set_position (GTK_EDITABLE (self->name_entry), cursor_position);

    if (gtk_widget_get_root (GTK_WIDGET (self)) != NULL)
    {
        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->name_entry));
    }
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
    {
        .name = "numbering-order-changed", .parameter_type = "s", .state = "'name-ascending'",
        .change_state = change_numbering_order
    },
    { .name = "add-numbering-no-zero-pad-tag", .activate = add_numbering_tag },
    { .name = "add-numbering-one-zero-pad-tag", .activate = add_numbering_tag },
    { .name = "add-numbering-two-zero-pad-tag", .activate = add_numbering_tag },
    { .name = "add-original-file-name-tag", .activate = add_metadata_tag },
    { .name = "add-creation-date-tag", .activate = add_metadata_tag },
    { .name = "add-equipment-tag", .activate = add_metadata_tag },
    { .name = "add-season-number-tag", .activate = add_metadata_tag },
    { .name = "add-episode-number-tag", .activate = add_metadata_tag },
    { .name = "add-video-album-tag", .activate = add_metadata_tag },
    { .name = "add-track-number-tag", .activate = add_metadata_tag },
    { .name = "add-artist-name-tag", .activate = add_metadata_tag },
    { .name = "add-title-tag", .activate = add_metadata_tag },
    { .name = "add-album-name-tag", .activate = add_metadata_tag },
};

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
            const gchar *tag_text_representation;

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

    gtk_widget_set_cursor (GTK_WIDGET (dialog->window), NULL);
}

static void
prepare_batch_rename (NautilusBatchRenameDialog *dialog)
{
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

    gtk_widget_set_cursor_from_name (GTK_WIDGET (dialog->window), "progress");

    gtk_widget_set_cursor_from_name (GTK_WIDGET (dialog), "progress");

    begin_batch_rename (dialog, dialog->new_names);

    adw_dialog_close (ADW_DIALOG (dialog));
}

static void
batch_rename_dialog_on_cancel (NautilusBatchRenameDialog *dialog,
                               gpointer                   user_data)
{
    if (dialog->directories_pending_conflict_check != NULL)
    {
        cancel_conflict_check (dialog);
    }

    adw_dialog_close (ADW_DIALOG (dialog));
}

static void
fill_display_listbox (NautilusBatchRenameDialog *dialog)
{
    guint items_size = g_list_length (dialog->new_names);
    g_autoptr (GPtrArray) item_array = g_ptr_array_new_full (items_size, g_object_unref);
    GList *l1;
    GList *l2;
    guint i;

    for (i = 0, l1 = dialog->new_names, l2 = dialog->selection; i < items_size; i++, l1 = l1->next, l2 = l2->next)
    {
        g_autofree gchar *name = g_markup_escape_text (nautilus_file_get_name (NAUTILUS_FILE (l2->data)), -1);
        g_autofree gchar *new_name = g_markup_escape_text (((GString *) l1->data)->str, -1);

        g_ptr_array_add (item_array, nautilus_batch_rename_item_new (name, new_name, dialog));
    }

    g_list_store_splice (dialog->batch_listmodel, 0, 0, item_array->pdata, item_array->len);
}

static void
select_nth_conflict (NautilusBatchRenameDialog *dialog)
{
    GList *l;
    GString *conflict_file_name;
    GString *display_text;
    GString *new_name;
    guint nth_conflict_index;
    gint nth_conflict;
    gint name_occurences;
    ConflictData *conflict_data;

    nth_conflict = dialog->selected_conflict;
    l = g_list_nth (dialog->duplicates, nth_conflict);
    conflict_data = l->data;

    /* the conflict that has to be selected */
    conflict_file_name = g_string_new (conflict_data->name);
    display_text = g_string_new ("");

    nth_conflict_index = conflict_data->index;

    gtk_list_view_scroll_to (GTK_LIST_VIEW (dialog->batch_listview),
                             nth_conflict_index,
                             GTK_LIST_SCROLL_SELECT, NULL);

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
    GList *duplicates;
    GListModel *model = G_LIST_MODEL (dialog->batch_listmodel);
    guint model_size = g_list_model_get_n_items (model);

    duplicates = dialog->duplicates;

    for (guint index = 0; index < model_size; index++)
    {
        g_autoptr (NautilusBatchRenameItem) item = g_list_model_get_item (model, index);

        if (duplicates != NULL)
        {
            ConflictData *conflict_data = duplicates->data;
            gboolean has_conflict = conflict_data->index == index;
            nautilus_batch_rename_item_set_has_conflict (item, has_conflict);
            if (has_conflict)
            {
                duplicates = duplicates->next;
            }
        }
        else
        {
            nautilus_batch_rename_item_set_has_conflict (item, FALSE);
        }
    }
}

static void
update_listbox (NautilusBatchRenameDialog *dialog)
{
    guint i;
    GList *l1;
    GList *l2;
    gboolean empty_name = FALSE;

    for (i = 0, l1 = dialog->new_names, l2 = dialog->selection;
         i < g_list_model_get_n_items (G_LIST_MODEL (dialog->batch_listmodel));
         i++, l1 = l1->next, l2 = l2->next)
    {
        g_autoptr (NautilusBatchRenameItem) item = g_list_model_get_item (G_LIST_MODEL (dialog->batch_listmodel), i);
        GString *new_name = l1->data;
        const char *old_name = nautilus_file_get_name (NAUTILUS_FILE (l2->data));
        g_autofree gchar *new_name_escaped = g_markup_escape_text (new_name->str, -1);

        nautilus_batch_rename_item_set_name_after (item, new_name_escaped);

        if (g_strcmp0 (new_name->str, "") == 0)
        {
            empty_name = TRUE;
        }

        if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT)
        {
            g_autofree gchar *old_name_escaped = g_markup_escape_text (old_name, -1);
            nautilus_batch_rename_item_set_name_before (item, old_name_escaped);
        }
        else
        {
            const gchar *replaced_text = gtk_editable_get_text (GTK_EDITABLE (dialog->find_entry));
            g_autoptr (GString) highlighted_name = markup_hightlight_text (old_name, replaced_text,
                                                                           "white", "#f57900");

            nautilus_batch_rename_item_set_name_before (item, highlighted_name->str);
        }
    }

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

        adw_toolbar_view_set_reveal_bottom_bars (dialog->toolbar_view, TRUE);

        dialog->selected_conflict = 0;
        dialog->conflicts_number = g_list_length (dialog->duplicates);

        select_nth_conflict (dialog);

        gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

        if (dialog->duplicates != NULL && dialog->duplicates->next == NULL)
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
        adw_toolbar_view_set_reveal_bottom_bars (dialog->toolbar_view, FALSE);

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
    GList *l1, *l2;
    guint index = 0;
    GHashTable *directory_files_table;
    GHashTable *new_names_table;
    GHashTable *names_conflicts_table;
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
        GString *new_name = l1->data;
        NautilusFile *file = NAUTILUS_FILE (l2->data);
        g_autofree gchar *parent_uri = nautilus_file_get_parent_uri (file);

        gboolean same_parent_directory = g_strcmp0 (parent_uri, current_directory) == 0;

        if (same_parent_directory)
        {
            gboolean tag_present = g_hash_table_contains (new_names_table, new_name->str);

            if (!tag_present)
            {
                g_hash_table_insert (new_names_table,
                                     g_strdup (new_name->str),
                                     g_steal_pointer (&parent_uri));
            }
            else
            {
                g_hash_table_insert (names_conflicts_table,
                                     g_strdup (new_name->str),
                                     g_steal_pointer (&parent_uri));
            }
        }
    }

    for (l1 = files; l1 != NULL; l1 = l1->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l1->data);
        g_hash_table_add (directory_files_table, g_strdup (nautilus_file_get_name (file)));
    }

    for (index = 0, l1 = dialog->selection, l2 = dialog->new_names;
         l1 != NULL && l2 != NULL;
         index++, l1 = l1->next, l2 = l2->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l1->data);
        GString *new_name = l2->data;
        const gchar *file_name = nautilus_file_get_name (file);
        g_autofree gchar *parent_uri = nautilus_file_get_parent_uri (file);
        gboolean have_conflict = FALSE;

        /* check for duplicate only if the parent of the current file is
         * the current directory and the name of the file has changed */
        if (g_strcmp0 (parent_uri, current_directory) == 0 &&
            !g_str_equal (new_name->str, file_name))
        {
            gboolean exists = g_hash_table_contains (directory_files_table, new_name->str);

            if (exists == TRUE &&
                !file_name_conflicts_with_results (dialog->selection, dialog->new_names, new_name, parent_uri))
            {
                conflict_data = g_new (ConflictData, 1);
                conflict_data->name = g_strdup (new_name->str);
                conflict_data->index = index;
                dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                     conflict_data);

                have_conflict = TRUE;
            }
        }

        if (!have_conflict)
        {
            gboolean tag_present = g_hash_table_lookup (names_conflicts_table, new_name->str) != NULL;

            if (tag_present &&
                g_strcmp0 (parent_uri, current_directory) == 0)
            {
                conflict_data = g_new (ConflictData, 1);
                conflict_data->name = g_strdup (new_name->str);
                conflict_data->index = index;
                dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                     conflict_data);

                have_conflict = TRUE;
            }
        }
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

    if (strchr (entry_text, '/') != NULL)
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

        adw_toolbar_view_set_reveal_bottom_bars (dialog->toolbar_view, TRUE);

        return TRUE;
    }
    else
    {
        adw_toolbar_view_set_reveal_bottom_bars (dialog->toolbar_view, FALSE);

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
        const gchar *tag_text_representation;

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
    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->format_mode_button)))
    {
        gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "format");

        dialog->mode = NAUTILUS_BATCH_RENAME_DIALOG_FORMAT;

        if (gtk_widget_get_root (GTK_WIDGET (dialog)) != NULL)
        {
            gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));
        }
    }
    else
    {
        gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "replace");

        dialog->mode = NAUTILUS_BATCH_RENAME_DIALOG_REPLACE;

        if (gtk_widget_get_root (GTK_WIDGET (dialog)) != NULL)
        {
            gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->find_entry));
        }
    }

    update_display_text (dialog);
}

void
nautilus_batch_rename_dialog_query_finished (NautilusBatchRenameDialog *dialog,
                                             GHashTable                *hash_table,
                                             GHashTable                *selection_metadata,
                                             gboolean                   has_metadata[])
{
    GHashTableIter selection_metadata_iter;
    FileMetadata *file_metadata;
    MetadataType metadata_type;
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
        g_autoptr (GMenuItem) first_created = NULL, last_created = NULL;

        first_created = g_menu_item_new ("First Created",
                                         "dialog.numbering-order-changed('first-created')");

        g_menu_append_item (dialog->numbering_order_menu, first_created);

        last_created = g_menu_item_new ("Last Created",
                                        "dialog.numbering-order-changed('last-created')");

        g_menu_append_item (dialog->numbering_order_menu, last_created);
    }

    dialog->selection_metadata = selection_metadata;

    g_hash_table_iter_init (&selection_metadata_iter, selection_metadata);
    g_hash_table_iter_next (&selection_metadata_iter, NULL, (gpointer *) &file_metadata);

    tag_info_keys = g_hash_table_get_keys (dialog->tag_info_table);
    for (l = tag_info_keys; l != NULL; l = l->next)
    {
        /* Only metadata has to be handled here. */
        TagData *tag_data = g_hash_table_lookup (dialog->tag_info_table, l->data);
        gboolean is_metadata = tag_data->tag_constants.is_metadata;

        if (!is_metadata)
        {
            continue;
        }

        metadata_type = tag_data->tag_constants.metadata_type;
        if (has_metadata[metadata_type] == FALSE ||
            file_metadata->metadata[metadata_type] == NULL ||
            file_metadata->metadata[metadata_type]->len <= 0)
        {
            disable_action (dialog, tag_data->tag_constants.action_name);
            tag_data->available = FALSE;
        }
    }
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
        const gchar *tag_text_representation;

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
                                                 end_position <= tag_end_position &&
                                                 start_position <= tag_data->position;
                selection_intersects_tag_end = start_position >= tag_data->position &&
                                               start_position < tag_end_position &&
                                               end_position >= tag_end_position;
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
        const gchar *tag_text_representation;
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

static GStrv
batch_row_conflict_css_name (GtkListItem *item,
                             gboolean     has_conflict)
{
    static gchar *css_names_has_conflict[2] = { "conflict-row", NULL };

    return has_conflict ? g_strdupv (css_names_has_conflict) : NULL;
}

static GtkOrientation
batch_row_orientation (GtkListItem               *item,
                       AdwBreakpoint             *current_breakpoint,
                       NautilusBatchRenameDialog *dialog)
{
    if (current_breakpoint == dialog->narrow_breakpoint)
    {
        return GTK_ORIENTATION_VERTICAL;
    }
    else
    {
        return GTK_ORIENTATION_HORIZONTAL;
    }
}

static gboolean
nautilus_batch_rename_dialog_grab_focus (GtkWidget *widget)
{
    NautilusBatchRenameDialog *dialog = NAUTILUS_BATCH_RENAME_DIALOG (widget);

    if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
    {
        return gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->find_entry));
    }
    else
    {
        return gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));
    }
}

static void
nautilus_batch_rename_dialog_dispose (GObject *object)
{
    NautilusBatchRenameDialog *dialog = NAUTILUS_BATCH_RENAME_DIALOG (object);

    gtk_widget_dispose_template (GTK_WIDGET (dialog), NAUTILUS_TYPE_BATCH_RENAME_DIALOG);

    G_OBJECT_CLASS (nautilus_batch_rename_dialog_parent_class)->dispose (object);
}

static void
nautilus_batch_rename_dialog_finalize (GObject *object)
{
    NautilusBatchRenameDialog *dialog;

    dialog = NAUTILUS_BATCH_RENAME_DIALOG (object);

    if (dialog->directories_pending_conflict_check != NULL)
    {
        cancel_conflict_check (dialog);
    }

    g_clear_pointer (&dialog->selection_metadata, g_hash_table_unref);

    if (dialog->create_date != NULL)
    {
        g_hash_table_destroy (dialog->create_date);
    }

    g_list_free_full (dialog->new_names, string_free);
    g_list_free_full (dialog->duplicates, conflict_data_free);

    nautilus_file_list_free (dialog->selection);
    nautilus_directory_list_free (dialog->distinct_parent_directories);

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

    oclass->dispose = nautilus_batch_rename_dialog_dispose;
    oclass->finalize = nautilus_batch_rename_dialog_finalize;

    widget_class->grab_focus = nautilus_batch_rename_dialog_grab_focus;

    g_type_ensure (NAUTILUS_TYPE_BATCH_RENAME_ITEM);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-batch-rename-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, toolbar_view);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, batch_listview);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, batch_listmodel);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, name_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, rename_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, find_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, replace_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, mode_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, format_mode_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_up);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_down);
    gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, narrow_breakpoint);

    gtk_widget_class_bind_template_callback (widget_class, file_names_widget_on_activate);
    gtk_widget_class_bind_template_callback (widget_class, file_names_widget_entry_on_changed);
    gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_mode_changed);
    gtk_widget_class_bind_template_callback (widget_class, select_next_conflict_up);
    gtk_widget_class_bind_template_callback (widget_class, select_next_conflict_down);
    gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_on_cancel);
    gtk_widget_class_bind_template_callback (widget_class, prepare_batch_rename);
    gtk_widget_class_bind_template_callback (widget_class, batch_row_conflict_css_name);
    gtk_widget_class_bind_template_callback (widget_class, batch_row_orientation);
}

/**
 * nautilus_batch_rename_dialog_new:
 * @selections: (element-type NautilusFile*) (transfer full): a list of files to rename.
 * @window: Parent root widget to change its cursor on during active operation.
 *
 * Returns: (transfer full): A dialog widget ready to present.
 */
GtkWidget *
nautilus_batch_rename_dialog_new (GList   *selection,
                                  GtkRoot *window)
{
    NautilusBatchRenameDialog *dialog;
    GString *dialog_title;
    GList *l;
    gboolean all_targets_are_folders;
    gboolean all_targets_are_regular_files;

    dialog = g_object_new (NAUTILUS_TYPE_BATCH_RENAME_DIALOG, NULL);

    dialog->selection = selection;
    dialog->window = window;

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
    guint selection_count = g_list_length (selection);
    if (all_targets_are_folders)
    {
        g_string_append_printf (dialog_title,
                                ngettext ("Rename %d Folder",
                                          "Rename %d Folders",
                                          selection_count),
                                selection_count);
    }
    else if (all_targets_are_regular_files)
    {
        g_string_append_printf (dialog_title,
                                ngettext ("Rename %d File",
                                          "Rename %d Files",
                                          selection_count),
                                selection_count);
    }
    else
    {
        g_string_append_printf (dialog_title,
                                /* To translators: %d is the total number of files and folders.
                                 * Singular case of the string is never used */
                                ngettext ("Rename %d File and Folder",
                                          "Rename %d Files and Folders",
                                          selection_count),
                                selection_count);
    }

    adw_dialog_set_title (ADW_DIALOG (dialog), dialog_title->str);

    dialog->distinct_parent_directories = batch_rename_files_get_distinct_parents (selection);

    add_tag (dialog, metadata_tags_constants[ORIGINAL_FILE_NAME]);

    nautilus_batch_rename_dialog_initialize_actions (dialog);

    fill_display_listbox (dialog);

    gtk_widget_set_cursor (GTK_WIDGET (window), NULL);

    g_string_free (dialog_title, TRUE);

    return GTK_WIDGET (dialog);
}

static void
nautilus_batch_rename_dialog_init (NautilusBatchRenameDialog *self)
{
    TagData *tag_data;
    guint i;

    gtk_widget_init_template (GTK_WIDGET (self));

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
        const gchar *tag_text_representation;

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
        const gchar *tag_text_representation;

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

    g_signal_connect_object (gtk_editable_get_delegate (GTK_EDITABLE (self->name_entry)),
                             "delete-text", G_CALLBACK (on_delete_text), self, 0);
    g_signal_connect_object (gtk_editable_get_delegate (GTK_EDITABLE (self->name_entry)),
                             "insert-text", G_CALLBACK (on_insert_text), self, 0);

    self->metadata_cancellable = g_cancellable_new ();
}
