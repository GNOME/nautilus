/* nautilus-view-icon-ui.c
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

#include "nautilus-view-icon-ui.h"
#include "nautilus-view-icon-item-ui.h"
#include "nautilus-view-icon-controller.h"
#include "nautilus-files-view.h"
#include "nautilus-file.h"
#include "nautilus-directory.h"
#include "nautilus-global-preferences.h"

struct _NautilusViewIconUi
{
    GtkFlowBox parent_instance;

    NautilusViewIconController *controller;
};

G_DEFINE_TYPE (NautilusViewIconUi, nautilus_view_icon_ui, GTK_TYPE_FLOW_BOX)

enum
{
    PROP_0,
    PROP_CONTROLLER,
    N_PROPS
};

static void
set_controller (NautilusViewIconUi         *self,
                NautilusViewIconController *controller)
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
    NautilusViewIconUi *self = NAUTILUS_VIEW_ICON_UI (object);

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
    NautilusViewIconUi *self = NAUTILUS_VIEW_ICON_UI (object);

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
nautilus_view_icon_ui_set_selection (NautilusViewIconUi *self,
                                     GQueue             *selection)
{
    NautilusViewItemModel *item_model;
    NautilusViewModel *model;
    GListStore *gmodel;
    gint i = 0;

    model = nautilus_view_icon_controller_get_model (self->controller);
    gmodel = nautilus_view_model_get_g_model (model);
    while ((item_model = NAUTILUS_VIEW_ITEM_MODEL (g_list_model_get_item (G_LIST_MODEL (gmodel), i))))
    {
        GtkWidget *item_ui;

        item_ui = nautilus_view_item_model_get_item_ui (item_model);
        if (g_queue_find (selection, item_model) != NULL)
        {
            gtk_flow_box_select_child (GTK_FLOW_BOX (self),
                                       GTK_FLOW_BOX_CHILD (item_ui));
        }
        else
        {
            gtk_flow_box_unselect_child (GTK_FLOW_BOX (self),
                                         GTK_FLOW_BOX_CHILD (item_ui));
        }

        i++;
    }
}


static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
    NautilusViewItemModel *item_model = NAUTILUS_VIEW_ITEM_MODEL (item);
    NautilusViewIconItemUi *child;

    child = nautilus_view_icon_item_ui_new (item_model);
    nautilus_view_item_model_set_item_ui (item_model, GTK_WIDGET (child));
    gtk_widget_show (GTK_WIDGET (child));

    return GTK_WIDGET (child);
}

static void
on_child_activated (GtkFlowBox      *flow_box,
                    GtkFlowBoxChild *child,
                    gpointer         user_data)
{
    NautilusViewIconUi *self = NAUTILUS_VIEW_ICON_UI (user_data);
    NautilusViewItemModel *item_model;
    NautilusFile *file;
    g_autoptr (GList) list = NULL;
    GdkEvent *event;
    guint keyval;
    gboolean is_preview = FALSE;

    item_model = nautilus_view_icon_item_ui_get_model (NAUTILUS_VIEW_ICON_ITEM_UI (child));
    file = nautilus_view_item_model_get_file (item_model);
    list = g_list_append (list, file);

    event = gtk_get_current_event ();
    if (event && gdk_event_get_keyval (event, &keyval))
    {
        if (keyval == GDK_KEY_space)
        {
            is_preview = TRUE;
        }
    }

    if (is_preview)
    {
        nautilus_files_view_preview_files (NAUTILUS_FILES_VIEW (self->controller), list, NULL);
    }
    else
    {
        nautilus_files_view_activate_files (NAUTILUS_FILES_VIEW (self->controller), list, 0, TRUE);
    }

    g_clear_pointer (&event, gdk_event_free);
}

static void
on_ui_selected_children_changed (GtkFlowBox *box,
                                 gpointer    user_data)
{
    NautilusViewIconUi *self;

    self = NAUTILUS_VIEW_ICON_UI (user_data);
    nautilus_files_view_notify_selection_changed (NAUTILUS_FILES_VIEW (self->controller));
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_view_icon_ui_parent_class)->finalize (object);
}

static void
constructed (GObject *object)
{
    NautilusViewIconUi *self = NAUTILUS_VIEW_ICON_UI (object);
    NautilusViewModel *model;
    GListStore *gmodel;

    G_OBJECT_CLASS (nautilus_view_icon_ui_parent_class)->constructed (object);

    gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (self), FALSE);
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self), 20);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self), GTK_SELECTION_MULTIPLE);
    gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self), FALSE);
    gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
    gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);
    gtk_widget_set_margin_top (GTK_WIDGET (self), 10);
    gtk_widget_set_margin_start (GTK_WIDGET (self), 10);
    gtk_widget_set_margin_bottom (GTK_WIDGET (self), 10);
    gtk_widget_set_margin_end (GTK_WIDGET (self), 10);

    model = nautilus_view_icon_controller_get_model (self->controller);
    gmodel = nautilus_view_model_get_g_model (model);
    gtk_flow_box_bind_model (GTK_FLOW_BOX (self),
                             G_LIST_MODEL (gmodel),
                             create_widget_func, self, NULL);

    g_signal_connect (self, "child-activated", (GCallback) on_child_activated, self);
    g_signal_connect (self, "selected-children-changed", (GCallback) on_ui_selected_children_changed, self);
}

static void
nautilus_view_icon_ui_class_init (NautilusViewIconUiClass *klass)
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
                                                          NAUTILUS_TYPE_VIEW_ICON_CONTROLLER,
                                                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
nautilus_view_icon_ui_init (NautilusViewIconUi *self)
{
}

NautilusViewIconUi *
nautilus_view_icon_ui_new (NautilusViewIconController *controller)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ICON_UI,
                         "controller", controller,
                         NULL);
}
