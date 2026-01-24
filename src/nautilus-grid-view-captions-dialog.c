/*
 * Copyright © 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-grid-view-captions-dialog.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include <nautilus-column-utilities.h>
#include <nautilus-extension.h>
#include <nautilus-global-preferences.h>
#include <nautilus-types.h>

struct _NautilusGridViewCaptionsDialog
{
    AdwDialog parent;

    AdwComboRow *captions_0_comborow;
    AdwComboRow *captions_1_comborow;
    AdwComboRow *captions_2_comborow;
};

G_DEFINE_FINAL_TYPE (NautilusGridViewCaptionsDialog, nautilus_grid_view_captions_dialog, ADW_TYPE_DIALOG);

static void
create_grid_caption_combo_row_items (AdwComboRow *combo_row,
                                     GList       *columns)
{
    g_autoptr (GListStore) list_store = g_list_store_new (NAUTILUS_TYPE_COLUMN);
    g_autoptr (NautilusColumn) none = g_object_new (NAUTILUS_TYPE_COLUMN,
                                                    "name", "none",
                                                    /* Translators: this is referred to captions under icons. */
                                                    "label", _("None"),
                                                    NULL);
    g_autoptr (GtkExpression) expression = gtk_property_expression_new (NAUTILUS_TYPE_COLUMN,
                                                                        NULL,
                                                                        "label");

    adw_combo_row_set_expression (combo_row, expression);
    g_list_store_append (list_store, none);

    for (GList *l = columns; l != NULL; l = l->next)
    {
        NautilusColumn *column = NAUTILUS_COLUMN (l->data);
        g_autofree char *name = NULL;

        g_object_get (G_OBJECT (column), "name", &name, NULL);

        /* Don't show name here, it doesn't make sense.
         * Starred is instead shown as an emblem for the grid view.
         */
        if (g_strcmp0 (name, "name") == 0 ||
            g_strcmp0 (name, "starred") == 0)
        {
            continue;
        }

        g_list_store_append (list_store, column);
    }

    adw_combo_row_set_model (combo_row, G_LIST_MODEL (list_store));
}

static void
captions_changed_callback (AdwComboRow *widget,
                           GParamSpec  *pspec,
                           gpointer     user_data)
{
    g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();
    g_auto (GStrv) captions = NULL;
    GPtrArray *combo_rows = user_data;

    for (uint i = 0; i < combo_rows->len; i++)
    {
        GtkWidget *combo_row = g_ptr_array_index (combo_rows, i);
        GObject *selected_column = adw_combo_row_get_selected_item (ADW_COMBO_ROW (combo_row));
        g_autofree char *name = NULL;

        if (G_UNLIKELY (!NAUTILUS_IS_COLUMN (selected_column)))
        {
            g_warn_if_reached ();

            continue;
        }

        g_object_get (selected_column, "name", &name, NULL);
        g_strv_builder_take (builder, g_steal_pointer (&name));
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
    AdwComboRow *combo_row = ADW_COMBO_ROW (g_ptr_array_index (combo_rows, combo_row_i));
    GListModel *model = adw_combo_row_get_model (combo_row);
    guint n_columns = g_list_model_get_n_items (model);

    g_signal_handlers_block_by_func (combo_row, G_CALLBACK (captions_changed_callback), combo_rows);

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

    g_signal_handlers_unblock_by_func (combo_row,
                                       G_CALLBACK (captions_changed_callback),
                                       combo_rows);
}

static void
update_grid_captions_from_settings (GPtrArray *combo_rows)
{
    g_auto (GStrv) captions = g_settings_get_strv (nautilus_icon_view_preferences,
                                                   NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

    if (captions == NULL)
    {
        return;
    }

    for (uint i = 0, j = 0; i < combo_rows->len; i++)
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
nautilus_preferences_dialog_setup_icon_caption_page (GPtrArray *combo_rows)
{
    GList *columns = nautilus_get_common_columns ();
    gboolean writable = g_settings_is_writable (nautilus_icon_view_preferences,
                                                NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

    for (uint i = 0; i < combo_rows->len; i++)
    {
        GtkWidget *combo_row = GTK_WIDGET (g_ptr_array_index (combo_rows, i));

        create_grid_caption_combo_row_items (ADW_COMBO_ROW (combo_row), columns);
        gtk_widget_set_sensitive (combo_row, writable);

        g_signal_connect_data (combo_row, "notify::selected",
                               G_CALLBACK (captions_changed_callback), g_ptr_array_ref (combo_rows),
                               (GClosureNotify) g_ptr_array_unref, 0);
    }

    nautilus_column_list_free (columns);

    update_grid_captions_from_settings (combo_rows);
}

static void
nautilus_grid_view_captions_dialog_dispose (GObject *object)
{
    NautilusGridViewCaptionsDialog *self = NAUTILUS_GRID_VIEW_CAPTIONS_DIALOG (object);

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_GRID_VIEW_CAPTIONS_DIALOG);

    G_OBJECT_CLASS (nautilus_grid_view_captions_dialog_parent_class)->dispose (object);
}

static void
nautilus_grid_view_captions_dialog_class_init (NautilusGridViewCaptionsDialogClass *self_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (self_class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (self_class);

    object_class->dispose = nautilus_grid_view_captions_dialog_dispose;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-grid-view-captions-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusGridViewCaptionsDialog, captions_0_comborow);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridViewCaptionsDialog, captions_1_comborow);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridViewCaptionsDialog, captions_2_comborow);

    gtk_widget_class_bind_template_callback (widget_class, captions_changed_callback);
}

static void
nautilus_grid_view_captions_dialog_init (NautilusGridViewCaptionsDialog *self)
{
    g_autoptr (GPtrArray) combo_rows = g_ptr_array_sized_new (3);

    gtk_widget_init_template (GTK_WIDGET (self));

    g_ptr_array_add (combo_rows, self->captions_0_comborow);
    g_ptr_array_add (combo_rows, self->captions_1_comborow);
    g_ptr_array_add (combo_rows, self->captions_2_comborow);

    nautilus_preferences_dialog_setup_icon_caption_page (combo_rows);
}

void
nautilus_grid_view_captions_dialog_present (GtkWidget *parent)
{
    NautilusGridViewCaptionsDialog *self = g_object_new (NAUTILUS_TYPE_GRID_VIEW_CAPTIONS_DIALOG,
                                                         NULL);

    adw_dialog_present (ADW_DIALOG (self), parent);
}
