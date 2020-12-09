/* nautilus-preferences-window.c - Functions to create and show the nautilus
 *  preference window.
 *
 *  Copyright (C) 2002 Jan Arne Petersen
 *  Copyright (C) 2016 Carlos Soriano <csoriano@gnome.com>
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
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
 */

#include <config.h>

#include "nautilus-preferences-window.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libhandy-1/handy.h>

#include <glib/gi18n.h>

#include <nautilus-extension.h>

#include "nautilus-column-utilities.h"
#include "nautilus-global-preferences.h"

/* bool preferences */
#define NAUTILUS_PREFERENCES_DIALOG_FOLDERS_FIRST_WIDGET                       \
    "sort_folders_first_switch"
#define NAUTILUS_PREFERENCES_DIALOG_DELETE_PERMANENTLY_WIDGET                  \
    "show_delete_permanently_switch"
#define NAUTILUS_PREFERENCES_DIALOG_CREATE_LINK_WIDGET                         \
    "show_create_link_switch"
#define NAUTILUS_PREFERENCES_DIALOG_LIST_VIEW_USE_TREE_WIDGET                  \
    "use_tree_view_switch"

/* combo preferences */
#define NAUTILUS_PREFERENCES_DIALOG_OPEN_ACTION_COMBO                          \
    "open_action_row"
#define NAUTILUS_PREFERENCES_DIALOG_SEARCH_RECURSIVE_ROW                       \
    "search_recursive_row"
#define NAUTILUS_PREFERENCES_DIALOG_THUMBNAILS_ROW                       \
    "thumbnails_row"
#define NAUTILUS_PREFERENCES_DIALOG_COUNT_ROW                       \
    "count_row"

static const char * const speed_tradeoff_values[] =
{
    "local-only", "always", "never",
    NULL
};

static const char * const click_behavior_values[] = {"single", "double", NULL};

static const char * const icon_captions_components[] =
{
    "captions_0_comborow", "captions_1_comborow", "captions_2_comborow", NULL
};

static GtkWidget *preferences_window = NULL;

static void list_store_append_string (GListStore  *list_store,
                                      const gchar *string)
{
    g_autoptr (HdyValueObject) obj = hdy_value_object_new_string (string);
    g_list_store_append (list_store, obj);
}

static void free_column_names_array(GPtrArray *column_names)
{
    g_ptr_array_foreach (column_names, (GFunc) g_free, NULL);
    g_ptr_array_free (column_names, TRUE);
}

static void create_icon_caption_combo_row_items(HdyComboRow *combo_row,
                                                GList       *columns)
{
    GListStore *list_store = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
    GList *l;
    GPtrArray *column_names;

    column_names = g_ptr_array_new ();

    /* Translators: this is referred to captions under icons. */
    list_store_append_string (list_store, _("None"));
    g_ptr_array_add (column_names, g_strdup ("none"));

    for (l = columns; l != NULL; l = l->next)
    {
        NautilusColumn *column;
        char *name;
        char *label;

        column = NAUTILUS_COLUMN (l->data);

        g_object_get (G_OBJECT (column), "name", &name, "label", &label, NULL);

        /* Don't show name here, it doesn't make sense */
        if (!strcmp (name, "name"))
        {
            g_free (name);
            g_free (label);
            continue;
        }

        list_store_append_string (list_store, label);
        g_ptr_array_add (column_names, name);

        g_free (label);
    }
    hdy_combo_row_bind_name_model (combo_row, G_LIST_MODEL (list_store),
                                   (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                   NULL, NULL);
    g_object_set_data_full (G_OBJECT (combo_row), "column_names", column_names,
                            (GDestroyNotify) free_column_names_array);
}

static void icon_captions_changed_callback(HdyComboRow *widget,
                                           GParamSpec  *pspec,
                                           gpointer     user_data)
{
    GPtrArray *captions;
    GtkBuilder *builder;
    int i;

    builder = GTK_BUILDER (user_data);

    captions = g_ptr_array_new ();

    for (i = 0; icon_captions_components[i] != NULL; i++)
    {
        GtkWidget *combo_row;
        int selected_index;
        GPtrArray *column_names;
        char *name;

        combo_row = GTK_WIDGET (
            gtk_builder_get_object (builder, icon_captions_components[i]));
        selected_index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (combo_row));

        column_names = g_object_get_data (G_OBJECT (combo_row), "column_names");

        name = g_ptr_array_index (column_names, selected_index);
        g_ptr_array_add (captions, name);
    }
    g_ptr_array_add (captions, NULL);

    g_settings_set_strv (nautilus_icon_view_preferences,
                         NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
                         (const char **) captions->pdata);
    g_ptr_array_free (captions, TRUE);
}

static void update_caption_combo_row(GtkBuilder *builder,
                                     const char *combo_row_name,
                                     const char *name)
{
    GtkWidget *combo_row;
    int i;
    GPtrArray *column_names;

    combo_row = GTK_WIDGET (gtk_builder_get_object (builder, combo_row_name));

    g_signal_handlers_block_by_func (
        combo_row, G_CALLBACK (icon_captions_changed_callback), builder);

    column_names = g_object_get_data (G_OBJECT (combo_row), "column_names");

    for (i = 0; i < column_names->len; ++i)
    {
        if (!strcmp (name, g_ptr_array_index (column_names, i)))
        {
            hdy_combo_row_set_selected_index (HDY_COMBO_ROW (combo_row), i);
            break;
        }
    }

    g_signal_handlers_unblock_by_func (
        combo_row, G_CALLBACK (icon_captions_changed_callback), builder);
}

static void update_icon_captions_from_settings(GtkBuilder *builder)
{
    char **captions;
    int i, j;

    captions = g_settings_get_strv (nautilus_icon_view_preferences,
                                    NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);
    if (captions == NULL)
    {
        return;
    }

    for (i = 0, j = 0; icon_captions_components[i] != NULL; i++)
    {
        char *data;

        if (captions[j])
        {
            data = captions[j];
            ++j;
        }
        else
        {
            data = "none";
        }

        update_caption_combo_row (builder, icon_captions_components[i], data);
    }

    g_strfreev (captions);
}

static void
nautilus_preferences_window_setup_icon_caption_page (GtkBuilder *builder)
{
    GList *columns;
    int i;
    gboolean writable;

    writable = g_settings_is_writable (nautilus_icon_view_preferences,
                                       NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

    columns = nautilus_get_common_columns ();

    for (i = 0; icon_captions_components[i] != NULL; i++)
    {
        GtkWidget *combo_row;

        combo_row = GTK_WIDGET (
            gtk_builder_get_object (builder, icon_captions_components[i]));

        create_icon_caption_combo_row_items (HDY_COMBO_ROW (combo_row), columns);
        gtk_widget_set_sensitive (combo_row, writable);

        g_signal_connect_data (
            combo_row, "notify::selected-index", G_CALLBACK (icon_captions_changed_callback),
            g_object_ref (builder), (GClosureNotify) g_object_unref, 0);
    }

    nautilus_column_list_free (columns);

    update_icon_captions_from_settings (builder);
}

static void bind_builder_bool(GtkBuilder *builder,
                              GSettings  *settings,
                              const char *widget_name,
                              const char *prefs)
{
    g_settings_bind (settings, prefs, gtk_builder_get_object (builder, widget_name),
                     "active", G_SETTINGS_BIND_DEFAULT);
}

static GVariant *combo_row_mapping_set(const GValue       *gvalue,
                                       const GVariantType *expected_type,
                                       gpointer            user_data)
{
    const gchar **values = user_data;

    return g_variant_new_string (values[g_value_get_int (gvalue)]);
}

static gboolean combo_row_mapping_get(GValue   *gvalue,
                                      GVariant *variant,
                                      gpointer  user_data)
{
    const gchar **values = user_data;
    const gchar *value;

    value = g_variant_get_string (variant, NULL);

    for (int i = 0; values[i]; i++)
    {
        if (g_strcmp0 (value, values[i]) == 0)
        {
            g_value_set_int (gvalue, i);

            return TRUE;
        }
    }

    return FALSE;
}

static void bind_builder_combo_row(GtkBuilder  *builder,
                                   GSettings   *settings,
                                   const char  *widget_name,
                                   const char  *prefs,
                                   const char **values)
{
    g_settings_bind_with_mapping (settings, prefs, gtk_builder_get_object (builder, widget_name),
                                  "selected-index", G_SETTINGS_BIND_DEFAULT,
                                  combo_row_mapping_get, combo_row_mapping_set,
                                  (gpointer) values, NULL);
}

static void setup_combo (GtkBuilder  *builder,
                         const char  *widget_name,
                         const char **strings)
{
    HdyComboRow *combo_row;
    GListStore *list_store;

    combo_row = (HdyComboRow *) gtk_builder_get_object (builder, widget_name);
    g_assert (HDY_IS_COMBO_ROW (combo_row));

    list_store = g_list_store_new (HDY_TYPE_VALUE_OBJECT);

    for (gsize i = 0; strings[i]; i++)
    {
        list_store_append_string (list_store, strings[i]);
    }

    hdy_combo_row_bind_name_model (combo_row, G_LIST_MODEL (list_store), (HdyComboRowGetNameFunc) hdy_value_object_dup_string, NULL, NULL);
}

static void nautilus_preferences_window_setup(GtkBuilder *builder,
                                              GtkWindow  *parent_window)
{
    GtkWidget *window;

    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_OPEN_ACTION_COMBO,
                 (const char *[]) { _("Single click"), _("Double click"), NULL });
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_SEARCH_RECURSIVE_ROW,
                 (const char *[]) { _("On this computer only"), _("All locations"), _("Never"), NULL });
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_THUMBNAILS_ROW,
                 (const char *[]) { _("On this computer only"), _("All files"), _("Never"), NULL });
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_COUNT_ROW,
                 (const char *[]) { _("On this computer only"), _("All folders"), _("Never"), NULL });

    /* setup preferences */
    bind_builder_bool (builder, gtk_filechooser_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_FOLDERS_FIRST_WIDGET,
                       NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
    bind_builder_bool (builder, nautilus_list_view_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_LIST_VIEW_USE_TREE_WIDGET,
                       NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);
    bind_builder_bool (builder, nautilus_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_CREATE_LINK_WIDGET,
                       NAUTILUS_PREFERENCES_SHOW_CREATE_LINK);
    bind_builder_bool (builder, nautilus_preferences,
                       NAUTILUS_PREFERENCES_DIALOG_DELETE_PERMANENTLY_WIDGET,
                       NAUTILUS_PREFERENCES_SHOW_DELETE_PERMANENTLY);

    bind_builder_combo_row (builder, nautilus_preferences,
                            NAUTILUS_PREFERENCES_DIALOG_OPEN_ACTION_COMBO,
                            NAUTILUS_PREFERENCES_CLICK_POLICY,
                            (const char **) click_behavior_values);
    bind_builder_combo_row (builder, nautilus_preferences,
                            NAUTILUS_PREFERENCES_DIALOG_SEARCH_RECURSIVE_ROW,
                            NAUTILUS_PREFERENCES_RECURSIVE_SEARCH,
                            (const char **) speed_tradeoff_values);
    bind_builder_combo_row (builder, nautilus_preferences,
                            NAUTILUS_PREFERENCES_DIALOG_THUMBNAILS_ROW,
                            NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS,
                            (const char **) speed_tradeoff_values);
    bind_builder_combo_row (builder, nautilus_preferences,
                            NAUTILUS_PREFERENCES_DIALOG_COUNT_ROW,
                            NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                            (const char **) speed_tradeoff_values);

    nautilus_preferences_window_setup_icon_caption_page (builder);

    /* UI callbacks */
    window = GTK_WIDGET (gtk_builder_get_object (builder, "preferences_window"));
    preferences_window = window;

    gtk_window_set_icon_name (GTK_WINDOW (preferences_window), APPLICATION_ID);

    g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &preferences_window);

    gtk_window_set_transient_for (GTK_WINDOW (preferences_window), parent_window);

    gtk_widget_show (preferences_window);
}

void nautilus_preferences_window_show(GtkWindow *window)
{
    GtkBuilder *builder;

    if (preferences_window != NULL)
    {
        gtk_window_present (GTK_WINDOW (preferences_window));
        return;
    }

    builder = gtk_builder_new ();

    gtk_builder_add_from_resource (
        builder, "/org/gnome/nautilus/ui/nautilus-preferences-window.ui", NULL);

    nautilus_preferences_window_setup (builder, window);

    g_object_unref (builder);
}
