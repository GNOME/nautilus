/* nautilus-column-chooser.h - A column chooser widget
 *
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the column COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Dave Camp <dave@ximian.com>
 */

#include <config.h>
#include "nautilus-column-chooser.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nautilus-extension.h>

#include "nautilus-column-utilities.h"

struct _NautilusColumnChooser
{
    GtkBox parent;

    GtkWidget *view;
    GtkListStore *store;

    GtkWidget *move_up_button;
    GtkWidget *move_down_button;
    GtkWidget *use_default_button;

    NautilusFile *file;
};

enum
{
    COLUMN_VISIBLE,
    COLUMN_LABEL,
    COLUMN_NAME,
    COLUMN_SENSITIVE,
    NUM_COLUMNS
};

enum
{
    PROP_FILE = 1,
    NUM_PROPERTIES
};

enum
{
    CHANGED,
    USE_DEFAULT,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NautilusColumnChooser, nautilus_column_chooser, GTK_TYPE_BOX);

static void
nautilus_column_chooser_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    NautilusColumnChooser *chooser;

    chooser = NAUTILUS_COLUMN_CHOOSER (object);

    switch (param_id)
    {
        case PROP_FILE:
        {
            chooser->file = g_value_get_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
update_buttons (NautilusColumnChooser *chooser)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->view));

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        gboolean visible;
        gboolean top;
        gboolean bottom;
        GtkTreePath *first;
        GtkTreePath *path;

        gtk_tree_model_get (GTK_TREE_MODEL (chooser->store),
                            &iter,
                            COLUMN_VISIBLE, &visible,
                            -1);

        path = gtk_tree_model_get_path (GTK_TREE_MODEL (chooser->store),
                                        &iter);
        first = gtk_tree_path_new_first ();

        top = (gtk_tree_path_compare (path, first) == 0);

        gtk_tree_path_free (path);
        gtk_tree_path_free (first);

        bottom = !gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->store),
                                            &iter);

        gtk_widget_set_sensitive (chooser->move_up_button,
                                  !top);
        gtk_widget_set_sensitive (chooser->move_down_button,
                                  !bottom);
    }
    else
    {
        gtk_widget_set_sensitive (chooser->move_up_button,
                                  FALSE);
        gtk_widget_set_sensitive (chooser->move_down_button,
                                  FALSE);
    }
}

static void
list_changed (NautilusColumnChooser *chooser)
{
    update_buttons (chooser);
    g_signal_emit (chooser, signals[CHANGED], 0);
}

static void
toggle_path (NautilusColumnChooser *chooser,
             GtkTreePath           *path)
{
    GtkTreeIter iter;
    gboolean visible;
    g_autofree gchar *name = NULL;

    gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->store),
                             &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL (chooser->store), &iter,
                        COLUMN_VISIBLE, &visible,
                        COLUMN_NAME, &name, -1);

    if (g_strcmp0 (name, "name") == 0)
    {
        /* Don't allow name column to be disabled. */
        return;
    }

    gtk_list_store_set (chooser->store,
                        &iter, COLUMN_VISIBLE, !visible, -1);
    list_changed (chooser);
}


static void
visible_toggled_callback (GtkCellRendererToggle *cell,
                          char                  *path_string,
                          gpointer               user_data)
{
    GtkTreePath *path;

    path = gtk_tree_path_new_from_string (path_string);
    toggle_path (NAUTILUS_COLUMN_CHOOSER (user_data), path);
    gtk_tree_path_free (path);
}

static void
view_row_activated_callback (GtkTreeView       *tree_view,
                             GtkTreePath       *path,
                             GtkTreeViewColumn *column,
                             gpointer           user_data)
{
    toggle_path (NAUTILUS_COLUMN_CHOOSER (user_data), path);
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            gpointer          user_data)
{
    update_buttons (NAUTILUS_COLUMN_CHOOSER (user_data));
}

static void
row_deleted_callback (GtkTreeModel *model,
                      GtkTreePath  *path,
                      gpointer      user_data)
{
    list_changed (NAUTILUS_COLUMN_CHOOSER (user_data));
}

static void
move_up_clicked_callback (GtkWidget *button,
                          gpointer   user_data)
{
    NautilusColumnChooser *chooser;
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    chooser = NAUTILUS_COLUMN_CHOOSER (user_data);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->view));

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GtkTreePath *path;
        GtkTreeIter prev;

        path = gtk_tree_model_get_path (GTK_TREE_MODEL (chooser->store), &iter);
        gtk_tree_path_prev (path);
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->store), &prev, path))
        {
            gtk_list_store_move_before (chooser->store,
                                        &iter,
                                        &prev);
        }
        gtk_tree_path_free (path);
    }

    list_changed (chooser);
}

static void
move_down_clicked_callback (GtkWidget *button,
                            gpointer   user_data)
{
    NautilusColumnChooser *chooser;
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    chooser = NAUTILUS_COLUMN_CHOOSER (user_data);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->view));

    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GtkTreeIter next;

        next = iter;

        if (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->store), &next))
        {
            gtk_list_store_move_after (chooser->store,
                                       &iter,
                                       &next);
        }
    }

    list_changed (chooser);
}

static void
use_default_clicked_callback (GtkWidget *button,
                              gpointer   user_data)
{
    g_signal_emit (NAUTILUS_COLUMN_CHOOSER (user_data),
                   signals[USE_DEFAULT], 0);
}

static void
populate_tree (NautilusColumnChooser *chooser)
{
    GList *columns;
    GList *l;

    columns = nautilus_get_columns_for_file (chooser->file);

    for (l = columns; l != NULL; l = l->next)
    {
        GtkTreeIter iter;
        NautilusColumn *column;
        char *name;
        char *label;
        gboolean visible = FALSE;
        gboolean sensitive = TRUE;

        column = NAUTILUS_COLUMN (l->data);

        g_object_get (G_OBJECT (column),
                      "name", &name, "label", &label,
                      NULL);

        if (strcmp (name, "name") == 0)
        {
            visible = TRUE;
            sensitive = FALSE;
        }
        if (strcmp (name, "starred") == 0)
        {
            g_free (name);
            g_free (label);
            continue;
        }

        gtk_list_store_append (chooser->store, &iter);
        gtk_list_store_set (chooser->store, &iter,
                            COLUMN_VISIBLE, visible,
                            COLUMN_LABEL, label,
                            COLUMN_NAME, name,
                            COLUMN_SENSITIVE, sensitive,
                            -1);

        g_free (name);
        g_free (label);
    }

    nautilus_column_list_free (columns);
}

static void
nautilus_column_chooser_constructed (GObject *object)
{
    NautilusColumnChooser *chooser;

    G_OBJECT_CLASS (nautilus_column_chooser_parent_class)->constructed (object);

    chooser = NAUTILUS_COLUMN_CHOOSER (object);

    populate_tree (chooser);

    g_signal_connect (chooser->store, "row-deleted",
                      G_CALLBACK (row_deleted_callback), chooser);
}

static void
set_visible_columns (NautilusColumnChooser  *chooser,
                     char                  **visible_columns)
{
    GHashTable *visible_columns_hash;
    GtkTreeIter iter;
    int i;

    visible_columns_hash = g_hash_table_new (g_str_hash, g_str_equal);
    /* always show the name column */
    g_hash_table_insert (visible_columns_hash, "name", "name");
    for (i = 0; visible_columns[i] != NULL; ++i)
    {
        g_hash_table_insert (visible_columns_hash,
                             visible_columns[i],
                             visible_columns[i]);
    }

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser->store),
                                       &iter))
    {
        do
        {
            char *name;
            gboolean visible;

            gtk_tree_model_get (GTK_TREE_MODEL (chooser->store),
                                &iter,
                                COLUMN_NAME, &name,
                                -1);

            visible = (g_hash_table_lookup (visible_columns_hash, name) != NULL);

            gtk_list_store_set (chooser->store,
                                &iter,
                                COLUMN_VISIBLE, visible,
                                -1);
            g_free (name);
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->store), &iter));
    }

    g_hash_table_destroy (visible_columns_hash);
}

static char **
get_column_names (NautilusColumnChooser *chooser,
                  gboolean               only_visible)
{
    GPtrArray *ret;
    GtkTreeIter iter;

    ret = g_ptr_array_new ();
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser->store),
                                       &iter))
    {
        do
        {
            char *name;
            gboolean visible;
            gtk_tree_model_get (GTK_TREE_MODEL (chooser->store),
                                &iter,
                                COLUMN_VISIBLE, &visible,
                                COLUMN_NAME, &name,
                                -1);
            if (!only_visible || visible)
            {
                /* give ownership to the array */
                g_ptr_array_add (ret, name);
            }
            else
            {
                g_free (name);
            }
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->store), &iter));
    }
    g_ptr_array_add (ret, NULL);

    return (char **) g_ptr_array_free (ret, FALSE);
}

static gboolean
get_column_iter (NautilusColumnChooser *chooser,
                 NautilusColumn        *column,
                 GtkTreeIter           *iter)
{
    char *column_name;

    g_object_get (NAUTILUS_COLUMN (column), "name", &column_name, NULL);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser->store),
                                       iter))
    {
        do
        {
            char *name;

            gtk_tree_model_get (GTK_TREE_MODEL (chooser->store),
                                iter,
                                COLUMN_NAME, &name,
                                -1);
            if (!strcmp (name, column_name))
            {
                g_free (column_name);
                g_free (name);
                return TRUE;
            }

            g_free (name);
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser->store), iter));
    }
    g_free (column_name);
    return FALSE;
}

static void
set_column_order (NautilusColumnChooser  *chooser,
                  char                  **column_order)
{
    GList *columns;
    GList *l;
    GtkTreePath *path;

    columns = nautilus_get_columns_for_file (chooser->file);
    columns = nautilus_sort_columns (columns, column_order);

    g_signal_handlers_block_by_func (chooser->store,
                                     G_CALLBACK (row_deleted_callback),
                                     chooser);

    path = gtk_tree_path_new_first ();
    for (l = columns; l != NULL; l = l->next)
    {
        GtkTreeIter iter;

        if (get_column_iter (chooser, NAUTILUS_COLUMN (l->data), &iter))
        {
            GtkTreeIter before;
            if (path)
            {
                gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->store),
                                         &before, path);
                gtk_list_store_move_after (chooser->store,
                                           &iter, &before);
                gtk_tree_path_next (path);
            }
            else
            {
                gtk_list_store_move_after (chooser->store,
                                           &iter, NULL);
            }
        }
    }
    gtk_tree_path_free (path);
    g_signal_handlers_unblock_by_func (chooser->store,
                                       G_CALLBACK (row_deleted_callback),
                                       chooser);

    nautilus_column_list_free (columns);
}

void
nautilus_column_chooser_set_settings (NautilusColumnChooser  *chooser,
                                      char                  **visible_columns,
                                      char                  **column_order)
{
    g_return_if_fail (NAUTILUS_IS_COLUMN_CHOOSER (chooser));
    g_return_if_fail (visible_columns != NULL);
    g_return_if_fail (column_order != NULL);

    set_visible_columns (chooser, visible_columns);
    set_column_order (chooser, column_order);

    list_changed (chooser);
}

void
nautilus_column_chooser_get_settings (NautilusColumnChooser   *chooser,
                                      char                  ***visible_columns,
                                      char                  ***column_order)
{
    g_return_if_fail (NAUTILUS_IS_COLUMN_CHOOSER (chooser));
    g_return_if_fail (visible_columns != NULL);
    g_return_if_fail (column_order != NULL);

    *visible_columns = get_column_names (chooser, TRUE);
    *column_order = get_column_names (chooser, FALSE);
}

static void
nautilus_column_chooser_class_init (NautilusColumnChooserClass *chooser_class)
{
    GtkWidgetClass *widget_class;
    GObjectClass *oclass;

    widget_class = GTK_WIDGET_CLASS (chooser_class);
    oclass = G_OBJECT_CLASS (chooser_class);

    oclass->set_property = nautilus_column_chooser_set_property;
    oclass->constructed = nautilus_column_chooser_constructed;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-column-chooser.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusColumnChooser, view);
    gtk_widget_class_bind_template_child (widget_class, NautilusColumnChooser, store);
    gtk_widget_class_bind_template_child (widget_class, NautilusColumnChooser, move_up_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusColumnChooser, move_down_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusColumnChooser, use_default_button);
    gtk_widget_class_bind_template_callback (widget_class, view_row_activated_callback);
    gtk_widget_class_bind_template_callback (widget_class, selection_changed_callback);
    gtk_widget_class_bind_template_callback (widget_class, visible_toggled_callback);
    gtk_widget_class_bind_template_callback (widget_class, move_up_clicked_callback);
    gtk_widget_class_bind_template_callback (widget_class, move_down_clicked_callback);
    gtk_widget_class_bind_template_callback (widget_class, use_default_clicked_callback);

    signals[CHANGED] = g_signal_new
                           ("changed",
                           G_TYPE_FROM_CLASS (chooser_class),
                           G_SIGNAL_RUN_LAST,
                           0, NULL, NULL,
                           g_cclosure_marshal_VOID__VOID,
                           G_TYPE_NONE, 0);

    signals[USE_DEFAULT] = g_signal_new
                               ("use-default",
                               G_TYPE_FROM_CLASS (chooser_class),
                               G_SIGNAL_RUN_LAST,
                               0, NULL, NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    g_object_class_install_property (oclass,
                                     PROP_FILE,
                                     g_param_spec_object ("file",
                                                          "File",
                                                          "The file this column chooser is for",
                                                          NAUTILUS_TYPE_FILE,
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_WRITABLE));
}

static void
nautilus_column_chooser_init (NautilusColumnChooser *chooser)
{
    gtk_widget_init_template (GTK_WIDGET (chooser));
}

GtkWidget *
nautilus_column_chooser_new (NautilusFile *file)
{
    return g_object_new (NAUTILUS_TYPE_COLUMN_CHOOSER, "file", file, NULL);
}
