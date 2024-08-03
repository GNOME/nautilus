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

#include "nautilus-preferences-dialog.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <nautilus-extension.h>

#include "nautilus-column-utilities.h"
#include "nautilus-date-utilities.h"
#include "nautilus-global-preferences.h"

/* bool preferences */
#define NAUTILUS_PREFERENCES_DIALOG_FOLDERS_FIRST_WIDGET                       \
        "sort_folders_first_row"
#define NAUTILUS_PREFERENCES_DIALOG_DELETE_PERMANENTLY_WIDGET                  \
        "show_delete_permanently_row"
#define NAUTILUS_PREFERENCES_DIALOG_CREATE_LINK_WIDGET                         \
        "show_create_link_row"
#define NAUTILUS_PREFERENCES_DIALOG_LIST_VIEW_USE_TREE_WIDGET                  \
        "use_tree_view_row"

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

static void
create_icon_caption_combo_row_items (AdwComboRow *combo_row,
                                     GList       *columns)
{
    g_autoptr (GListStore) list_store = g_list_store_new (NAUTILUS_TYPE_COLUMN);
    g_autoptr (NautilusColumn) none = NULL;
    g_autoptr (GtkExpression) expression = NULL;
    GList *l;

    expression = gtk_property_expression_new (NAUTILUS_TYPE_COLUMN, NULL, "label");
    adw_combo_row_set_expression (combo_row, expression);

    none = g_object_new (NAUTILUS_TYPE_COLUMN,
                         "name", "none",
                         /* Translators: this is referred to captions under icons. */
                         "label", _("None"),
                         NULL);
    g_list_store_append (list_store, none);

    for (l = columns; l != NULL; l = l->next)
    {
        NautilusColumn *column;
        g_autofree char *name = NULL;

        column = NAUTILUS_COLUMN (l->data);

        g_object_get (G_OBJECT (column), "name", &name, NULL);

        /* Don't show name here, it doesn't make sense
         * starred is instead shown as an emblem for the grid view
         */
        if (!strcmp (name, "name") || !strcmp (name, "starred"))
        {
            continue;
        }

        g_list_store_append (list_store, column);
    }
    adw_combo_row_set_model (combo_row, G_LIST_MODEL (list_store));
}

static void
icon_captions_changed_callback (AdwComboRow *widget,
                                GParamSpec  *pspec,
                                gpointer     user_data)
{
    g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();
    g_auto (GStrv) captions = NULL;
    GPtrArray *combo_rows = (GPtrArray *) user_data;

    for (int i = 0; icon_captions_components[i] != NULL; i++)
    {
        GtkWidget *combo_row;
        GObject *selected_column;
        g_autofree char *name = NULL;

        combo_row = g_ptr_array_index (combo_rows, i);
        selected_column = adw_combo_row_get_selected_item (ADW_COMBO_ROW (combo_row));
        if (G_UNLIKELY (!NAUTILUS_IS_COLUMN (selected_column)))
        {
            g_warn_if_reached ();
            continue;
        }

        g_object_get (selected_column, "name", &name, NULL);
        g_strv_builder_add (builder, name);
    }
    captions = g_strv_builder_end (builder);

    g_settings_set_strv (nautilus_icon_view_preferences,
                         NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
                         (const char **) captions);
}

static void
update_caption_combo_row (GPtrArray  *combo_rows,
                          int         combo_row_i,
                          const char *name)
{
    AdwComboRow *combo_row;
    GListModel *model;
    guint n_columns;

    combo_row = ADW_COMBO_ROW (g_ptr_array_index (combo_rows, combo_row_i));
    model = adw_combo_row_get_model (combo_row);
    n_columns = g_list_model_get_n_items (model);

    g_signal_handlers_block_by_func (
        combo_row, G_CALLBACK (icon_captions_changed_callback), combo_rows);

    for (guint i = 0; i < n_columns; ++i)
    {
        g_autoptr (NautilusColumn) column_i = g_list_model_get_item (model, i);
        g_autofree char *name_i = NULL;

        g_object_get (column_i, "name", &name_i, NULL);
        if (g_strcmp0 (name, name_i) == 0)
        {
            adw_combo_row_set_selected (ADW_COMBO_ROW (combo_row), i);
            break;
        }
    }

    g_signal_handlers_unblock_by_func (
        combo_row, G_CALLBACK (icon_captions_changed_callback), combo_rows);
}

static void
update_icon_captions_from_settings (GPtrArray *combo_rows)
{
    g_auto (GStrv) captions = NULL;
    int i, j;

    captions = g_settings_get_strv (nautilus_icon_view_preferences,
                                    NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);
    if (captions == NULL)
    {
        return;
    }

    for (i = 0, j = 0; icon_captions_components[i] != NULL; i++)
    {
        const char *data;

        if (captions[j])
        {
            data = captions[j];
            ++j;
        }
        else
        {
            data = "none";
        }

        update_caption_combo_row (combo_rows, i, data);
    }
}

static void
nautilus_preferences_dialog_setup_icon_caption_page (GtkBuilder *builder)
{
    g_autoptr (GPtrArray) combo_rows = g_ptr_array_sized_new (G_N_ELEMENTS (icon_captions_components));
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
        g_ptr_array_add (combo_rows, combo_row);

        create_icon_caption_combo_row_items (ADW_COMBO_ROW (combo_row), columns);
        gtk_widget_set_sensitive (combo_row, writable);

        g_signal_connect_data (
            combo_row, "notify::selected", G_CALLBACK (icon_captions_changed_callback),
            g_ptr_array_ref (combo_rows), (GClosureNotify) g_ptr_array_unref, 0);
    }

    nautilus_column_list_free (columns);

    update_icon_captions_from_settings (combo_rows);
}

static void
bind_builder_bool (GtkBuilder *builder,
                   GSettings  *settings,
                   const char *widget_name,
                   const char *prefs)
{
    g_settings_bind (settings, prefs, gtk_builder_get_object (builder, widget_name),
                     "active", G_SETTINGS_BIND_DEFAULT);
}

/* Translators: Both %s will be replaced with formatted timestamps. */
#define DATE_FORMAT_ROW_SUBTITLE _("Examples: “%s”, “%s”")

static void
setup_detailed_date (GtkBuilder *builder)
{
    AdwActionRow *simple_row = ADW_ACTION_ROW (gtk_builder_get_object (builder, "date_format_simple_row"));
    AdwActionRow *detailed_row = ADW_ACTION_ROW (gtk_builder_get_object (builder, "date_format_detailed_row"));

    g_autoptr (GDateTime) now = g_date_time_new_now_local ();
    g_autoptr (GDateTime) earlier = g_date_time_add_days (now, -3);

    g_autofree gchar *simple_date_now = nautilus_date_preview_detailed_format (now, FALSE);
    g_autofree gchar *simple_date_earlier = nautilus_date_preview_detailed_format (earlier, FALSE);
    g_autofree gchar *simple_row_subtitle = g_strdup_printf (DATE_FORMAT_ROW_SUBTITLE,
                                                             simple_date_now,
                                                             simple_date_earlier);
    adw_action_row_set_subtitle (simple_row, simple_row_subtitle);

    g_autofree gchar *detailed_date_now = nautilus_date_preview_detailed_format (now, TRUE);
    g_autofree gchar *detailed_date_earlier = nautilus_date_preview_detailed_format (earlier, TRUE);
    g_autofree gchar *detailed_row_subtitle = g_strdup_printf (DATE_FORMAT_ROW_SUBTITLE,
                                                               detailed_date_now,
                                                               detailed_date_earlier);
    adw_action_row_set_subtitle (detailed_row, detailed_row_subtitle);
}

static GVariant *
combo_row_mapping_set (const GValue       *gvalue,
                       const GVariantType *expected_type,
                       gpointer            user_data)
{
    const gchar **values = user_data;

    return g_variant_new_string (values[g_value_get_uint (gvalue)]);
}

static gboolean
combo_row_mapping_get (GValue   *gvalue,
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
            g_value_set_uint (gvalue, i);

            return TRUE;
        }
    }

    return FALSE;
}

static void
bind_builder_combo_row (GtkBuilder  *builder,
                        GSettings   *settings,
                        const char  *widget_name,
                        const char  *prefs,
                        const char **values)
{
    g_settings_bind_with_mapping (settings, prefs, gtk_builder_get_object (builder, widget_name),
                                  "selected", G_SETTINGS_BIND_DEFAULT,
                                  combo_row_mapping_get, combo_row_mapping_set,
                                  (gpointer) values, NULL);
}

static void
setup_combo (GtkBuilder  *builder,
             const char  *widget_name,
             const char **strings)
{
    AdwComboRow *combo_row;
    g_autoptr (GtkStringList) list_store = NULL;

    combo_row = (AdwComboRow *) gtk_builder_get_object (builder, widget_name);
    g_assert (ADW_IS_COMBO_ROW (combo_row));

    list_store = gtk_string_list_new (strings);
    adw_combo_row_set_model (combo_row, G_LIST_MODEL (list_store));
}

static void
nautilus_preferences_dialog_setup (GtkBuilder *builder)
{
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_OPEN_ACTION_COMBO,
                 (const char *[]) { _("Single-Click"), _("Double-Click"), NULL });
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_SEARCH_RECURSIVE_ROW,
                 (const char *[]) { _("On This Device Only"), _("All Locations"), _("Never"), NULL });
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_THUMBNAILS_ROW,
                 (const char *[]) { _("On This Device Only"), _("All Files"), _("Never"), NULL });
    setup_combo (builder, NAUTILUS_PREFERENCES_DIALOG_COUNT_ROW,
                 (const char *[]) { _("On This Device Only"), _("All Folders"), _("Never"), NULL });

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

    setup_detailed_date (builder);

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

    nautilus_preferences_dialog_setup_icon_caption_page (builder);
}

void
nautilus_preferences_dialog_show (GtkWidget *parent)
{
    static AdwPreferencesDialog *preferences_dialog = NULL;
    g_autoptr (GtkBuilder) builder = NULL;
    g_autoptr (GSimpleActionGroup) action_group = g_simple_action_group_new ();
    g_autoptr (GAction) date_time_action = NULL;

    if (preferences_dialog != NULL)
    {
        /* Destroy existing window, which might be hidden behind other windows,
         * attached to another parent. */
        adw_dialog_force_close (ADW_DIALOG (preferences_dialog));
    }

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-preferences-dialog.ui");
    nautilus_preferences_dialog_setup (builder);

    preferences_dialog = ADW_PREFERENCES_DIALOG (gtk_builder_get_object (builder, "preferences_dialog"));
    g_object_add_weak_pointer (G_OBJECT (preferences_dialog), (gpointer *) &preferences_dialog);

    date_time_action = g_settings_create_action (nautilus_preferences,
                                                 NAUTILUS_PREFERENCES_DATE_TIME_FORMAT);
    g_action_map_add_action (G_ACTION_MAP (action_group), date_time_action);
    gtk_widget_insert_action_group (GTK_WIDGET (preferences_dialog),
                                    "preferences",
                                    G_ACTION_GROUP (action_group));

    adw_dialog_present (ADW_DIALOG (preferences_dialog), parent);
}
