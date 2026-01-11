/* nautilus-templates-dialog.c
 *
 * Copyright 2022 Ignacy Kuchciński <ignacykuchcinski@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-templates-dialog.h"

#include <glib/gi18n.h>

#include "nautilus-directory.h"
#include "nautilus-file-operations.h"
#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"

#define MAX_MENU_LEVELS 5
#define TEMPLATE_LIMIT 30

struct _NautilusTemplatesDialog
{
    AdwDialog parent_instance;

    GtkWidget *templates_list_box;
    GtkWidget *templates_search_entry;

    GList *templates_directory_list;
    GtkTreeListModel *templates_list_model;
    GtkStack *templates_stack;
    GtkFilterListModel *filter_model;
    GtkCustomFilter *filter;

    guint update_timeout_id;

    gboolean show_hidden_files;
};

G_DEFINE_TYPE (NautilusTemplatesDialog, nautilus_templates_dialog, ADW_TYPE_DIALOG)

/* Forward declarations */
static void update_templates_menu (NautilusTemplatesDialog *self);

static void schedule_update (NautilusTemplatesDialog *self);

static void remove_update_timeout_callback (NautilusTemplatesDialog *self);

static void
on_import_response (GtkFileDialog           *chooser_dialog,
                    GAsyncResult            *result,
                    NautilusTemplatesDialog *self)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) file = NULL;

    file = gtk_file_dialog_open_finish (chooser_dialog, result, &error);

    if (file != NULL)
    {
        g_autofree char *path = g_file_get_path (file);
        g_autoptr (GFile) templates_location =
            g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));

        nautilus_file_operations_copy_async (&(GList){.data = file},
                                             templates_location,
                                             GTK_WINDOW (self),
                                             NULL, NULL, NULL);

        schedule_update (self);
    }
}

static void
nautilus_templates_dialog_import_template_dialog (NautilusTemplatesDialog *self G_GNUC_UNUSED)
{
    GtkFileDialog *chooser_dialog = gtk_file_dialog_new ();

    gtk_file_dialog_set_accept_label (chooser_dialog, _("_Import"));
    gtk_file_dialog_set_title (chooser_dialog, _("Import Template"));

    gtk_file_dialog_open (chooser_dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                          NULL, (GAsyncReadyCallback) on_import_response, self);
}

static void
templates_added_or_changed_callback (NautilusDirectory *directory,
                                     GList             *files,
                                     gpointer           callback_data)
{
    NautilusTemplatesDialog *self;

    self = NAUTILUS_TEMPLATES_DIALOG (callback_data);

    schedule_update (self);
}

static void
add_directory_to_directory_list (NautilusTemplatesDialog  *self,
                                 NautilusDirectory        *directory,
                                 GList                   **directory_list,
                                 GCallback                 changed_callback)
{
    NautilusFileAttributes attributes;

    if (g_list_find (*directory_list, directory) == NULL)
    {
        nautilus_directory_ref (directory);

        attributes =
            NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
            NAUTILUS_FILE_ATTRIBUTE_INFO |
            NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

        nautilus_directory_file_monitor_add (directory, directory_list,
                                             FALSE, attributes,
                                             (NautilusDirectoryCallback) changed_callback, self);

        g_signal_connect_object (directory, "files-added",
                                 G_CALLBACK (changed_callback), self, 0);
        g_signal_connect_object (directory, "files-changed",
                                 G_CALLBACK (changed_callback), self, 0);

        *directory_list = g_list_append (*directory_list, directory);
    }
}

static void
remove_directory_from_directory_list (NautilusTemplatesDialog  *self,
                                      NautilusDirectory        *directory,
                                      GList                   **directory_list,
                                      GCallback                 changed_callback)
{
    *directory_list = g_list_remove (*directory_list, directory);

    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (changed_callback),
                                          self);

    nautilus_directory_file_monitor_remove (directory, directory_list);

    nautilus_directory_unref (directory);
}

static void
add_directory_to_templates_directory_list (NautilusTemplatesDialog *self,
                                           NautilusDirectory       *directory)
{
    add_directory_to_directory_list (self, directory,
                                     &self->templates_directory_list,
                                     G_CALLBACK (templates_added_or_changed_callback));
}

static void
remove_directory_from_templates_directory_list (NautilusTemplatesDialog *self,
                                                NautilusDirectory       *directory)
{
    remove_directory_from_directory_list (self, directory,
                                          &self->templates_directory_list,
                                          G_CALLBACK (templates_added_or_changed_callback));
}

static void
nautilus_templates_dialog_dispose (GObject *object)
{
    NautilusTemplatesDialog *self = NAUTILUS_TEMPLATES_DIALOG (object);

    /* Add other cleanup statements here*/
    remove_update_timeout_callback (self);

    g_clear_list (&self->templates_directory_list, g_object_unref);

    g_clear_object (&self->templates_list_model);

    G_OBJECT_CLASS (nautilus_templates_dialog_parent_class)->dispose (object);
}

static void
remove_update_timeout_callback (NautilusTemplatesDialog *self)
{
    if (self->update_timeout_id != 0)
    {
        g_source_remove (self->update_timeout_id);
        self->update_timeout_id = 0;
    }
}

static gboolean
update_timeout_callback (gpointer data)
{
    NautilusTemplatesDialog *self = NAUTILUS_TEMPLATES_DIALOG (data);

    g_object_ref (G_OBJECT (self));

    self->update_timeout_id = 0;
    update_templates_menu (self);

    g_object_unref (G_OBJECT (self));

    return FALSE;
}

static gboolean
directory_belongs_in_templates_menu (const char *templates_directory_uri,
                                     const char *uri)
{
    int num_levels;
    int i;

    if (templates_directory_uri == NULL)
    {
        return FALSE;
    }

    if (!g_str_has_prefix (uri, templates_directory_uri))
    {
        return FALSE;
    }

    num_levels = 0;
    for (i = strlen (templates_directory_uri); uri[i] != '\0'; i++)
    {
        if (uri[i] == '/')
        {
            num_levels++;
        }
    }

    if (num_levels > MAX_MENU_LEVELS)
    {
        return FALSE;
    }

    return TRUE;
}

static gboolean
filter_templates_callback (NautilusFile *file,
                           gpointer      callback_data)
{
    /*
     * We want to show hidden files, but not directories. This is a compromise
     * to allow creating hidden files but to prevent content from .git directory
     * for example. See https://gitlab.gnome.org/GNOME/nautilus/issues/1413.
     */
    NautilusTemplatesDialog *self = callback_data;

    if (nautilus_file_is_hidden_file (file))
    {
        if (!self->show_hidden_files)
        {
            return FALSE;
        }

        if (nautilus_file_is_directory (file))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static GListStore *
update_directory_in_templates_menu (NautilusTemplatesDialog *self,
                                    NautilusDirectory       *directory)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    g_autolist (NautilusFile) file_list = nautilus_directory_get_file_list (directory);
    int num = 0;
    GListStore *store = g_list_store_new (NAUTILUS_TYPE_FILE);
    gboolean any_templates = FALSE;

    file_list = nautilus_file_list_filter (file_list, filter_templates_callback, self);
    file_list = nautilus_file_list_sort_by_display_name (file_list);

    for (GList *node = file_list; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++)
    {
        NautilusFile *file = node->data;

        g_list_store_append (store, file);
        any_templates = TRUE;
    }

    if (!any_templates)
    {
        g_object_unref (store);
        store = NULL;
    }

    return G_LIST_STORE (store);
}

static GListModel *
templates_tree_list_model (GObject  *item,
                           gpointer  user_data)
{
    NautilusFile *file;
    NautilusDirectory *dir;
    char *uri;
    char *templates_directory_uri;
    NautilusTemplatesDialog *self = NAUTILUS_TEMPLATES_DIALOG (user_data);

    file = NAUTILUS_FILE (item);

    templates_directory_uri = nautilus_get_templates_directory_uri ();

    if (nautilus_file_is_directory (file))
    {
        uri = nautilus_file_get_uri (file);
        if (directory_belongs_in_templates_menu (templates_directory_uri, uri))
        {
            dir = nautilus_directory_get_by_uri (uri);
            add_directory_to_templates_directory_list (self, dir);

            return G_LIST_MODEL (update_directory_in_templates_menu (self, dir));
        }
        g_free (uri);
    }

    return NULL;
}

static void
update_templates_menu (NautilusTemplatesDialog *self)
{
    g_autofree char *templates_directory_uri = NULL;
    g_autolist (NautilusDirectory) sorted_copy = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    GListStore *model;
    GtkTreeListModel *treemodel;

    if (!nautilus_should_use_templates_directory ())
    {
        return;
    }

    templates_directory_uri = nautilus_get_templates_directory_uri ();
    sorted_copy = nautilus_directory_list_sort_by_uri
                      (nautilus_directory_list_copy (self->templates_directory_list));

    for (GList *dir_l = sorted_copy; dir_l != NULL; dir_l = dir_l->next)
    {
        g_autofree char *uri = nautilus_directory_get_uri (dir_l->data);
        if (!directory_belongs_in_templates_menu (templates_directory_uri, uri))
        {
            remove_directory_from_templates_directory_list (self, dir_l->data);
        }
    }

    directory = nautilus_directory_get_by_uri (templates_directory_uri);

    model = update_directory_in_templates_menu (self, directory);
    treemodel = gtk_tree_list_model_new (G_LIST_MODEL (model),
                                         FALSE, FALSE,
                                         (GtkTreeListModelCreateModelFunc) templates_tree_list_model,
                                         self, NULL);

    self->templates_list_model = treemodel;
    gtk_filter_list_model_set_model (self->filter_model, G_LIST_MODEL (self->templates_list_model));
}

static void
row_activated (GtkListBox    *self,
               GtkListBoxRow *row,
               gpointer       user_data)
{
    GtkTreeExpander *expander;
    NautilusFile *file;

    expander = GTK_TREE_EXPANDER (gtk_list_box_row_get_child (row));
    file = NAUTILUS_FILE (gtk_tree_expander_get_item (expander));

    if (file == NULL)
    {
        return;
    }

    if (nautilus_file_is_directory (file))
    {
        if (!gtk_tree_list_row_get_expanded (gtk_tree_expander_get_list_row (expander)))
        {
            gtk_tree_list_row_set_expanded (gtk_tree_expander_get_list_row (expander), TRUE);
        }
        else
        {
            gtk_tree_list_row_set_expanded (gtk_tree_expander_get_list_row (expander), FALSE);
        }
    }

    g_object_unref (file);
}

static GtkWidget *
templates_list_box_widgets (GObject  *item,
                            gpointer  user_data)
{
    NautilusFile *file;
    GtkWidget *expander;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *image;
    GtkWidget *list_box_row;
    NautilusFileIconFlags flags;

    flags = 0;
    file = NAUTILUS_FILE (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (item)));

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    expander = gtk_tree_expander_new ();
    gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), GTK_TREE_LIST_ROW (item));
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    label = gtk_label_new (nautilus_file_get_name (file));
    image = gtk_image_new ();

    gtk_image_set_from_gicon (GTK_IMAGE (image), nautilus_file_get_gicon (file, flags));
    gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
    gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), box);
    gtk_box_append (GTK_BOX (box), image);
    gtk_box_append (GTK_BOX (box), label);

    gtk_widget_set_margin_top (expander, 12);
    gtk_widget_set_margin_bottom (expander, 12);
    gtk_widget_set_margin_start (expander, 12);
    gtk_widget_set_margin_end (expander, 12);

    list_box_row = gtk_list_box_row_new ();
    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (list_box_row), expander);

    if (nautilus_file_is_directory (file))
    {
        gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (list_box_row), FALSE);
    }

    g_object_unref (file);

    return list_box_row;
}

static void
set_expanded_recursive (GtkTreeListRow *row,
                        gboolean        expanded)
{
    guint i = 0;
    GtkTreeListRow *child_row;

    gtk_tree_list_row_set_expanded (row, expanded);

    while ((child_row = gtk_tree_list_row_get_child_row (row, i)) != NULL)
    {
        set_expanded_recursive (child_row, expanded);
        i += 1;
    }
}

static gboolean
match_function (GObject  *item,
                gpointer  user_data)
{
    const gchar *text;
    NautilusFile *file;
    const gchar *file_name;

    file = NAUTILUS_FILE (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (item)));

    if (file == NULL)
    {
        return FALSE;
    }

    file_name = nautilus_file_get_display_name (file);
    text = gtk_editable_get_text (GTK_EDITABLE (user_data));

    g_return_val_if_fail (text != NULL, TRUE);

    return g_str_match_string (text, file_name, TRUE);
}

static void
search_changed (GtkSearchEntry *entry,
                gpointer        user_data)
{
    NautilusTemplatesDialog *self = NAUTILUS_TEMPLATES_DIALOG (user_data);
    guint i = 0;
    GtkTreeListRow *child_row;
    const gchar *text;
    gboolean check;

    text = gtk_editable_get_text (GTK_EDITABLE (entry));

    g_return_if_fail (text != NULL);

    if (g_strcmp0 ("", text) != 0)
    {
        check = TRUE;
    }
    else
    {
        check = FALSE;
    }

    while ((child_row = gtk_tree_list_model_get_child_row (self->templates_list_model, i)) != NULL)
    {
        if (check)
        {
            set_expanded_recursive (child_row, TRUE);
        }
        else
        {
            set_expanded_recursive (child_row, FALSE);
        }

        i += 1;
    }

    gtk_filter_changed (GTK_FILTER (self->filter), GTK_FILTER_CHANGE_DIFFERENT);
}

static void
schedule_update (NautilusTemplatesDialog *self)
{
    if (self->update_timeout_id == 0)
    {
        self->update_timeout_id
            = g_timeout_add (100, update_timeout_callback, self);
    }
}

NautilusFile *
nautilus_templates_dialog_get_selected_file (NautilusTemplatesDialog *self)
{
    GtkTreeExpander *expander;
    GtkListBoxRow *row;
    NautilusFile *file = NULL;

    row = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->templates_list_box));

    if (row != NULL)
    {
        expander = GTK_TREE_EXPANDER (gtk_list_box_row_get_child (row));
        file = NAUTILUS_FILE (gtk_tree_expander_get_item (expander));
    }

    return file;
}

void
nautilus_templates_dialog_new (GtkWindow *parent_window)
{
    NautilusTemplatesDialog *self = g_object_new (NAUTILUS_TYPE_TEMPLATES_DIALOG,
                                                  NULL);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent_window));
}

static void
nautilus_templates_dialog_class_init (NautilusTemplatesDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = nautilus_templates_dialog_dispose;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-templates-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusTemplatesDialog, templates_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusTemplatesDialog, templates_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusTemplatesDialog, templates_search_entry);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_templates_dialog_import_template_dialog);
    gtk_widget_class_bind_template_callback (widget_class, row_activated);
    gtk_widget_class_bind_template_callback (widget_class, search_changed);
}

static void
nautilus_templates_dialog_init (NautilusTemplatesDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    self->filter = gtk_custom_filter_new ((GtkCustomFilterFunc) match_function, self->templates_search_entry, NULL);
    self->filter_model = gtk_filter_list_model_new (NULL, GTK_FILTER (self->filter));

    /* Make templates model */
    update_templates_menu (self);

    gtk_list_box_bind_model (GTK_LIST_BOX (self->templates_list_box),
                             G_LIST_MODEL (self->filter_model),
                             (GtkListBoxCreateWidgetFunc) templates_list_box_widgets,
                             NULL,
                             NULL);

    if (g_list_model_get_n_items (G_LIST_MODEL (self->templates_list_model)) == 0)
    {
        gtk_stack_set_visible_child_name (self->templates_stack, "no-templates");
    }
    else
    {
        gtk_stack_set_visible_child_name (self->templates_stack, "templates-list");
    }

    self->show_hidden_files = g_settings_get_boolean (gtk_filechooser_preferences,
                                                      NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
}
