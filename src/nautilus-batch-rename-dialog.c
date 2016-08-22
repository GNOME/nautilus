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

#define ADD_TEXT_ENTRY_SIZE 550
#define REPLACE_ENTRY_SIZE  275
#define TAG_UNAVAILABLE -2
#define HAVE_CONFLICT 1

struct _NautilusBatchRenameDialog
{
        GtkDialog                        parent;

        GtkWidget                       *grid;
        NautilusWindow                  *window;

        GtkWidget                       *cancel_button;
        GtkWidget                       *original_name_listbox;
        GtkWidget                       *arrow_listbox;
        GtkWidget                       *result_listbox;
        GtkWidget                       *name_entry;
        GtkWidget                       *rename_button;
        GtkWidget                       *find_entry;
        GtkWidget                       *mode_stack;
        GtkWidget                       *replace_entry;
        GtkWidget                       *format_mode_button;
        GtkWidget                       *replace_mode_button;
        GtkWidget                       *add_button;
        GtkWidget                       *add_popover;
        GtkWidget                       *numbering_order_label;
        GtkWidget                       *numbering_label;
        GtkWidget                       *scrolled_window;
        GtkWidget                       *numbering_order_popover;
        GtkWidget                       *numbering_order_button;
        GtkWidget                       *conflict_box;
        GtkWidget                       *conflict_label;
        GtkWidget                       *conflict_down;
        GtkWidget                       *conflict_up;

        GList                           *original_name_listbox_rows;
        GList                           *arrow_listbox_rows;
        GList                           *result_listbox_rows;
        GList                           *listbox_labels_new;
        GList                           *listbox_labels_old;
        GList                           *listbox_icons;
        GtkSizeGroup                    *size_group;

        GList                           *selection;
        GList                           *new_names;
        NautilusBatchRenameDialogMode    mode;
        NautilusDirectory               *directory;

        GActionGroup                    *action_group;

        GMenu                           *numbering_order_menu;
        GMenu                           *add_tag_menu;

        GHashTable                      *create_date;
        GList                           *selection_metadata;

        /* check if all files in selection have the same parent */
        gboolean                         same_parent;
        /* the index of the currently selected conflict */
        gint                             selected_conflict;
        /* total conflicts number */
        gint                             conflicts_number;

        gint                             checked_parents;
        GList                           *duplicates;
        GList                           *distinct_parents;
        GTask                           *conflicts_task;
        GCancellable                    *conflict_cancellable;
        gboolean                         checking_conflicts;

        /* this hash table has information about the status
         * of all tags: availability, if it's currently used
         * and position */
        GHashTable                      *tag_info_table;

        GtkWidget                       *preselected_row1;
        GtkWidget                       *preselected_row2;

        gint                             row_height;
        gboolean                         rename_clicked;
};

typedef struct
{
        gboolean available;
        gboolean set;
        gint position;
} TagData;

static void     update_display_text     (NautilusBatchRenameDialog *dialog);

G_DEFINE_TYPE (NautilusBatchRenameDialog, nautilus_batch_rename_dialog, GTK_TYPE_DIALOG);

static void
add_numbering_order (GSimpleAction       *action,
                     GVariant            *value,
                     gpointer             user_data)
{
        NautilusBatchRenameDialog *dialog;
        const gchar *target_name;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        target_name = g_variant_get_string (value, NULL);

        if (g_strcmp0 (target_name, "name-ascending") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     _("Original name (Ascending)"));
                dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                       ORIGINAL_ASCENDING,
                                                                       NULL);
        }

        if (g_strcmp0 (target_name, "name-descending") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     _("Original name (Descending)"));
                dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                       ORIGINAL_DESCENDING,
                                                                       NULL);
        }

        if (g_strcmp0 (target_name, "first-modified") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     _("First Modified"));
                dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                       FIRST_MODIFIED,
                                                                       NULL);
        }

        if (g_strcmp0 (target_name, "last-modified") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     _("Last Modified"));
                dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                       LAST_MODIFIED,
                                                                       NULL);
        }

        if (g_strcmp0 (target_name, "first-created") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     _("First Created"));
                dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                       FIRST_CREATED,
                                                                       dialog->create_date);
        }

        if (g_strcmp0 (target_name, "last-created") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     _("Last Created"));
                dialog->selection = nautilus_batch_rename_dialog_sort (dialog->selection,
                                                                       LAST_CREATED,
                                                                       dialog->create_date);
        }

        g_simple_action_set_state (action, value);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->numbering_order_button), FALSE);

        update_display_text (dialog);
}

static void
add_original_file_name_tag (GSimpleAction       *action,
                            GVariant            *value,
                            gpointer             user_data)
{
        NautilusBatchRenameDialog *dialog;
        gint cursor_position;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        g_object_get (dialog->name_entry, "cursor-position", &cursor_position, NULL);

        gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                  ORIGINAL_FILE_NAME,
                                  strlen (ORIGINAL_FILE_NAME),
                                  &cursor_position);

        gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));

        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
disable_action (NautilusBatchRenameDialog *dialog,
                gchar                     *action_name)
{
        GAction *action;

        action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                             action_name);
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
add_metadata_tag (GSimpleAction       *action,
                  GVariant            *value,
                  gpointer             user_data)
{
        NautilusBatchRenameDialog *dialog;
        const gchar *action_name;
        gint cursor_position;
        TagData *tag_data;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        action_name = g_action_get_name (G_ACTION (action));
        g_object_get (dialog->name_entry, "cursor-position", &cursor_position, NULL);

        if (g_strrstr (action_name, "creation-date")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, CREATION_DATE);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          CREATION_DATE,
                                          strlen (CREATION_DATE),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-creation-date-tag");
        }

        if (g_strrstr (action_name, "equipment")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, CAMERA_MODEL);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          CAMERA_MODEL,
                                          strlen (CAMERA_MODEL),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-equipment-tag");
        }

        if (g_strrstr (action_name, "season")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, SEASON_NUMBER);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          SEASON_NUMBER,
                                          strlen (SEASON_NUMBER),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-season-tag");
        }

        if (g_strrstr (action_name, "episode")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, EPISODE_NUMBER);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          EPISODE_NUMBER,
                                          strlen (EPISODE_NUMBER),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-episode-tag");
        }

        if (g_strrstr (action_name, "track")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, TRACK_NUMBER);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          TRACK_NUMBER,
                                          strlen (TRACK_NUMBER),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-track-number-tag");
        }

        if (g_strrstr (action_name, "artist")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, ARTIST_NAME);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          ARTIST_NAME,
                                          strlen (ARTIST_NAME),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-artist-name-tag");
        }

        if (g_strrstr (action_name, "title")) {
                tag_data = g_hash_table_lookup (dialog->tag_info_table, TITLE);
                tag_data->available = TRUE;
                tag_data->set = FALSE;

                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          TITLE,
                                          strlen (TITLE),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
                disable_action (dialog, "add-title-tag");
        }

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));
}

static void
add_numbering_tag (GSimpleAction       *action,
                   GVariant            *value,
                   gpointer             user_data)
{
        NautilusBatchRenameDialog *dialog;
        const gchar *action_name;
        gint cursor_position;
        GAction *add_numbering_action;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        action_name = g_action_get_name (G_ACTION (action));
        g_object_get (dialog->name_entry, "cursor-position", &cursor_position, NULL);

        if (g_strrstr (action_name, "zero")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          NUMBERING,
                                          strlen (NUMBERING),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
        }

        if (g_strrstr (action_name, "one")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          NUMBERING0,
                                          strlen (NUMBERING0),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
        }

        if (g_strrstr (action_name, "two")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          NUMBERING00,
                                          strlen (NUMBERING00),
                                          &cursor_position);
                gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), cursor_position);
        }

        add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                           "add-numbering-tag-zero");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);
        add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                           "add-numbering-tag-one");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

        add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                           "add-numbering-tag-two");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));
}

const GActionEntry dialog_entries[] = {
        { "numbering-order-changed", NULL, "s", "'name-ascending'",  add_numbering_order },
        { "add-original-file-name-tag", add_original_file_name_tag },
        { "add-numbering-tag-zero", add_numbering_tag },
        { "add-numbering-tag-one", add_numbering_tag },
        { "add-numbering-tag-two", add_numbering_tag },
        { "add-creation-date-tag", add_metadata_tag },
        { "add-equipment-tag", add_metadata_tag },
        { "add-season-tag", add_metadata_tag },
        { "add-episode-tag", add_metadata_tag },
        { "add-video-album-tag", add_metadata_tag },
        { "add-track-number-tag", add_metadata_tag },
        { "add-artist-name-tag", add_metadata_tag },
        { "add-title-tag", add_metadata_tag },

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
                return;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);
        index = gtk_list_box_row_get_index (listbox_row);

        if (GTK_WIDGET (box) == dialog->original_name_listbox) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->arrow_listbox), index);
                gtk_list_box_select_row (GTK_LIST_BOX (dialog->arrow_listbox),
                                         row);
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->result_listbox), index);
                gtk_list_box_select_row (GTK_LIST_BOX (dialog->result_listbox),
                                         row);
        }

        if (GTK_WIDGET (box) == dialog->arrow_listbox) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->original_name_listbox), index);
                gtk_list_box_select_row (GTK_LIST_BOX (dialog->original_name_listbox),
                                         row);
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->result_listbox), index);
                gtk_list_box_select_row (GTK_LIST_BOX (dialog->result_listbox),
                                         row);
        }

        if (GTK_WIDGET (box) == dialog->result_listbox) {
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->arrow_listbox), index);
                gtk_list_box_select_row (GTK_LIST_BOX (dialog->arrow_listbox),
                                         row);
                row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->original_name_listbox), index);
                gtk_list_box_select_row (GTK_LIST_BOX (dialog->original_name_listbox),
                                         row);
        }
}

gint compare_int (gconstpointer a,
                  gconstpointer b)
{
        int *number1 = (int*) a;
        int *number2 = (int*) b;

        return *number1 - *number2;
}

/* This function splits the entry text into a list of regular text and tags.
 * For instance, "[1, 2, 3]Paris[Creation date]" would result in:
 * "[1, 2, 3]", "Paris", "[Creation date]" */
static GList*
split_entry_text (NautilusBatchRenameDialog *dialog,
                  gchar                     *entry_text)
{
        GString *string;
        GString *tag;
        GArray *tag_positions;
        gint tags;
        gint i;
        gint tag_end_position;
        GList *result = NULL;
        TagData *tag_data;

        tags = 0;
        tag_end_position = 0;
        tag_positions = g_array_new (FALSE, FALSE, sizeof (gint));

        tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING0);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING00);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, ORIGINAL_FILE_NAME);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, CREATION_DATE);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, CAMERA_MODEL);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, SEASON_NUMBER);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, EPISODE_NUMBER);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, TRACK_NUMBER);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, ARTIST_NAME);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, TITLE);
        if (tag_data->set) {
                g_array_append_val (tag_positions, tag_data->position);
                tags++;
        }

        g_array_sort (tag_positions, compare_int);

        for (i = 0; i < tags; i++) {
                tag = g_string_new ("");

                string = g_string_new ("");

                string = g_string_append_len (string,
                                              entry_text + tag_end_position,
                                              g_array_index (tag_positions, gint, i) - tag_end_position);

                if (g_strcmp0 (string->str, ""))
                        result = g_list_prepend (result, string);

                tag_data = g_hash_table_lookup (dialog->tag_info_table, ORIGINAL_FILE_NAME);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (ORIGINAL_FILE_NAME);
                        tag = g_string_append (tag, ORIGINAL_FILE_NAME);
                }

                tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (NUMBERING);
                        tag = g_string_append (tag, NUMBERING);
                }

                tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING0);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (NUMBERING0);
                        tag = g_string_append (tag, NUMBERING0);
                }

                tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING00);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (NUMBERING00);
                        tag = g_string_append (tag, NUMBERING00);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, CREATION_DATE);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (CREATION_DATE);
                        tag = g_string_append (tag, CREATION_DATE);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, CAMERA_MODEL);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (CAMERA_MODEL);
                        tag = g_string_append (tag, CAMERA_MODEL);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, SEASON_NUMBER);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (SEASON_NUMBER);
                        tag = g_string_append (tag, SEASON_NUMBER);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, EPISODE_NUMBER);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (EPISODE_NUMBER);
                        tag = g_string_append (tag, EPISODE_NUMBER);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, TRACK_NUMBER);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (TRACK_NUMBER);
                        tag = g_string_append (tag, TRACK_NUMBER);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, ARTIST_NAME);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (ARTIST_NAME);
                        tag = g_string_append (tag, ARTIST_NAME);
                }
                tag_data = g_hash_table_lookup (dialog->tag_info_table, TITLE);
                if (g_array_index (tag_positions, gint, i) == tag_data->position && tag_data->set) {
                        tag_end_position = g_array_index (tag_positions, gint, i) +
                                           strlen (TITLE);
                        tag = g_string_append (tag, TITLE);
                }

                result = g_list_prepend (result, tag);
        }
        string = g_string_new ("");
        string = g_string_append (string, entry_text + tag_end_position);

        if (g_strcmp0 (string->str, ""))
                result = g_list_prepend (result, string);

        result = g_list_reverse (result);

        g_array_free (tag_positions, TRUE);
        return result;
}

static GList*
batch_rename_dialog_get_new_names (NautilusBatchRenameDialog *dialog)
{
        GList *result = NULL;
        GList *selection;
        GList *tags_list;
        g_autofree gchar *entry_text;
        g_autofree gchar *replace_text;

        selection = dialog->selection;
        tags_list = NULL;

        if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
                entry_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->find_entry)));
        else
                entry_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

        replace_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry)));

        if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE) {
                result = batch_rename_dialog_get_new_names_list (dialog->mode,
                                                                 selection,
                                                                 NULL,
                                                                 NULL,
                                                                 entry_text,
                                                                 replace_text);
        } else {
                tags_list = split_entry_text (dialog, entry_text);

                result = batch_rename_dialog_get_new_names_list (dialog->mode,
                                                                 selection,
                                                                 tags_list,
                                                                 dialog->selection_metadata,
                                                                 entry_text,
                                                                 replace_text);
                g_list_free_full (tags_list, string_free);
        }

        result = g_list_reverse (result);

        return result;
}

static void
begin_batch_rename_dialog (NautilusBatchRenameDialog *dialog,
                           GList                     *new_names)
{
        GList *l1;
        GList *l2;
        GList *l3;
        gchar *file_name;
        gchar *old_file_name;
        GString *new_file_name;
        GList *new_name;
        NautilusFile *file;

        for (l1 = new_names, l2 = dialog->selection; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                old_file_name = nautilus_file_get_name (NAUTILUS_FILE (l2->data));
                new_file_name = l1->data;

                for (l3 = dialog->selection; l3 != NULL; l3 = l3->next) {
                        file_name = nautilus_file_get_name (NAUTILUS_FILE (l3->data));
                        if (l3 != l2 && g_strcmp0 (file_name, new_file_name->str) == 0) {
                                file = NAUTILUS_FILE (l3->data);
                                new_name = l1->data;

                                dialog->selection = g_list_remove_link (dialog->selection, l3);
                                new_names = g_list_remove_link (new_names, l1);

                                dialog->selection = g_list_prepend (dialog->selection, file);
                                new_names = g_list_prepend (new_names, new_name);

                                g_free (file_name);

                                break;
                        }

                        g_free (file_name);
                }

                g_free (old_file_name);
        }

        /* do the actual rename here */
        nautilus_file_batch_rename (dialog->selection, new_names, NULL, NULL);

        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog->window)), NULL);
}

static void
listbox_header_func (GtkListBoxRow               *row,
                     GtkListBoxRow               *before,
                     NautilusBatchRenameDialog   *dialog)
{
        gboolean show_separator;

        show_separator = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row),
                                          "show-separator"));

        if (show_separator)
        {
                GtkWidget *separator;

                separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
                gtk_widget_show (separator);

                gtk_list_box_row_set_header (row, separator);
        }
}

/* This is manually done instead of using GtkSizeGroup because of the complexity of
 * the later.*/
static void
update_rows_height (NautilusBatchRenameDialog *dialog)
{
        GList *l;
        gint minimum_height;
        gint natural_height;
        gint new_maximum_height;

        new_maximum_height = -1;

        /* check if maximum height has changed */
        for (l = dialog->listbox_labels_new; l != NULL; l = l->next) {
                gtk_widget_get_preferred_height (GTK_WIDGET (l->data),
                                                 &minimum_height,
                                                 &natural_height);

                if (minimum_height > new_maximum_height) {
                        new_maximum_height = minimum_height;
                }
        }

        for (l = dialog->listbox_labels_old; l != NULL; l = l->next) {
                gtk_widget_get_preferred_height (GTK_WIDGET (l->data),
                                                 &minimum_height,
                                                 &natural_height);

                if (minimum_height > new_maximum_height) {
                        new_maximum_height = minimum_height;
                }
        }

        for (l = dialog->listbox_icons; l != NULL; l = l->next) {
                gtk_widget_get_preferred_height (GTK_WIDGET (l->data),
                                                 &minimum_height,
                                                 &natural_height);

                if (minimum_height > new_maximum_height) {
                        new_maximum_height = minimum_height;
                }
        }

        if (new_maximum_height != dialog->row_height) {
                dialog->row_height = new_maximum_height;

                for (l = dialog->listbox_icons; l != NULL; l = l->next) {
                        g_object_set (G_OBJECT (l->data), "height-request", dialog->row_height, NULL);
                }

               for (l = dialog->listbox_labels_new; l != NULL; l = l->next) {
                        g_object_set (G_OBJECT (l->data), "height-request", dialog->row_height, NULL);
                }

               for (l = dialog->listbox_labels_old; l != NULL; l = l->next) {
                        g_object_set (G_OBJECT (l->data), "height-request", dialog->row_height, NULL);
                }
        }
}

static GtkWidget*
create_original_name_row_for_label (NautilusBatchRenameDialog *dialog,
                                    const gchar               *old_text,
                                    gboolean                   show_separator)
{
        GtkWidget *row;
        GtkWidget *label_old;

        row = gtk_list_box_row_new ();

        g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

        label_old = g_object_new (GTK_TYPE_LABEL,
                                  "label",old_text,
                                  "hexpand", TRUE,
                                  "xalign", 0.0,
                                  "margin-start", 6,
                                  NULL);

        gtk_label_set_ellipsize (GTK_LABEL (label_old), PANGO_ELLIPSIZE_END);

        dialog->listbox_labels_old = g_list_prepend (dialog->listbox_labels_old, label_old);

        gtk_container_add (GTK_CONTAINER (row), label_old);
        gtk_widget_show_all (row);

        return row;
}

static GtkWidget*
create_result_row_for_label (NautilusBatchRenameDialog *dialog,
                             const gchar               *new_text,
                             gboolean                   show_separator)
{
        GtkWidget *row;
        GtkWidget *label_new;

        row = gtk_list_box_row_new ();

        g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

        label_new = g_object_new (GTK_TYPE_LABEL,
                                  "label",new_text,
                                  "hexpand", TRUE,
                                  "xalign", 0.0,
                                  "margin-start", 6,
                                  NULL);

        gtk_label_set_ellipsize (GTK_LABEL (label_new), PANGO_ELLIPSIZE_END);

        dialog->listbox_labels_new = g_list_prepend (dialog->listbox_labels_new, label_new);

        gtk_container_add (GTK_CONTAINER (row), label_new);
        gtk_widget_show_all (row);

        return row;
}

static GtkWidget*
create_arrow_row_for_label (NautilusBatchRenameDialog *dialog,
                            gboolean                   show_separator)
{
        GtkWidget *row;
        GtkWidget *icon;

        row = gtk_list_box_row_new ();

        g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

        icon = g_object_new (GTK_TYPE_LABEL,
                             "label","→",
                             "hexpand", FALSE,
                             "xalign", 1.0,
                             "margin-start", 6,
                             NULL);

        dialog->listbox_icons = g_list_prepend (dialog->listbox_icons, icon);

        gtk_container_add (GTK_CONTAINER (row), icon);
        gtk_widget_show_all (row);

        return row;
}

static void
batch_rename_dialog_on_response (NautilusBatchRenameDialog *dialog,
                                 gint                       response_id,
                                 gpointer                   user_data)
{
        if (response_id == GTK_RESPONSE_OK) {
                /* wait for checking conflicts to finish, to be sure that
                 * the rename can actually take place */
                if (dialog->checking_conflicts) {
                        dialog->rename_clicked = TRUE;
                        return;
                }

                if (!gtk_widget_is_sensitive (dialog->rename_button))
                        return;

                GdkCursor *cursor;
                GdkDisplay *display;

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
                begin_batch_rename_dialog (dialog, dialog->new_names);
        }

        if (dialog->conflict_cancellable)
                g_cancellable_cancel (dialog->conflict_cancellable);

        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
fill_display_listbox (NautilusBatchRenameDialog *dialog)
{
        GtkWidget *row;
        GList *l1;
        GList *l2;
        NautilusFile *file;
        GString *new_name;
        gchar *name;

        dialog->original_name_listbox_rows = NULL;
        dialog->arrow_listbox_rows = NULL;
        dialog->result_listbox_rows = NULL;

        gtk_size_group_add_widget (dialog->size_group, dialog->result_listbox);
        gtk_size_group_add_widget (dialog->size_group, dialog->original_name_listbox);

        /* add rows to a list so that they can be removed when the renaming
         * result changes */
        for (l1 = dialog->new_names, l2 = dialog->selection; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                file = NAUTILUS_FILE (l2->data);
                new_name = l1->data;

                name = nautilus_file_get_name (file);
                row = create_original_name_row_for_label (dialog, name, TRUE);
                gtk_container_add (GTK_CONTAINER (dialog->original_name_listbox), row);
                dialog->original_name_listbox_rows = g_list_prepend (dialog->original_name_listbox_rows,
                                                                     row);

                row = create_arrow_row_for_label (dialog, TRUE);
                gtk_container_add (GTK_CONTAINER (dialog->arrow_listbox), row);
                dialog->arrow_listbox_rows = g_list_prepend (dialog->arrow_listbox_rows,
                                                             row);

                row = create_result_row_for_label (dialog, new_name->str, TRUE);
                gtk_container_add (GTK_CONTAINER (dialog->result_listbox), row);
                dialog->result_listbox_rows = g_list_prepend (dialog->result_listbox_rows,
                                                              row);

                g_free (name);
        }

        dialog->original_name_listbox_rows = g_list_reverse (dialog->original_name_listbox_rows);
        dialog->arrow_listbox_rows = g_list_reverse (dialog->arrow_listbox_rows);
        dialog->result_listbox_rows = g_list_reverse (dialog->result_listbox_rows);
        dialog->listbox_labels_old = g_list_reverse (dialog->listbox_labels_old);
        dialog->listbox_labels_new = g_list_reverse (dialog->listbox_labels_new);
        dialog->listbox_icons = g_list_reverse (dialog->listbox_icons);
}

static void
select_nth_conflict (NautilusBatchRenameDialog *dialog)
{
        GList *l;
        GString *file_name;
        GString *display_text;
        GString *new_name;
        gint nth_conflict_index;
        gint nth_conflict;
        gint name_occurences;
        GtkAdjustment *adjustment;
        GtkAllocation allocation;
        ConflictData *data;

        nth_conflict = dialog->selected_conflict;
        l = g_list_nth (dialog->duplicates, nth_conflict);
        data = l->data;

        /* the conflict that has to be selected */
        file_name = g_string_new (data->name);
        display_text = g_string_new ("");

        nth_conflict_index = data->index;

        l = g_list_nth (dialog->original_name_listbox_rows, nth_conflict_index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->original_name_listbox),
                                 l->data);

        l = g_list_nth (dialog->arrow_listbox_rows, nth_conflict_index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->arrow_listbox),
                                 l->data);

        l = g_list_nth (dialog->result_listbox_rows, nth_conflict_index);
        gtk_list_box_select_row (GTK_LIST_BOX (dialog->result_listbox),
                                 l->data);

        /* scroll to the selected row */
        adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (dialog->scrolled_window));
        gtk_widget_get_allocation (GTK_WIDGET (l->data), &allocation);
        gtk_adjustment_set_value (adjustment, (allocation.height + 1) * nth_conflict_index);

        name_occurences = 0;
        for (l = dialog->new_names; l != NULL; l = l->next) {
                new_name = l->data;
                if (g_string_equal (new_name, file_name))
                        name_occurences++;
        }
        if (name_occurences > 1)
                g_string_append_printf (display_text,
                                        _("\"%s\" would not be a unique new name"),
                                        file_name->str);
        else
                g_string_append_printf (display_text,
                                        _("\"%s\" would conflict with an existing file."),
                                        file_name->str);

        gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                             display_text->str);

        g_string_free (file_name, TRUE);
}

static void
select_next_conflict_down (NautilusBatchRenameDialog *dialog)
{
        dialog->selected_conflict++;

        if (dialog->selected_conflict == 1)
                gtk_widget_set_sensitive (dialog->conflict_up, TRUE);

        if (dialog->selected_conflict == dialog->conflicts_number - 1)
                gtk_widget_set_sensitive (dialog->conflict_down, FALSE);

        select_nth_conflict (dialog);
}

static void
select_next_conflict_up (NautilusBatchRenameDialog *dialog)
{
        dialog->selected_conflict--;

        if (dialog->selected_conflict == 0)
                gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

        if (dialog->selected_conflict == dialog->conflicts_number - 2)
                gtk_widget_set_sensitive (dialog->conflict_down, TRUE);

        select_nth_conflict (dialog);
}

static void
update_conflict_row_background (NautilusBatchRenameDialog *dialog)
{
        GList *l1;
        GList *l2;
        GList *l3;
        GList *l;
        gint index;
        GtkStyleContext *context;
        ConflictData *data;

        index = 0;

        for (l1 = dialog->original_name_listbox_rows,
             l2 = dialog->arrow_listbox_rows,
             l3 = dialog->result_listbox_rows;
             l1 != NULL && l2 != NULL && l3 != NULL;
             l1 = l1->next, l2 = l2->next, l3 = l3->next) {
                context = gtk_widget_get_style_context (GTK_WIDGET (l1->data));

                if (gtk_style_context_has_class (context, "conflict-row")) {
                        gtk_style_context_remove_class (context, "conflict-row");

                        context = gtk_widget_get_style_context (GTK_WIDGET (l2->data));
                        gtk_style_context_remove_class (context, "conflict-row");

                        context = gtk_widget_get_style_context (GTK_WIDGET (l3->data));
                        gtk_style_context_remove_class (context, "conflict-row");

                }

                for (l = dialog->duplicates; l != NULL; l = l->next) {
                        data = l->data;

                        if (data->index == index) {
                                context = gtk_widget_get_style_context (GTK_WIDGET (l1->data));
                                gtk_style_context_add_class (context, "conflict-row");

                                context = gtk_widget_get_style_context (GTK_WIDGET (l2->data));
                                gtk_style_context_add_class (context, "conflict-row"); 

                                context = gtk_widget_get_style_context (GTK_WIDGET (l3->data));
                                gtk_style_context_add_class (context, "conflict-row"); 
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

        for (l1 = dialog->new_names, l2 = dialog->listbox_labels_new; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                label = GTK_LABEL (l2->data);
                new_name = l1->data;

                gtk_label_set_label (label, new_name->str);
        }

        for (l1 = dialog->selection, l2 = dialog->listbox_labels_old; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                label = GTK_LABEL (l2->data);
                file = NAUTILUS_FILE (l1->data);

                old_name = nautilus_file_get_name (file);

                if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT) {
                        gtk_label_set_label (label, old_name);
                } else {
                        new_name = batch_rename_dialog_replace_label_text (old_name,
                                                                           gtk_entry_get_text (GTK_ENTRY (dialog->find_entry)));
                        gtk_label_set_markup (GTK_LABEL (label), new_name->str);

                        g_string_free (new_name, TRUE);
                }

                g_free (old_name);
        }

        update_rows_height (dialog);

        /* check if there are name conflicts and display them if they exist */
        if (dialog->duplicates != NULL) {
                update_conflict_row_background (dialog);

                gtk_widget_set_sensitive (dialog->rename_button, FALSE);

                gtk_widget_show (dialog->conflict_box);

                dialog->selected_conflict = 0;
                dialog->conflicts_number = g_list_length (dialog->duplicates);

                select_nth_conflict (dialog);

                gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

                if (g_list_length (dialog->duplicates) == 1)
                    gtk_widget_set_sensitive (dialog->conflict_down, FALSE);
                else
                    gtk_widget_set_sensitive (dialog->conflict_down, TRUE);
        } else {
                gtk_widget_hide (dialog->conflict_box);

                /* re-enable the rename button if there are no more name conflicts */
                if (dialog->duplicates == NULL && !gtk_widget_is_sensitive (dialog->rename_button)) {
                        update_conflict_row_background (dialog);
                        gtk_widget_set_sensitive (dialog->rename_button, TRUE);
                }
        }

        /* if the rename button was clicked and there's no conflict, then start renaming */
        if (dialog->rename_clicked && dialog->duplicates == NULL) {
                batch_rename_dialog_on_response (dialog, GTK_RESPONSE_OK, NULL);
        }

        if (dialog->rename_clicked && dialog->duplicates != NULL) {
                dialog->rename_clicked = FALSE;
        }
}


void
check_conflict_for_file (NautilusBatchRenameDialog *dialog,
                         NautilusDirectory         *directory,
                         GList                     *files)
{
        gchar *current_directory;
        gchar *parent_uri;
        gchar *table_parent_uri;
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
        ConflictData *data;

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

        for (l1 = dialog->new_names, l2 = dialog->selection;
             l1 != NULL && l2 != NULL;
             l1 = l1->next, l2 = l2->next) {
                new_name = l1->data;
                file = NAUTILUS_FILE (l2->data);
                parent_uri = nautilus_file_get_parent_uri (file);

                table_parent_uri = g_hash_table_lookup (new_names_table, new_name->str);

                if (g_strcmp0 (parent_uri, current_directory) == 0)
                        g_hash_table_insert (new_names_table,
                                             g_strdup (new_name->str),
                                             nautilus_file_get_parent_uri (file));

                if (table_parent_uri != NULL  && g_strcmp0 (current_directory, parent_uri) == 0) {
                        g_hash_table_insert (names_conflicts_table,
                                             g_strdup (new_name->str),
                                             nautilus_file_get_parent_uri (file));
                }

                g_free (parent_uri);
        }

        for (l1 = files; l1 != NULL; l1 = l1->next) {
                file = NAUTILUS_FILE (l1->data);
                g_hash_table_insert (directory_files_table,
                                     nautilus_file_get_name (file),
                                     GINT_TO_POINTER (HAVE_CONFLICT));
        }

        for (l1 = dialog->selection, l2 = dialog->new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                file = NAUTILUS_FILE (l1->data);

                file_name = g_string_new ("");
                name = nautilus_file_get_name (file);
                g_string_append (file_name, name);
                g_free (name);

                parent_uri = nautilus_file_get_parent_uri (file);

                new_name = l2->data;

                have_conflict = FALSE;

                /* check for duplicate only if the parent of the current file is
                 * the current directory and the name of the file has changed */
                if (g_strcmp0 (parent_uri, current_directory) == 0 &&
                    !g_string_equal (new_name, file_name)) {
                        exists = GPOINTER_TO_INT (g_hash_table_lookup (directory_files_table, new_name->str));

                        if (exists == HAVE_CONFLICT &&
                            !file_name_conflicts_with_results (dialog->selection, dialog->new_names, new_name, parent_uri)) {
                                data = g_new (ConflictData, 1);
                                data->name = g_strdup (new_name->str);
                                data->index = g_list_index (dialog->selection, l1->data);
                                dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                                     data);

                                have_conflict = TRUE;
                        }
                }
                if (!have_conflict) {
                        table_parent_uri = g_hash_table_lookup (names_conflicts_table, new_name->str);

                        if (table_parent_uri != NULL && g_strcmp0 (nautilus_file_get_parent_uri (file), current_directory) == 0) {
                                data = g_new (ConflictData, 1);
                                data->name = g_strdup (new_name->str);
                                data->index = g_list_index (dialog->selection, l1->data);
                                dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                                     data);

                                have_conflict = TRUE;
                        }
                }

                g_string_free (file_name, TRUE);
                g_free (parent_uri);
        }

        /* check if this is the last call of the callback. Update
         * the listbox with the conflicts if it is. */
        if (dialog->checked_parents == g_list_length (dialog->distinct_parents) - 1) {
                dialog->duplicates = g_list_reverse (dialog->duplicates);

                dialog->checking_conflicts = FALSE;

                update_listbox (dialog);                
        }

        dialog->checked_parents++;

        g_free (current_directory);
        g_hash_table_destroy (directory_files_table);
        g_hash_table_destroy (new_names_table);
        g_hash_table_destroy  (names_conflicts_table);
}

static void
file_names_list_has_duplicates_callback (GObject     *object,
                                        GAsyncResult *res,
                                        gpointer      user_data)
{
        NautilusBatchRenameDialog *dialog;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (object);

        if (g_cancellable_is_cancelled (dialog->conflict_cancellable))
                return;

        if (dialog->same_parent)
                update_listbox (dialog);
}

static void
on_call_when_ready (NautilusDirectory *directory,
                    GList             *files,
                    gpointer           callback_data)
{
        check_conflict_for_file (NAUTILUS_BATCH_RENAME_DIALOG (callback_data),
                                 directory,
                                 files);
}

static void
file_names_list_has_duplicates_async_thread (GTask        *task,
                                             gpointer      object,
                                             gpointer      task_data,
                                             GCancellable *cancellable)
{
        NautilusBatchRenameDialog *dialog;
        GList *new_names;
        GList *directory_files;
        GList *l1;
        GList *l2;
        NautilusFile *file;
        GString *file_name;
        GString *new_name;
        NautilusDirectory *parent;
        gboolean have_conflict;
        gboolean hash_table_insertion;
        gchar *name;
        GHashTable *directory_names_table;
        GHashTable *new_names_table;
        GHashTable *names_conflicts_table;
        gint exists;
        ConflictData *data;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (object);

        dialog->duplicates = NULL;

       if (g_cancellable_is_cancelled (cancellable))
                return;

        g_return_if_fail (g_list_length (dialog->new_names) == g_list_length (dialog->selection));

        /* If the batch rename is launched in a search, then for each file we have to check for
         * conflicts with each file in the file's parent directory */
        if (dialog->distinct_parents != NULL) {
                for (l1 = dialog->distinct_parents; l1 != NULL; l1 = l1->next) {
                        if (g_cancellable_is_cancelled (cancellable))
                                return;

                        parent = nautilus_directory_get_by_uri (l1->data);

                        nautilus_directory_call_when_ready (parent,
                                                            NAUTILUS_FILE_ATTRIBUTE_INFO,
                                                            TRUE,
                                                            on_call_when_ready,
                                                            dialog);
                }

                g_task_return_pointer (task, object, NULL);
                return;
        }

        new_names = batch_rename_dialog_get_new_names (dialog);

        directory_names_table = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       (GDestroyNotify) g_free,
                                                       NULL);
        new_names_table = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 NULL);
        names_conflicts_table = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       (GDestroyNotify) g_free,
                                                       NULL);

        directory_files = nautilus_directory_get_file_list (dialog->directory);

        for (l1 = new_names; l1 != NULL; l1 = l1->next) {
                new_name = l1->data;
                hash_table_insertion = g_hash_table_insert (new_names_table,
                                                            g_strdup (new_name->str),
                                                            GINT_TO_POINTER (HAVE_CONFLICT));

                if (!hash_table_insertion) {
                        g_hash_table_insert (names_conflicts_table,
                                             g_strdup (new_name->str),
                                             GINT_TO_POINTER (HAVE_CONFLICT));
                }
        }

        for (l1 = directory_files; l1 != NULL; l1 = l1->next) {
                file = NAUTILUS_FILE (l1->data);
                g_hash_table_insert (directory_names_table,
                                     nautilus_file_get_name (file),
                                     GINT_TO_POINTER (HAVE_CONFLICT));
        }

        for (l1 = new_names, l2 = dialog->selection; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                if (g_cancellable_is_cancelled (cancellable)) {
                        g_list_free_full (dialog->duplicates, conflict_data_free);
                        break;
                }

                file = NAUTILUS_FILE (l2->data);
                new_name = l1->data;

                have_conflict = FALSE;

                name = nautilus_file_get_name (file);
                file_name = g_string_new (name);

                g_free (name);

                /* check for duplicate only if the name has changed */
                if (!g_string_equal (new_name, file_name)) {
                        /* check with already existing files */
                        exists = GPOINTER_TO_INT (g_hash_table_lookup (directory_names_table, new_name->str));

                        if (exists == HAVE_CONFLICT &&
                            !file_name_conflicts_with_results (dialog->selection, new_names, new_name, NULL)) {
                                        data = g_new (ConflictData, 1);
                                        data->name = g_strdup (new_name->str);
                                        data->index = g_list_index (dialog->selection, l2->data);
                                        dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                                             data);
                                        have_conflict = TRUE;
                        }

                        /* check with files that will result from the batch rename, unless
                         * this file already has a conflict */
                        if (!have_conflict) {
                                exists = GPOINTER_TO_INT (g_hash_table_lookup (names_conflicts_table, new_name->str));

                                if (exists == HAVE_CONFLICT) {
                                        data = g_new (ConflictData, 1);
                                        data->name = g_strdup (new_name->str);
                                        data->index = g_list_index (dialog->selection, l2->data);
                                        dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                                             data);
                                }
                        }
                }

                g_string_free (file_name, TRUE);
        }

        g_hash_table_destroy (directory_names_table);
        g_hash_table_destroy (new_names_table);
        g_hash_table_destroy (names_conflicts_table);
        nautilus_file_list_free (directory_files);
        g_list_free_full (new_names, string_free);

        dialog->duplicates = g_list_reverse (dialog->duplicates);

        dialog->checking_conflicts = FALSE;

        g_task_return_pointer (task, object, NULL);

}

static void
file_names_list_has_duplicates_async (NautilusBatchRenameDialog *dialog,
                                     GAsyncReadyCallback         callback,
                                     gpointer                    user_data)
{
        if (dialog->checking_conflicts == TRUE)
                g_cancellable_cancel (dialog->conflict_cancellable);

        dialog->conflict_cancellable = g_cancellable_new ();

        dialog->checking_conflicts = TRUE;
        dialog->conflicts_task = g_task_new (dialog, dialog->conflict_cancellable, callback, user_data);

        g_task_set_priority (dialog->conflicts_task, G_PRIORITY_DEFAULT);
        g_task_run_in_thread (dialog->conflicts_task, file_names_list_has_duplicates_async_thread);

        g_object_unref (dialog->conflicts_task);
}

static void
check_if_tag_is_used (NautilusBatchRenameDialog *dialog,
                      gchar                     *tag_name,
                      gchar                     *action_name)
{
        GString *entry_text;
        GAction *action;
        TagData *tag_data;

        entry_text = g_string_new (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

        tag_data = g_hash_table_lookup (dialog->tag_info_table, tag_name);

        if (g_strrstr (entry_text->str, tag_name) && tag_data->set == FALSE) {
                action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                     action_name);
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
        }

        if (g_strrstr (entry_text->str, tag_name) == NULL) {
                action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                     action_name);
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

                tag_data->set = FALSE;
        }

        if (g_strrstr (entry_text->str, tag_name)) {
                tag_data->position = g_strrstr (entry_text->str, tag_name) -
                                     entry_text->str;
                tag_data->set = TRUE;
        }

        g_string_free (entry_text, TRUE);
}

static void
check_numbering_tags (NautilusBatchRenameDialog *dialog)
{
        GString *entry_text;
        GAction *add_numbering_action;
        TagData *tag_data;
        TagData *tag_data0;
        TagData *tag_data00;

        entry_text = g_string_new (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

        tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING);
        tag_data0 = g_hash_table_lookup (dialog->tag_info_table, NUMBERING0);
        tag_data00 = g_hash_table_lookup (dialog->tag_info_table, NUMBERING00);

        if ((g_strrstr (entry_text->str, NUMBERING) ||
            g_strrstr (entry_text->str, NUMBERING0) ||
            g_strrstr (entry_text->str, NUMBERING00)) &&
            (!tag_data->set && !tag_data0->set && !tag_data00->set)) {
                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-zero");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-one");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-two");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

                if (g_strrstr (entry_text->str, NUMBERING))
                        tag_data->set = TRUE;
                if (g_strrstr (entry_text->str, NUMBERING0))
                        tag_data0->set = TRUE;
                if (g_strrstr (entry_text->str, NUMBERING00))
                        tag_data00->set = TRUE;
        }

        if (g_strrstr (entry_text->str, NUMBERING) == NULL &&
            g_strrstr (entry_text->str, NUMBERING0) == NULL &&
            g_strrstr (entry_text->str, NUMBERING00) == NULL &&
            (tag_data->set || tag_data0->set || tag_data00->set)) {
                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-zero");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), TRUE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-one");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), TRUE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-two");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), TRUE);

                tag_data->set = FALSE;
                tag_data0->set = FALSE;
                tag_data00->set = FALSE;
        }

        if (g_strrstr (entry_text->str, NUMBERING)) {
                tag_data->position = g_strrstr (entry_text->str, NUMBERING) - entry_text->str;
        }

        if (g_strrstr (entry_text->str, NUMBERING0)) {
                tag_data0->position = g_strrstr (entry_text->str, NUMBERING0) - entry_text->str;
        }
        if (g_strrstr (entry_text->str, NUMBERING00)) {
                tag_data00->position = g_strrstr (entry_text->str, NUMBERING00) - entry_text->str;
        }
        g_string_free (entry_text, TRUE);
}

static void
update_tags (NautilusBatchRenameDialog *dialog)
{
        TagData *tag_data;

        check_if_tag_is_used (dialog,
                              ORIGINAL_FILE_NAME,
                              "add-original-file-name-tag");

        tag_data = g_hash_table_lookup (dialog->tag_info_table, CREATION_DATE);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      CREATION_DATE,
                                      "add-creation-date-tag");
        tag_data = g_hash_table_lookup (dialog->tag_info_table, CAMERA_MODEL);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      CAMERA_MODEL,
                                      "add-equipment-tag");
        tag_data = g_hash_table_lookup (dialog->tag_info_table, SEASON_NUMBER);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      SEASON_NUMBER,
                                      "add-season-tag");
        tag_data = g_hash_table_lookup (dialog->tag_info_table, EPISODE_NUMBER);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      EPISODE_NUMBER,
                                      "add-episode-tag");
        tag_data = g_hash_table_lookup (dialog->tag_info_table, TRACK_NUMBER);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      TRACK_NUMBER,
                                      "add-track-number-tag");

        tag_data = g_hash_table_lookup (dialog->tag_info_table, ARTIST_NAME);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      ARTIST_NAME,
                                      "add-artist-name-tag");

        tag_data = g_hash_table_lookup (dialog->tag_info_table, TITLE);
        if (tag_data->available)
                check_if_tag_is_used (dialog,
                                      TITLE,
                                      "add-title-tag");

        check_numbering_tags (dialog);
}

static gboolean
have_unallowed_character (NautilusBatchRenameDialog *dialog)
{
        const gchar *entry_text;
        gboolean have_unallowed_character;

        have_unallowed_character = FALSE;

        if (dialog->mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT) {
                entry_text = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));

                if (strstr (entry_text, "/") != NULL) {
                        have_unallowed_character = TRUE;
                }
        } else {
                entry_text = gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry));

                if (strstr (entry_text, "/") != NULL) {
                        have_unallowed_character = TRUE;
                }
        }

        if (have_unallowed_character) {
                gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                                    "\"/\" is an unallowed character");

                gtk_widget_set_sensitive (dialog->rename_button, FALSE);
                gtk_widget_set_sensitive (dialog->conflict_down, FALSE);
                gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

                gtk_widget_show (dialog->conflict_box);

                return TRUE;
        } else {
                gtk_widget_hide (dialog->conflict_box);

                return FALSE;
        }
}

static void
file_names_widget_entry_on_changed (NautilusBatchRenameDialog *dialog)
{
        TagData *tag_data;
        TagData *tag_data0;
        TagData *tag_data00;

        tag_data = g_hash_table_lookup (dialog->tag_info_table, NUMBERING);
        tag_data0 = g_hash_table_lookup (dialog->tag_info_table, NUMBERING0);
        tag_data00 = g_hash_table_lookup (dialog->tag_info_table, NUMBERING00);

        if (dialog->conflict_cancellable != NULL)
                g_cancellable_cancel (dialog->conflict_cancellable);

        if(dialog->selection == NULL)
                return;

        if (dialog->duplicates != NULL) {
                g_list_free_full (dialog->duplicates, conflict_data_free);
                dialog->duplicates = NULL;
        }

        if (have_unallowed_character (dialog))
                return;

        if (dialog->new_names != NULL)
                g_list_free_full (dialog->new_names, string_free);

        update_tags (dialog);

        if (!tag_data->set && !tag_data0->set && !tag_data00->set) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_label), "");
                gtk_widget_hide (dialog->numbering_order_button);
        } else {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_label), _("Automatic Numbering Order"));
                gtk_widget_show (dialog->numbering_order_button);
        }

        dialog->new_names = batch_rename_dialog_get_new_names (dialog);
        dialog->checked_parents = 0;

        file_names_list_has_duplicates_async (dialog,
                                              file_names_list_has_duplicates_callback,
                                              NULL);
}

static void
update_display_text (NautilusBatchRenameDialog *dialog)
{
        file_names_widget_entry_on_changed (dialog);
}

static void
batch_rename_dialog_mode_changed (NautilusBatchRenameDialog *dialog)
{
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->format_mode_button))) {
                gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "format");

                dialog->mode = NAUTILUS_BATCH_RENAME_DIALOG_FORMAT;

                gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));

        } else {
                gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "replace");

                dialog->mode = NAUTILUS_BATCH_RENAME_DIALOG_REPLACE;

                gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->find_entry));
        }

        update_display_text (dialog);

}

static void
add_button_clicked (NautilusBatchRenameDialog *dialog)
{
        if (gtk_widget_is_visible (dialog->add_popover))
                gtk_widget_set_visible (dialog->add_popover, FALSE);
        else
                gtk_widget_set_visible (dialog->add_popover, TRUE);
}

static void
add_popover_closed (NautilusBatchRenameDialog *dialog)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->add_button), FALSE);
}

static void
numbering_order_button_clicked (NautilusBatchRenameDialog *dialog)
{
        if (gtk_widget_is_visible (dialog->numbering_order_popover))
                gtk_widget_set_visible (dialog->numbering_order_popover, FALSE);
        else
                gtk_widget_set_visible (dialog->numbering_order_popover, TRUE);
}

static void
numbering_order_popover_closed (NautilusBatchRenameDialog *dialog)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->numbering_order_button), FALSE);
}

void
nautilus_batch_rename_dialog_query_finished (NautilusBatchRenameDialog *dialog,
                                             GHashTable                *hash_table,
                                             GList                     *selection_metadata)
{
        GMenuItem *first_created;
        GMenuItem *last_created;
        FileMetadata *metadata;
        TagData *tag_data;

        /* for files with no metadata */
        if (hash_table != NULL && g_hash_table_size (hash_table) == 0) {
                g_hash_table_destroy (hash_table);

                hash_table = NULL;
        }

        if (hash_table == NULL)
                dialog->create_date = NULL;
        else
                dialog->create_date = hash_table;

        if (dialog->create_date != NULL) {
                first_created = g_menu_item_new ("First Created",
                                                 "dialog.numbering-order-changed('first-created')");

                g_menu_append_item (dialog->numbering_order_menu, first_created);

                last_created = g_menu_item_new ("Last Created",
                                                 "dialog.numbering-order-changed('last-created')");

                g_menu_append_item (dialog->numbering_order_menu, last_created);

        }

        dialog->selection_metadata = selection_metadata;
        metadata = selection_metadata->data;

        tag_data = g_hash_table_lookup (dialog->tag_info_table, CREATION_DATE);
        if (metadata->creation_date == NULL || g_strcmp0 (metadata->creation_date->str, "") == 0) {
               disable_action (dialog, "add-creation-date-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, CAMERA_MODEL);
        if (metadata->equipment == NULL || g_strcmp0 (metadata->equipment->str, "") == 0) {
               disable_action (dialog, "add-equipment-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, SEASON_NUMBER);
        if (metadata->season == NULL || g_strcmp0 (metadata->season->str, "") == 0) {
               disable_action (dialog, "add-season-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, EPISODE_NUMBER);
        if (metadata->episode_number == NULL || g_strcmp0 (metadata->episode_number->str, "") == 0) {
               disable_action (dialog, "add-episode-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, TRACK_NUMBER);
        if (metadata->track_number == NULL || g_strcmp0 (metadata->track_number->str, "") == 0) {
               disable_action (dialog, "add-track-number-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, ARTIST_NAME);
        if (metadata->artist_name == NULL || g_strcmp0 (metadata->artist_name->str, "") == 0) {
               disable_action (dialog, "add-artist-name-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }

        tag_data = g_hash_table_lookup (dialog->tag_info_table, TITLE);
        if (metadata->title == NULL || g_strcmp0 (metadata->title->str, "") == 0) {
               disable_action (dialog, "add-title-tag");
               tag_data->available = FALSE;
        } else {
                tag_data->set = FALSE;
        }
}

static void
update_row_shadowing (GtkWidget *row,
                      gboolean   shown)
{
        GtkStyleContext *context;
        GtkStateFlags flags;

        if (!GTK_IS_LIST_BOX_ROW (row))
                return;

        context = gtk_widget_get_style_context (row);
        flags = gtk_style_context_get_state (context);

        if (shown)
                flags |= GTK_STATE_PRELIGHT;
        else
                flags &= ~GTK_STATE_PRELIGHT;

        gtk_style_context_set_state (context, flags);

}

static gboolean
on_leave_event (GtkWidget      *widget,
                      GdkEventMotion *event,
                      gpointer        user_data)
{
        NautilusBatchRenameDialog *dialog;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        update_row_shadowing (dialog->preselected_row1, FALSE);
        update_row_shadowing (dialog->preselected_row2, FALSE);

        dialog->preselected_row1 = NULL;
        dialog->preselected_row2 = NULL;

        return FALSE;
}

static gboolean
on_motion (GtkWidget      *widget,
           GdkEventMotion *event,
           gpointer        user_data)
{
        GtkListBoxRow *row;
        NautilusBatchRenameDialog *dialog;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        if (dialog->preselected_row1 && dialog->preselected_row2) {
                update_row_shadowing (dialog->preselected_row1, FALSE);
                update_row_shadowing (dialog->preselected_row2, FALSE);
        }

        if (widget == dialog->result_listbox) {
                row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->original_name_listbox), event->y);
                update_row_shadowing (GTK_WIDGET (row), TRUE);
                dialog->preselected_row1 = GTK_WIDGET (row);

                row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->arrow_listbox), event->y);
                update_row_shadowing (GTK_WIDGET (row), TRUE);
                dialog->preselected_row2 = GTK_WIDGET (row);

        }

        if (widget == dialog->arrow_listbox) {
                row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->original_name_listbox), event->y);
                update_row_shadowing (GTK_WIDGET (row), TRUE);
                dialog->preselected_row1 = GTK_WIDGET (row);

                row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->result_listbox), event->y);
                update_row_shadowing (GTK_WIDGET (row), TRUE);
                dialog->preselected_row2 = GTK_WIDGET (row);
        }

        if (widget == dialog->original_name_listbox) {
                row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->result_listbox), event->y);
                update_row_shadowing (GTK_WIDGET (row), TRUE);
                dialog->preselected_row1 = GTK_WIDGET (row);

                row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (dialog->arrow_listbox), event->y);
                update_row_shadowing (GTK_WIDGET (row), TRUE);
                dialog->preselected_row2 = GTK_WIDGET (row);
        }

        return FALSE;
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
                                             "add-original-file-name-tag");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

        check_metadata_for_selection (dialog, dialog->selection);
}

static void
file_names_widget_on_activate (NautilusBatchRenameDialog *dialog)
{
        batch_rename_dialog_on_response (dialog, GTK_RESPONSE_OK, NULL);
}

static gboolean
remove_tag (NautilusBatchRenameDialog *dialog,
            gchar                     *tag_name,
            gchar                     *keyval_name,
            gboolean                   is_modifier)
{
        TagData *tag_data;
        gint cursor_position;
        GString *new_entry_text;
        GString *entry_text;

        g_object_get (dialog->name_entry, "cursor-position", &cursor_position, NULL);

        tag_data = g_hash_table_lookup (dialog->tag_info_table, tag_name);

        entry_text = g_string_new (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

        if (gtk_editable_get_selection_bounds (GTK_EDITABLE (dialog->name_entry), NULL, NULL) &&
            cursor_position == tag_data->position + strlen (tag_name) &&
            g_strcmp0(keyval_name, "BackSpace") == 0)
                return FALSE;

        if (gtk_editable_get_selection_bounds (GTK_EDITABLE (dialog->name_entry), NULL, NULL) &&
            cursor_position == tag_data->position &&
            g_strcmp0(keyval_name, "Delete") == 0)
                return FALSE;

        if (g_strcmp0(keyval_name, "BackSpace") == 0 && tag_data->set) {
                if (cursor_position > tag_data->position &&
                    cursor_position <= tag_data->position + strlen (tag_name)) {
                        new_entry_text = g_string_new ("");
                        new_entry_text = g_string_append_len (new_entry_text,
                                                              entry_text->str,
                                                              tag_data->position);
                        new_entry_text = g_string_append (new_entry_text,
                                                          entry_text->str + tag_data->position + strlen (tag_name));

                        gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), new_entry_text->str);
                        gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), tag_data->position);

                        tag_data->set = FALSE;

                        g_string_free (new_entry_text, TRUE);
                        g_string_free (entry_text, TRUE);

                        return TRUE;
                }
        }

        if (g_strcmp0(keyval_name, "Delete") == 0 && tag_data->set) {
                if (cursor_position >= tag_data->position &&
                    cursor_position < tag_data->position + strlen (tag_name)) {
                        new_entry_text = g_string_new ("");
                        new_entry_text = g_string_append_len (new_entry_text,
                                                              entry_text->str,
                                                              tag_data->position);
                        new_entry_text = g_string_append (new_entry_text,
                                                          entry_text->str + tag_data->position + strlen (tag_name));

                        gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), new_entry_text->str);
                        gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), tag_data->position);

                        tag_data->set = FALSE;

                        g_string_free (new_entry_text, TRUE);
                        g_string_free (entry_text, TRUE);

                        return TRUE;
                }
        }

        if (!is_modifier && tag_data->set &&
            g_strcmp0(keyval_name, "Left") != 0 &&
            g_strcmp0(keyval_name, "Right") != 0 &&
            g_strcmp0(keyval_name, "Return") != 0) {
                if (cursor_position > tag_data->position &&
                    cursor_position < tag_data->position + strlen (tag_name)) {
                        new_entry_text = g_string_new ("");
                        new_entry_text = g_string_append_len (new_entry_text,
                                                              entry_text->str,
                                                              tag_data->position);
                        new_entry_text = g_string_append (new_entry_text,
                                                          entry_text->str + tag_data->position + strlen (tag_name));

                        gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), new_entry_text->str);
                        gtk_editable_set_position (GTK_EDITABLE (dialog->name_entry), tag_data->position);

                        tag_data->set = FALSE;

                        g_string_free (new_entry_text, TRUE);
                        g_string_free (entry_text, TRUE);

                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
on_key_press_event (GtkWidget    *widget,
                    GdkEventKey  *event,
                    gpointer      user_data)
{
        NautilusBatchRenameDialog *dialog;
        gchar* keyval_name;
        GdkEvent *gdk_event;

        gdk_event = (GdkEvent *) event;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (user_data);

        keyval_name = gdk_keyval_name (event->keyval);

        if (remove_tag (dialog, ORIGINAL_FILE_NAME, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, NUMBERING, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, NUMBERING0, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, NUMBERING00, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, CAMERA_MODEL, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, CREATION_DATE, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, SEASON_NUMBER, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, EPISODE_NUMBER, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, TRACK_NUMBER, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, ARTIST_NAME, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        if (remove_tag (dialog, TITLE, keyval_name, gdk_event->key.is_modifier))
                return TRUE;

        return FALSE;
}

static void
nautilus_batch_rename_dialog_finalize (GObject *object)
{
        NautilusBatchRenameDialog *dialog;
        GList *l;

        dialog = NAUTILUS_BATCH_RENAME_DIALOG (object);

        if (dialog->checking_conflicts) {
                g_cancellable_cancel (dialog->conflict_cancellable);
                g_object_unref (dialog->conflict_cancellable);
        }

        g_list_free (dialog->original_name_listbox_rows);
        g_list_free (dialog->arrow_listbox_rows);
        g_list_free (dialog->result_listbox_rows);
        g_list_free (dialog->listbox_labels_new);
        g_list_free (dialog->listbox_labels_old);
        g_list_free (dialog->listbox_icons);

        for (l = dialog->selection_metadata; l != NULL; l = l->next) {
                FileMetadata *metadata;

                metadata = l->data;

                if (metadata->file_name != NULL)
                        g_string_free (metadata->file_name, TRUE);
                if (metadata->creation_date != NULL)
                        g_string_free (metadata->creation_date, TRUE);
                if (metadata->equipment != NULL)
                        g_string_free (metadata->equipment, TRUE);
                if (metadata->season != NULL)
                        g_string_free (metadata->season, TRUE);
                if (metadata->episode_number != NULL)
                        g_string_free (metadata->episode_number, TRUE);
                if (metadata->track_number != NULL)
                        g_string_free (metadata->track_number, TRUE);
                if (metadata->artist_name != NULL)
                        g_string_free (metadata->artist_name, TRUE);
        }

        if (dialog->create_date != NULL)
                g_hash_table_destroy (dialog->create_date);

        g_list_free_full (dialog->distinct_parents, g_free);
        g_list_free_full (dialog->new_names, string_free);
        g_list_free_full (dialog->duplicates, conflict_data_free);

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
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, add_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, add_popover);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, scrolled_window);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_popover);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_order_menu);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_box);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_up);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, conflict_down);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, add_tag_menu);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRenameDialog, numbering_label);

        gtk_widget_class_bind_template_callback (widget_class, file_names_widget_entry_on_changed);
        gtk_widget_class_bind_template_callback (widget_class, file_names_widget_on_activate);
        gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_mode_changed);
        gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, add_popover_closed);
        gtk_widget_class_bind_template_callback (widget_class, numbering_order_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, numbering_order_popover_closed);
        gtk_widget_class_bind_template_callback (widget_class, select_next_conflict_up);
        gtk_widget_class_bind_template_callback (widget_class, select_next_conflict_down);
        gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_on_response);
        gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
}

GtkWidget*
nautilus_batch_rename_dialog_new (GList             *selection,
                                  NautilusDirectory *directory,
                                  NautilusWindow    *window)
{
        NautilusBatchRenameDialog *dialog;
        gint files_number;
        GList *l;
        GString *dialog_title;

        dialog = g_object_new (NAUTILUS_TYPE_BATCH_RENAME_DIALOG, "use-header-bar", TRUE, NULL);

        dialog->selection = selection;
        dialog->directory = directory;
        dialog->window = window;

        gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (window));

        files_number = 0;

        for (l = dialog->selection; l != NULL; l = l->next)
                files_number++;

        dialog_title = g_string_new ("");
        g_string_append_printf (dialog_title, "Renaming %d files", files_number);
        gtk_window_set_title (GTK_WINDOW (dialog), dialog_title->str);

        nautilus_batch_rename_dialog_initialize_actions (dialog);

        dialog->same_parent = !NAUTILUS_IS_SEARCH_DIRECTORY (directory);

        if (!dialog->same_parent)
                dialog->distinct_parents = distinct_file_parents (dialog->selection);
        else
                dialog->distinct_parents = NULL;

        update_display_text (dialog);

        fill_display_listbox (dialog);

        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);

        g_string_free (dialog_title, TRUE);

        return GTK_WIDGET (dialog);
}

static void
nautilus_batch_rename_dialog_init (NautilusBatchRenameDialog *self)
{
        TagData *tag_data;

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

        gtk_popover_bind_model (GTK_POPOVER (self->numbering_order_popover),
                                G_MENU_MODEL (self->numbering_order_menu),
                                NULL);
        gtk_popover_bind_model (GTK_POPOVER (self->add_popover),
                                G_MENU_MODEL (self->add_tag_menu),
                                NULL);

        gtk_label_set_ellipsize (GTK_LABEL (self->conflict_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (GTK_LABEL (self->conflict_label), 1);

        self->duplicates = NULL;
        self->new_names = NULL;

        self->checking_conflicts = FALSE;

        self->rename_clicked = FALSE;


        self->tag_info_table = g_hash_table_new_full (g_str_hash,
                                                      g_str_equal,
                                                      (GDestroyNotify) g_free,
                                                      (GDestroyNotify) g_free);
        tag_data = g_new (TagData, 1);
        tag_data->available = TRUE;
        tag_data->set = TRUE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (ORIGINAL_FILE_NAME), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = TRUE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (NUMBERING), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = TRUE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (NUMBERING0), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = TRUE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (NUMBERING00), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (CREATION_DATE), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (CAMERA_MODEL), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (SEASON_NUMBER), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (EPISODE_NUMBER), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (TRACK_NUMBER), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (ARTIST_NAME), tag_data);

        tag_data = g_new (TagData, 1);
        tag_data->available = FALSE;
        tag_data->set = FALSE;
        tag_data->position = 0;
        g_hash_table_insert (self->tag_info_table, g_strdup (TITLE), tag_data);

        gtk_entry_set_text (GTK_ENTRY (self->name_entry),ORIGINAL_FILE_NAME);

        self->row_height = -1;

        g_signal_connect (self->original_name_listbox, "row-selected", G_CALLBACK (row_selected), self);
        g_signal_connect (self->arrow_listbox, "row-selected", G_CALLBACK (row_selected), self);
        g_signal_connect (self->result_listbox, "row-selected", G_CALLBACK (row_selected), self);

        self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        g_signal_connect (self->original_name_listbox,
                          "motion-notify-event",
                          G_CALLBACK (on_motion),
                          self);
        g_signal_connect (self->result_listbox,
                          "motion-notify-event",
                          G_CALLBACK (on_motion),
                          self);
        g_signal_connect (self->arrow_listbox,
                          "motion-notify-event",
                          G_CALLBACK (on_motion),
                          self);

        g_signal_connect (self->original_name_listbox,
                          "leave-notify-event",
                          G_CALLBACK (on_leave_event),
                          self);
        g_signal_connect (self->result_listbox,
                          "leave-notify-event",
                          G_CALLBACK (on_leave_event),
                          self);
        g_signal_connect (self->arrow_listbox,
                          "leave-notify-event",
                          G_CALLBACK (on_leave_event),
                          self);
}