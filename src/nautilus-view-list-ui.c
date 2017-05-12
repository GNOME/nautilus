/* nautilus-view-list-ui.c
 *
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
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
 */

#include <config.h>
#include <glib.h>

#include "nautilus-view-list-ui.h"
#include "nautilus-view-list-item-ui.h"
#include "nautilus-view-list-controller.h"
#include "nautilus-files-view.h"
#include "nautilus-file.h"
#include "nautilus-directory.h"
#include "nautilus-global-preferences.h"

struct _NautilusViewListUi
{
    GtkListBox parent_instance;

    NautilusViewListController *controller;
};

G_DEFINE_TYPE (NautilusViewListUi, nautilus_view_list_ui, GTK_TYPE_LIST_BOX)

enum
{
    PROP_0,
    PROP_CONTROLLER,
    N_PROPS
};

static void
set_controller (NautilusViewListUi         *self,
                NautilusViewListController *controller)
{
    self->controller = controller;

    g_object_notify (G_OBJECT (self), "controller");
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    NautilusViewListUi *self = NAUTILUS_VIEW_LIST_UI (object);

    switch (prop_id)
    {
        case PROP_CONTROLLER:
        {
            g_value_set_object (value, self->controller);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusViewListUi *self = NAUTILUS_VIEW_LIST_UI (object);

    switch (prop_id)
    {
        case PROP_CONTROLLER:
        {
            set_controller (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

void
nautilus_view_list_ui_set_selection (NautilusViewListUi *self,
                                     GQueue             *selection)
{
    NautilusViewItemModel *item_model;
    NautilusViewModel *model;
    GListStore *gmodel;
    gint i = 0;

    model = nautilus_view_list_controller_get_model (self->controller);
    gmodel = nautilus_view_model_get_g_model (model);
    while ((item_model = NAUTILUS_VIEW_ITEM_MODEL (g_list_model_get_item (G_LIST_MODEL (gmodel), i))))
    {
        GtkWidget *item_ui;

        item_ui = nautilus_view_item_model_get_item_ui (item_model);
        if (g_queue_find (selection, item_model) != NULL)
        {
            gtk_list_box_select_row (GTK_LIST_BOX (self),
                                     GTK_LIST_BOX_ROW (item_ui));
        }
        else
        {
            gtk_list_box_unselect_row (GTK_LIST_BOX (self),
                                       GTK_LIST_BOX_ROW (item_ui));
        }

        i++;
    }
}


static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
    NautilusViewItemModel *item_model = NAUTILUS_VIEW_ITEM_MODEL (item);
    NautilusViewListItemUi *child;

    child = nautilus_view_list_item_ui_new (item_model);
    nautilus_view_item_model_set_item_ui (item_model, GTK_WIDGET (child));
    gtk_widget_show (GTK_WIDGET (child));

    return GTK_WIDGET (child);
}

static void
on_child_activated (GtkListBox      *list_box,
                    GtkListBoxRow   *child,
                    gpointer         user_data)
{
    NautilusViewListUi *self = NAUTILUS_VIEW_LIST_UI (user_data);
    NautilusViewItemModel *item_model;
    NautilusFile *file;
    g_autoptr (GList) list = NULL;

    item_model = nautilus_view_list_item_ui_get_model (NAUTILUS_VIEW_LIST_ITEM_UI (child));
    file = nautilus_view_item_model_get_file (item_model);
    list = g_list_append (list, file);

    nautilus_files_view_activate_files (NAUTILUS_FILES_VIEW (self->controller), list, 0, TRUE);
}

static void
on_ui_selected_children_changed (GtkListBox *box,
                                 gpointer    user_data)
{
    NautilusViewListUi *self;

    self = NAUTILUS_VIEW_LIST_UI (user_data);
    nautilus_files_view_notify_selection_changed (NAUTILUS_FILES_VIEW (self->controller));
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_view_list_ui_parent_class)->finalize (object);
}

static void
constructed (GObject *object)
{
    NautilusViewListUi *self = NAUTILUS_VIEW_LIST_UI (object);
    NautilusViewModel *model;
    GListStore *gmodel;

    G_OBJECT_CLASS (nautilus_view_list_ui_parent_class)->constructed (object);

    gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (self), FALSE);
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self), GTK_SELECTION_MULTIPLE);
    gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);

    model = nautilus_view_list_controller_get_model (self->controller);
    gmodel = nautilus_view_model_get_g_model (model);
    gtk_list_box_bind_model (GTK_LIST_BOX (self),
                             G_LIST_MODEL (gmodel),
                             create_widget_func, self, NULL);

    g_signal_connect (self, "row-activated", (GCallback) on_child_activated, self);
    g_signal_connect (self, "selected-rows-changed", (GCallback) on_ui_selected_children_changed, self);
}

static void
nautilus_view_list_ui_class_init (NautilusViewListUiClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = finalize;
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->constructed = constructed;

    g_object_class_install_property (object_class,
                                     PROP_CONTROLLER,
                                     g_param_spec_object ("controller",
                                                          "Controller",
                                                          "The controller of the view",
                                                          NAUTILUS_TYPE_VIEW_LIST_CONTROLLER,
                                                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
nautilus_view_list_ui_init (NautilusViewListUi *self)
{
}

NautilusViewListUi *
nautilus_view_list_ui_new (NautilusViewListController *controller)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_LIST_UI,
                         "controller", controller,
                         NULL);
}
