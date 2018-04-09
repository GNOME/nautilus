/* nautilus-action-bar-box.c
 *
 * Copyright 2018 Carlos Soriano <csoriano@redhat.com>
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

#include "nautilus-action-bar-box.h"

#define INFO_SECTION_WIDTH_RATIO 0.3
#define INFO_SECTION_CHILD_POSITION_INDEX 0
#define ACTION_SECTION_CHILD_POSITION_INDEX 1

struct _NautilusActionBarBox
{
    GtkBox parent_instance;
};

G_DEFINE_TYPE (NautilusActionBarBox, nautilus_action_bar_box, GTK_TYPE_BOX)

enum {
    PROP_0,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

NautilusActionBarBox *
nautilus_action_bar_box_new (void)
{
    return g_object_new (NAUTILUS_TYPE_ACTION_BAR_BOX, NULL);
}

static void
nautilus_action_bar_box_finalize (GObject *object)
{
    NautilusActionBarBox *self = (NautilusActionBarBox *)object;

    G_OBJECT_CLASS (nautilus_action_bar_box_parent_class)->finalize (object);
}

static void
nautilus_action_bar_box_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    NautilusActionBarBox *self = NAUTILUS_ACTION_BAR_BOX (object);

    switch (prop_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_action_bar_box_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    NautilusActionBarBox *self = NAUTILUS_ACTION_BAR_BOX (object);

    switch (prop_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_action_bar_box_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
    NautilusActionBarBox *self = NAUTILUS_ACTION_BAR_BOX (widget);
    g_autoptr (GList) children = NULL;
    GList *child;
    GtkRequisition minimum_size;
    GtkRequisition natural_size;
    gint available_width = 0;
    gint margin_sizes = 0;
    GtkRequestedSize *sizes;
    gint n_visible_children;
    GtkAllocation child_allocation;
    gint current_x = 0;
    gint i;

    children = gtk_container_get_children (GTK_CONTAINER (self));

    /* Only the info section and the action section should be added in this container */
    g_return_if_fail (g_list_length (children) == 2);
    n_visible_children = 2;

    sizes = g_newa (GtkRequestedSize, g_list_length (children));
    available_width = allocation->width;

    gtk_widget_get_preferred_size (widget, &minimum_size, &natural_size);

    gtk_widget_set_allocation (widget, allocation);

    for (child = children, i = 0; child != NULL; child = child->next, i++)
    {
        gtk_widget_get_preferred_width_for_height (child->data,
                                                   allocation->height,
                                                   &sizes[i].minimum_size,
                                                   &sizes[i].natural_size);
        sizes[i].data = child->data;
        margin_sizes += gtk_widget_get_margin_end (child->data) +
                        gtk_widget_get_margin_start (child->data);
    }

    g_print ("allocation width %d %d\n", allocation->width, margin_sizes);
    available_width -= margin_sizes;
    if ((sizes[INFO_SECTION_CHILD_POSITION_INDEX].natural_size +
         sizes[ACTION_SECTION_CHILD_POSITION_INDEX].natural_size)
        < available_width)
    {
        /* Info section */
        sizes[INFO_SECTION_CHILD_POSITION_INDEX].minimum_size = MAX (INFO_SECTION_WIDTH_RATIO * available_width,
                                                                     sizes[INFO_SECTION_CHILD_POSITION_INDEX].minimum_size);
        sizes[INFO_SECTION_CHILD_POSITION_INDEX].natural_size = sizes[INFO_SECTION_CHILD_POSITION_INDEX].minimum_size;
    g_print ("info %d\n", sizes[INFO_SECTION_CHILD_POSITION_INDEX].minimum_size);

        /* Actions section */
        sizes[ACTION_SECTION_CHILD_POSITION_INDEX].minimum_size = MAX ((1.0 - INFO_SECTION_WIDTH_RATIO) * available_width,
                                                                       sizes[ACTION_SECTION_CHILD_POSITION_INDEX].minimum_size);
        sizes[ACTION_SECTION_CHILD_POSITION_INDEX].natural_size = sizes[ACTION_SECTION_CHILD_POSITION_INDEX].minimum_size;
    g_print ("action %d\n", sizes[ACTION_SECTION_CHILD_POSITION_INDEX].minimum_size);
    }
    available_width -= sizes[INFO_SECTION_CHILD_POSITION_INDEX].minimum_size +
                       sizes[ACTION_SECTION_CHILD_POSITION_INDEX].minimum_size;
    g_print ("available width %d\n", available_width);

    gtk_distribute_natural_allocation (MAX (0, available_width),
                                       n_visible_children, sizes);

    for (child = children, i = 0; child != NULL; child = child->next, i++)
    {
        child_allocation.x = current_x + gtk_widget_get_margin_start (child->data);
        child_allocation.y = allocation->y;
        child_allocation.width = sizes[i].minimum_size;
        child_allocation.height = allocation->height;

        g_print ("child alloc %d %d %d %d %d\n", current_x, child_allocation.x, sizes[i].minimum_size, gtk_widget_get_margin_end (child->data), gtk_widget_get_margin_start (child->data));
        gtk_widget_size_allocate (child->data, &child_allocation);

        current_x += sizes[i].minimum_size + gtk_widget_get_margin_end (child->data);
    }
}

static void
nautilus_action_bar_box_class_init (NautilusActionBarBoxClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_action_bar_box_finalize;
    object_class->get_property = nautilus_action_bar_box_get_property;
    object_class->set_property = nautilus_action_bar_box_set_property;

    widget_class->size_allocate = nautilus_action_bar_box_size_allocate;
}

static void
nautilus_action_bar_box_init (NautilusActionBarBox *self)
{
}
