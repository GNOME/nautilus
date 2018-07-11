/* nautilus-pathbar.c
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "nautilus-pathbar.h"
#include "nautilus-properties-window.h"

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-names.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"

#include "nautilus-window-slot-dnd.h"

enum
{
    OPEN_LOCATION,
    PATH_CLICKED,
    LAST_SIGNAL
};

typedef enum
{
    NORMAL_BUTTON,
    OTHER_LOCATIONS_BUTTON,
    ROOT_BUTTON,
    HOME_BUTTON,
    STARRED_BUTTON,
    RECENT_BUTTON,
    MOUNT_BUTTON,
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *) (x))

#define SCROLL_TIMEOUT           150
#define INITIAL_SCROLL_TIMEOUT   300

static guint path_bar_signals [LAST_SIGNAL] = { 0 };

#define NAUTILUS_PATH_BAR_ICON_SIZE 16
#define NAUTILUS_PATH_BAR_BUTTON_MAX_WIDTH 250

typedef struct
{
    GtkWidget *button;
    ButtonType type;
    char *dir_name;
    GFile *path;
    NautilusFile *file;
    unsigned int file_changed_signal_id;

    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *bold_label;
    GtkWidget *separator;
    GtkWidget *disclosure_arrow;
    GtkWidget *container;

    NautilusPathBar *path_bar;

    guint ignore_changes : 1;
    guint is_root : 1;
} ButtonData;

typedef struct
{
    GFile *current_path;
    gpointer current_button_data;

    GList *button_list;
    GList *first_scrolled_button;
    GtkWidget *up_slider_button;
    GtkWidget *down_slider_button;
    guint settings_signal_id;
    guint timer;
    guint slider_visible : 1;
    guint need_timer : 1;
    guint ignore_click : 1;

    unsigned int drag_slider_timeout;
    gboolean drag_slider_timeout_for_up_button;

    GtkPopover *current_view_menu_popover;
    GMenu *current_view_menu;
    GMenu *extensions_background_menu;
    GMenu *templates_menu;
} NautilusPathBarPrivate;


G_DEFINE_TYPE_WITH_PRIVATE (NautilusPathBar, nautilus_path_bar,
                            GTK_TYPE_CONTAINER);

static void nautilus_path_bar_scroll_up (NautilusPathBar *self);
static void nautilus_path_bar_scroll_down (NautilusPathBar *self);
static void nautilus_path_bar_stop_scrolling (NautilusPathBar *self);
static void on_long_press_gesture_pressed (GtkGestureLongPress *gesture,
                                           gdouble              x,
                                           gdouble              y,
                                           gpointer             user_data);
static void on_long_press_gesture_cancelled (GtkGestureLongPress *gesture,
                                             gpointer             user_data);
static void nautilus_path_bar_check_icon_theme (NautilusPathBar *self);
static void nautilus_path_bar_update_button_appearance (ButtonData *button_data);
static void nautilus_path_bar_update_button_state (ButtonData *button_data,
                                                   gboolean    current_dir);
static void nautilus_path_bar_update_path (NautilusPathBar *self,
                                           GFile           *file_path);

static GtkWidget *
get_slider_button (NautilusPathBar *self,
                   const gchar     *arrow_type)
{
    GtkWidget *button;

    button = gtk_button_new ();
    gtk_widget_set_focus_on_click (button, FALSE);
    gtk_container_add (GTK_CONTAINER (button), gtk_image_new_from_icon_name (arrow_type));
    gtk_container_add (GTK_CONTAINER (self), button);

    return button;
}

static gboolean
slider_timeout (gpointer user_data)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;

    self = NAUTILUS_PATH_BAR (user_data);
    priv = nautilus_path_bar_get_instance_private (self);

    priv->drag_slider_timeout = 0;

    if (gtk_widget_get_visible (GTK_WIDGET (self)))
    {
        if (priv->drag_slider_timeout_for_up_button)
        {
            nautilus_path_bar_scroll_up (self);
        }
        else
        {
            nautilus_path_bar_scroll_down (self);
        }
    }

    return FALSE;
}

static void
nautilus_path_bar_slider_drag_motion (GtkWidget *widget,
                                      GdkDrop   *drop,
                                      int        x,
                                      int        y,
                                      gpointer   user_data)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    GtkSettings *settings;
    unsigned int timeout;

    self = NAUTILUS_PATH_BAR (user_data);
    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->drag_slider_timeout == 0)
    {
        settings = gtk_widget_get_settings (widget);

        g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);
        priv->drag_slider_timeout =
            g_timeout_add (timeout,
                           slider_timeout,
                           self);

        priv->drag_slider_timeout_for_up_button =
            widget == priv->up_slider_button;
    }
}

static void
nautilus_path_bar_slider_drag_leave (GtkWidget *widget,
                                     GdkDrop   *drop,
                                     gpointer   user_data)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;

    self = NAUTILUS_PATH_BAR (user_data);
    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->drag_slider_timeout != 0)
    {
        g_source_remove (priv->drag_slider_timeout);
        priv->drag_slider_timeout = 0;
    }
}

static void
on_event_controller_scroll_scroll (GtkEventControllerScroll *controller,
                                   double                    dx,
                                   double                    dy,
                                   gpointer                  user_data)
{
    GtkWidget *widget;
    NautilusPathBar *self;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    self = NAUTILUS_PATH_BAR (widget);

    if (dx < 0 || dy < 0)
    {
        nautilus_path_bar_scroll_down (self);
    }
    else if (dx > 0 || dy > 0)
    {
        nautilus_path_bar_scroll_up (self);
    }
}

static void
nautilus_path_bar_init (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    GtkBuilder *builder;
    GtkGesture *gesture;
    GtkEventController *controller;

    priv = nautilus_path_bar_get_instance_private (self);

    /* Context menu */
    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-pathbar-context-menu.ui");
    priv->current_view_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "current-view-menu")));
    priv->current_view_menu_popover = GTK_POPOVER (gtk_popover_new_from_model (NULL,
                                                                              G_MENU_MODEL (priv->current_view_menu)));
    g_object_unref (builder);

    gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

    priv->up_slider_button = get_slider_button (self, "pan-start-symbolic");
    priv->down_slider_button = get_slider_button (self, "pan-end-symbolic");
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (priv->up_slider_button)),
                                 "slider-button");
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (priv->down_slider_button)),
                                 "slider-button");

    g_signal_connect_swapped (priv->up_slider_button, "clicked", G_CALLBACK (nautilus_path_bar_scroll_up), self);
    g_signal_connect_swapped (priv->down_slider_button, "clicked", G_CALLBACK (nautilus_path_bar_scroll_down), self);

    gesture = gtk_gesture_long_press_new ();

    gtk_widget_add_controller (priv->up_slider_button, GTK_EVENT_CONTROLLER (gesture));

    g_signal_connect (gesture, "pressed",
                      G_CALLBACK (on_long_press_gesture_pressed), self);
    g_signal_connect (gesture, "cancelled",
                      G_CALLBACK (on_long_press_gesture_cancelled), self);

    gesture = gtk_gesture_long_press_new ();

    gtk_widget_add_controller (priv->down_slider_button, GTK_EVENT_CONTROLLER (gesture));

    g_signal_connect (gesture, "pressed",
                      G_CALLBACK (on_long_press_gesture_pressed), self);
    g_signal_connect (gesture, "cancelled",
                      G_CALLBACK (on_long_press_gesture_cancelled), self);

    gtk_drag_dest_set (GTK_WIDGET (priv->up_slider_button),
                       0, NULL, 0);
    gtk_drag_dest_set_track_motion (GTK_WIDGET (priv->up_slider_button), TRUE);
    g_signal_connect (priv->up_slider_button,
                      "drag-motion",
                      G_CALLBACK (nautilus_path_bar_slider_drag_motion),
                      self);
    g_signal_connect (priv->up_slider_button,
                      "drag-leave",
                      G_CALLBACK (nautilus_path_bar_slider_drag_leave),
                      self);

    gtk_drag_dest_set (GTK_WIDGET (priv->down_slider_button),
                       0, NULL, 0);
    gtk_drag_dest_set_track_motion (GTK_WIDGET (priv->down_slider_button), TRUE);
    g_signal_connect (priv->down_slider_button,
                      "drag-motion",
                      G_CALLBACK (nautilus_path_bar_slider_drag_motion),
                      self);
    g_signal_connect (priv->down_slider_button,
                      "drag-leave",
                      G_CALLBACK (nautilus_path_bar_slider_drag_leave),
                      self);

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)),
                                 GTK_STYLE_CLASS_LINKED);
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)),
                                 "path-bar");

    controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES |
                                                  GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);

    gtk_widget_add_controller (GTK_WIDGET (self), controller);

    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);

    g_signal_connect (controller,
                      "scroll", G_CALLBACK (on_event_controller_scroll_scroll),
                      NULL);
}

static void
nautilus_path_bar_finalize (GObject *object)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;

    self = NAUTILUS_PATH_BAR (object);
    priv = nautilus_path_bar_get_instance_private (self);

    nautilus_path_bar_stop_scrolling (self);

    if (priv->drag_slider_timeout != 0)
    {
        g_source_remove (priv->drag_slider_timeout);
        priv->drag_slider_timeout = 0;
    }

    g_list_free (priv->button_list);
    g_clear_object (&priv->current_view_menu);

    G_OBJECT_CLASS (nautilus_path_bar_parent_class)->finalize (object);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (NautilusPathBar *self,
                        GdkDisplay      *display)
{
    NautilusPathBarPrivate *priv;

    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->settings_signal_id != 0)
    {
        GtkSettings *settings;

        settings = gtk_settings_get_for_display (display);
        g_signal_handler_disconnect (settings, priv->settings_signal_id);
        priv->settings_signal_id = 0;
    }
}

static void
nautilus_path_bar_dispose (GObject *object)
{
    NautilusPathBar *self;

    self = NAUTILUS_PATH_BAR (object);

    remove_settings_signal (self, gtk_widget_get_display (GTK_WIDGET (object)));

    G_OBJECT_CLASS (nautilus_path_bar_parent_class)->dispose (object);
}

static const char *
get_dir_name (ButtonData *button_data)
{
    switch (button_data->type)
    {
        case HOME_BUTTON:
        {
            return _("Home");
        }

        case OTHER_LOCATIONS_BUTTON:
        {
            return _("Other Locations");
        }

        case STARRED_BUTTON:
        {
            return _("Starred");
        }

        default:
        {
            return button_data->dir_name;
        }
    }
}

/* We always want to request the same size for the label, whether
 * or not the contents are bold
 */
static void
set_label_size_request (ButtonData *button_data)
{
    gint width, height;
    GtkRequisition nat_req, bold_req;

    if (button_data->label == NULL)
    {
        return;
    }

    gtk_widget_get_preferred_size (button_data->label, NULL, &nat_req);
    gtk_widget_get_preferred_size (button_data->bold_label, &bold_req, NULL);

    width = MAX (nat_req.width, bold_req.width);
    width = MIN (width, NAUTILUS_PATH_BAR_BUTTON_MAX_WIDTH);
    height = MAX (nat_req.height, bold_req.height);

    gtk_widget_set_size_request (button_data->label, width, height);
}

/* Size requisition:
 *
 * Ideally, our size is determined by another widget, and we are just filling
 * available space.
 */
static void
nautilus_path_bar_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
    NautilusPathBar *self = NULL;
    NautilusPathBarPrivate *priv = NULL;
    ButtonData *button_data = NULL;
    GtkRequisition child_minimum = { 0 };
    GtkRequisition child_natural = { 0 };
    int minimum_size = 0;
    int natural_size = 0;

    self = NAUTILUS_PATH_BAR (widget);
    priv = nautilus_path_bar_get_instance_private (self);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        GtkRequisition down_button_minimum;
        GtkRequisition up_button_minimum;

        for (GList *list = priv->button_list; list != NULL; list = list->next)
        {
            button_data = BUTTON_DATA (list->data);

            set_label_size_request (button_data);

            gtk_widget_get_preferred_size (button_data->container,
                                           &child_minimum, &child_natural);

            if (button_data->type == NORMAL_BUTTON)
            {
                /* Use 2*Height as button width because of ellipsized label.  */
                child_minimum.width = MAX (child_minimum.width, child_minimum.height * 2);
                child_natural.width = MAX (child_minimum.width, child_minimum.height * 2);
            }

            minimum_size = MAX (minimum_size, child_minimum.width);
            natural_size += child_natural.width;
        }

        /* Add space for slider, if we have more than one path */
        /* Theoretically, the slider could be bigger than the other button.  But we're
         * not going to worry about that now.
         */
        gtk_widget_get_preferred_size (priv->down_slider_button,
                                       &down_button_minimum,
                                       NULL);
        gtk_widget_get_preferred_size (priv->up_slider_button,
                                       &up_button_minimum,
                                       NULL);

        if (priv->button_list)
        {
            minimum_size += down_button_minimum.width + up_button_minimum.width;
            natural_size += down_button_minimum.width + up_button_minimum.width;
        }
    }
    else
    {
        for (GList *list = priv->button_list; list != NULL; list = list->next)
        {
            button_data = BUTTON_DATA (list->data);
            set_label_size_request (button_data);

            gtk_widget_get_preferred_size (button_data->container,
                                           &child_minimum, &child_natural);

            minimum_size = MAX (minimum_size, child_minimum.height);
            natural_size = MAX (natural_size, child_natural.height);
        }
    }

    if (minimum != NULL)
    {
        *minimum = minimum_size;
    }
    if (natural != NULL)
    {
        *natural = natural_size;
    }
}

static void
nautilus_path_bar_update_slider_buttons (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;

    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->button_list)
    {
        GtkWidget *container;

        container = BUTTON_DATA (priv->button_list->data)->container;
        if (gtk_widget_get_child_visible (container))
        {
            gtk_widget_set_sensitive (priv->down_slider_button, FALSE);
        }
        else
        {
            gtk_widget_set_sensitive (priv->down_slider_button, TRUE);
        }
        container = BUTTON_DATA (g_list_last (priv->button_list)->data)->container;
        if (gtk_widget_get_child_visible (container))
        {
            gtk_widget_set_sensitive (priv->up_slider_button, FALSE);
        }
        else
        {
            gtk_widget_set_sensitive (priv->up_slider_button, TRUE);
        }
    }
}

static void
nautilus_path_bar_unmap (GtkWidget *widget)
{
    nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));

    GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->unmap (widget);
}

#define BUTTON_BOTTOM_SHADOW 1

/* This is a tad complicated */
static void
nautilus_path_bar_size_allocate (GtkWidget           *widget,
                                 const GtkAllocation *allocation,
                                 int                  baseline)
{
    GtkWidget *child;
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    GtkTextDirection direction;
    GtkRequisition up_button_minimum;
    GtkRequisition down_button_minimum;
    GtkAllocation child_allocation;
    GList *list, *first_button;
    gint width;
    gint largest_width;
    gboolean need_sliders;
    gint up_slider_offset;
    gint down_slider_offset;
    GtkRequisition child_requisition;

    need_sliders = TRUE;
    up_slider_offset = 0;
    down_slider_offset = 0;
    self = NAUTILUS_PATH_BAR (widget);
    priv = nautilus_path_bar_get_instance_private (NAUTILUS_PATH_BAR (widget));

    gtk_widget_set_allocation (widget, allocation);

    /* No path is set so we don't have to allocate anything. */
    if (priv->button_list == NULL)
    {
        return;
    }
    direction = gtk_widget_get_direction (widget);
    gtk_widget_get_preferred_size (priv->up_slider_button,
                                   &up_button_minimum,
                                   NULL);
    gtk_widget_get_preferred_size (priv->down_slider_button,
                                   &down_button_minimum,
                                   NULL);

    /* First, we check to see if we need the scrollbars. */
    width = 0;

    gtk_widget_get_preferred_size (BUTTON_DATA (priv->button_list->data)->container,
                                   &child_requisition, NULL);
    width += child_requisition.width;

    for (list = priv->button_list->next; list; list = list->next)
    {
        child = BUTTON_DATA (list->data)->button;
        gtk_widget_get_preferred_size (child, &child_requisition, NULL);
        width += child_requisition.width;
    }

    if (width <= allocation->width && !need_sliders)
    {
        first_button = g_list_last (priv->button_list);
    }
    else
    {
        gboolean reached_end;
        gint slider_space;
        reached_end = FALSE;
        slider_space = down_button_minimum.width + up_button_minimum.width;

        if (priv->first_scrolled_button)
        {
            first_button = priv->first_scrolled_button;
        }
        else
        {
            first_button = priv->button_list;
        }

        need_sliders = TRUE;
        /* To see how much space we have, and how many buttons we can display.
         * We start at the first button, count forward until hit the new
         * button, then count backwards.
         */
        /* Count down the path chain towards the end. */
        gtk_widget_get_preferred_size (BUTTON_DATA (first_button->data)->container,
                                       &child_requisition, NULL);
        width = child_requisition.width;
        list = first_button->prev;
        while (list && !reached_end)
        {
            child = BUTTON_DATA (list->data)->container;
            gtk_widget_get_preferred_size (child, &child_requisition, NULL);

            if (width + child_requisition.width + slider_space > allocation->width)
            {
                reached_end = TRUE;
            }
            else
            {
                width += child_requisition.width;
            }

            list = list->prev;
        }

        /* Finally, we walk up, seeing how many of the previous buttons we can add*/

        while (first_button->next && !reached_end)
        {
            child = BUTTON_DATA (first_button->next->data)->button;
            gtk_widget_get_preferred_size (child, &child_requisition, NULL);

            if (width + child_requisition.width + slider_space > allocation->width)
            {
                reached_end = TRUE;
            }
            else
            {
                width += child_requisition.width;
                first_button = first_button->next;
            }
        }
    }

    /* Now, we allocate space to the buttons */
    child_allocation.y = allocation->y;
    child_allocation.height = allocation->height;

    if (direction == GTK_TEXT_DIR_RTL)
    {
        child_allocation.x = allocation->x + allocation->width;
        if (need_sliders)
        {
            child_allocation.x -= up_button_minimum.width;
            up_slider_offset = allocation->width - up_button_minimum.width;
        }
    }
    else
    {
        child_allocation.x = allocation->x;
        if (need_sliders)
        {
            up_slider_offset = 0;
            child_allocation.x += up_button_minimum.width;
        }
    }

    /* Determine the largest possible allocation size */
    largest_width = allocation->width;
    if (need_sliders)
    {
        largest_width -= (down_button_minimum.width + up_button_minimum.width);
    }

    for (list = first_button; list; list = list->prev)
    {
        child = BUTTON_DATA (list->data)->container;
        gtk_widget_get_preferred_size (child, &child_requisition, NULL);

        child_allocation.width = MIN (child_requisition.width, largest_width);
        if (direction == GTK_TEXT_DIR_RTL)
        {
            child_allocation.x -= child_allocation.width;
        }
        /* Check to see if we've don't have any more space to allocate buttons */
        if (need_sliders && direction == GTK_TEXT_DIR_RTL)
        {
            if (child_allocation.x - down_button_minimum.width < allocation->x)
            {
                break;
            }
        }
        else
        {
            if (need_sliders && direction == GTK_TEXT_DIR_LTR)
            {
                if (child_allocation.x + child_allocation.width + down_button_minimum.width > allocation->x + allocation->width)
                {
                    break;
                }
            }
        }

        gtk_widget_set_child_visible (child, TRUE);
        gtk_widget_size_allocate (child, &child_allocation, -1);

        if (direction == GTK_TEXT_DIR_RTL)
        {
            down_slider_offset = child_allocation.x - allocation->x - down_button_minimum.width;
        }
        else
        {
            down_slider_offset += child_allocation.width;
            child_allocation.x += child_allocation.width;
        }
    }
    /* Now we go hide all the widgets that don't fit */
    while (list)
    {
        child = BUTTON_DATA (list->data)->container;
        gtk_widget_set_child_visible (child, FALSE);
        list = list->prev;
    }
    for (list = first_button->next; list; list = list->next)
    {
        child = BUTTON_DATA (list->data)->container;
        gtk_widget_set_child_visible (child, FALSE);
    }

    if (need_sliders)
    {
        child_allocation.width = up_button_minimum.width;
        child_allocation.x = up_slider_offset + allocation->x;
        gtk_widget_size_allocate (priv->up_slider_button, &child_allocation, -1);

        gtk_widget_set_child_visible (priv->up_slider_button, TRUE);

        if (direction == GTK_TEXT_DIR_LTR)
        {
            down_slider_offset += up_button_minimum.width;
        }
    }
    else
    {
        gtk_widget_set_child_visible (priv->up_slider_button, FALSE);
    }

    if (need_sliders)
    {
        child_allocation.width = down_button_minimum.width;
        child_allocation.x = down_slider_offset + allocation->x;
        gtk_widget_size_allocate (priv->down_slider_button, &child_allocation, -1);

        gtk_widget_set_child_visible (priv->down_slider_button, TRUE);
        nautilus_path_bar_update_slider_buttons (self);
    }
    else
    {
        gtk_widget_set_child_visible (priv->down_slider_button, FALSE);
    }
}

static void
nautilus_path_bar_style_updated (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->style_updated (widget);

    nautilus_path_bar_check_icon_theme (NAUTILUS_PATH_BAR (widget));
}

static void
nautilus_path_bar_display_changed (GtkWidget  *widget,
                                   GdkDisplay *previous_display)
{
    if (GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->display_changed)
    {
        GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->display_changed (widget, previous_display);
    }
    /* We might nave a new settings, so we remove the old one */
    if (previous_display != NULL)
    {
        remove_settings_signal (NAUTILUS_PATH_BAR (widget), previous_display);
    }
    nautilus_path_bar_check_icon_theme (NAUTILUS_PATH_BAR (widget));
}

static void
nautilus_path_bar_add (GtkContainer *container,
                       GtkWidget    *widget)
{
    gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
nautilus_path_bar_remove_1 (GtkContainer *container,
                            GtkWidget    *widget)
{
    gboolean was_visible = gtk_widget_get_visible (widget);
    gtk_widget_unparent (widget);
    if (was_visible)
    {
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
nautilus_path_bar_remove (GtkContainer *container,
                          GtkWidget    *widget)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    GList *children;

    self = NAUTILUS_PATH_BAR (container);
    priv = nautilus_path_bar_get_instance_private (self);

    if (widget == priv->up_slider_button)
    {
        nautilus_path_bar_remove_1 (container, widget);
        priv->up_slider_button = NULL;
        return;
    }

    if (widget == priv->down_slider_button)
    {
        nautilus_path_bar_remove_1 (container, widget);
        priv->down_slider_button = NULL;
        return;
    }

    children = priv->button_list;
    while (children)
    {
        if (widget == BUTTON_DATA (children->data)->container)
        {
            nautilus_path_bar_remove_1 (container, widget);
            priv->button_list = g_list_remove_link (priv->button_list, children);
            g_list_free_1 (children);
            return;
        }
        children = children->next;
    }
}

static void
nautilus_path_bar_forall (GtkContainer *container,
                          gboolean      include_internals,
                          GtkCallback   callback,
                          gpointer      callback_data)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    GList *children;

    g_return_if_fail (callback != NULL);
    self = NAUTILUS_PATH_BAR (container);
    priv = nautilus_path_bar_get_instance_private (self);

    children = priv->button_list;
    while (children)
    {
        GtkWidget *child;
        child = BUTTON_DATA (children->data)->container;
        children = children->next;
        (*callback)(child, callback_data);
    }

    if (priv->up_slider_button)
    {
        (*callback)(priv->up_slider_button, callback_data);
    }

    if (priv->down_slider_button)
    {
        (*callback)(priv->down_slider_button, callback_data);
    }
}

static void
nautilus_path_bar_grab_notify (GtkWidget *widget,
                               gboolean   was_grabbed)
{
    if (!was_grabbed)
    {
        nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
    }
}

static void
nautilus_path_bar_state_changed (GtkWidget    *widget,
                                 GtkStateType  previous_state)
{
    if (!gtk_widget_get_sensitive (widget))
    {
        nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
    }
}

static GtkWidgetPath *
nautilus_path_bar_get_path_for_child (GtkContainer *container,
                                      GtkWidget    *child)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    GtkWidgetPath *path;

    self = NAUTILUS_PATH_BAR (container);
    priv = nautilus_path_bar_get_instance_private (self);
    path = gtk_widget_path_copy (gtk_widget_get_path (GTK_WIDGET (self)));

    if (gtk_widget_get_visible (child) &&
        gtk_widget_get_child_visible (child))
    {
        GtkWidgetPath *sibling_path;
        GList *visible_children;
        GList *l;
        int pos;

        /* 1. Build the list of visible children, in visually left-to-right order
         * (i.e. independently of the widget's direction).  Note that our
         * button_list is stored in innermost-to-outermost path order!
         */

        visible_children = NULL;

        if (gtk_widget_get_visible (priv->down_slider_button) &&
            gtk_widget_get_child_visible (priv->down_slider_button))
        {
            visible_children = g_list_prepend (visible_children, priv->down_slider_button);
        }

        for (l = priv->button_list; l; l = l->next)
        {
            ButtonData *data = l->data;

            if (gtk_widget_get_visible (data->container) &&
                gtk_widget_get_child_visible (data->container))
            {
                visible_children = g_list_prepend (visible_children, data->container);
            }
        }

        if (gtk_widget_get_visible (priv->up_slider_button) &&
            gtk_widget_get_child_visible (priv->up_slider_button))
        {
            visible_children = g_list_prepend (visible_children, priv->up_slider_button);
        }

        if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
        {
            visible_children = g_list_reverse (visible_children);
        }

        /* 2. Find the index of the child within that list */

        pos = 0;

        for (l = visible_children; l; l = l->next)
        {
            GtkWidget *button = l->data;

            if (button == child)
            {
                break;
            }

            pos++;
        }

        /* 3. Build the path */

        sibling_path = gtk_widget_path_new ();

        for (l = visible_children; l; l = l->next)
        {
            gtk_widget_path_append_for_widget (sibling_path, l->data);
        }

        gtk_widget_path_append_with_siblings (path, sibling_path, pos);

        g_list_free (visible_children);
        gtk_widget_path_unref (sibling_path);
    }
    else
    {
        gtk_widget_path_append_for_widget (path, child);
    }

    return path;
}

static void
nautilus_path_bar_class_init (NautilusPathBarClass *path_bar_class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;
    GtkContainerClass *container_class;

    gobject_class = (GObjectClass *) path_bar_class;
    widget_class = (GtkWidgetClass *) path_bar_class;
    container_class = (GtkContainerClass *) path_bar_class;

    gobject_class->finalize = nautilus_path_bar_finalize;
    gobject_class->dispose = nautilus_path_bar_dispose;

    widget_class->measure = nautilus_path_bar_measure;
    widget_class->unmap = nautilus_path_bar_unmap;
    widget_class->size_allocate = nautilus_path_bar_size_allocate;
    widget_class->style_updated = nautilus_path_bar_style_updated;
    widget_class->display_changed = nautilus_path_bar_display_changed;
    widget_class->grab_notify = nautilus_path_bar_grab_notify;
    widget_class->state_changed = nautilus_path_bar_state_changed;

    container_class->add = nautilus_path_bar_add;
    container_class->forall = nautilus_path_bar_forall;
    container_class->remove = nautilus_path_bar_remove;
    container_class->get_path_for_child = nautilus_path_bar_get_path_for_child;

    path_bar_signals [OPEN_LOCATION] =
        g_signal_new ("open-location",
                      G_OBJECT_CLASS_TYPE (path_bar_class),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusPathBarClass, open_location),
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 2,
                      G_TYPE_FILE,
                      GTK_TYPE_PLACES_OPEN_FLAGS);
    path_bar_signals [PATH_CLICKED] =
        g_signal_new ("path-clicked",
                      G_OBJECT_CLASS_TYPE (path_bar_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (NautilusPathBarClass, path_clicked),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      G_TYPE_FILE);

    gtk_container_class_handle_border_width (container_class);
}

static void
update_current_view_menu (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;

    priv = nautilus_path_bar_get_instance_private (self);
    if (priv->extensions_background_menu != NULL)
    {
        nautilus_gmenu_merge (priv->current_view_menu,
                              priv->extensions_background_menu,
                              "extensions",
                              TRUE);
    }

    if (priv->templates_menu != NULL)
    {
        nautilus_gmenu_merge (priv->current_view_menu, priv->templates_menu,
                              "templates-submenu", TRUE);
    }
}

static void
reset_current_view_menu (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    g_autoptr (GtkBuilder) builder = NULL;

    priv = nautilus_path_bar_get_instance_private (self);

    g_clear_object (&priv->current_view_menu);
    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-pathbar-context-menu.ui");
    priv->current_view_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder,
                                                                                 "current-view-menu")));
    gtk_popover_bind_model (priv->current_view_menu_popover,
                            G_MENU_MODEL (priv->current_view_menu), NULL);
}

void
nautilus_path_bar_set_extensions_background_menu (NautilusPathBar *self,
                                                  GMenu           *menu)
{
    NautilusPathBarPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));

    priv = nautilus_path_bar_get_instance_private (self);
    reset_current_view_menu (self);
    g_clear_object (&priv->extensions_background_menu);
    if (menu != NULL)
    {
        priv->extensions_background_menu = g_object_ref (menu);
    }

    update_current_view_menu (self);
}

void
nautilus_path_bar_set_templates_menu (NautilusPathBar *self,
                                      GMenu           *menu)
{
    NautilusPathBarPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));

    priv = nautilus_path_bar_get_instance_private (self);
    reset_current_view_menu (self);
    g_clear_object (&priv->templates_menu);
    if (menu != NULL)
    {
        priv->templates_menu = g_object_ref (menu);
    }

    update_current_view_menu (self);
}

static void
nautilus_path_bar_scroll_down (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    GList *list;
    GList *down_button;
    GList *up_button;
    gint space_available;
    gint space_needed;
    GtkTextDirection direction;
    GtkAllocation allocation, button_allocation, slider_allocation;

    priv = nautilus_path_bar_get_instance_private (self);

    down_button = NULL;
    up_button = NULL;

    if (priv->ignore_click)
    {
        priv->ignore_click = FALSE;
        return;
    }

    gtk_widget_queue_resize (GTK_WIDGET (self));

    direction = gtk_widget_get_direction (GTK_WIDGET (self));

    /* We find the button at the 'down' end that we have to make */
    /* visible */
    for (list = priv->button_list; list; list = list->next)
    {
        if (list->next && gtk_widget_get_child_visible (BUTTON_DATA (list->next->data)->container))
        {
            down_button = list;
            break;
        }
    }

    if (down_button == NULL)
    {
        return;
    }

    /* Find the last visible button on the 'up' end */
    for (list = g_list_last (priv->button_list); list; list = list->prev)
    {
        if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button))
        {
            up_button = list;
            break;
        }
    }

    gtk_widget_get_allocation (BUTTON_DATA (down_button->data)->container, &button_allocation);
    gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
    gtk_widget_get_allocation (priv->down_slider_button, &slider_allocation);

    space_needed = button_allocation.width;
    if (direction == GTK_TEXT_DIR_RTL)
    {
        space_available = slider_allocation.x - allocation.x;
    }
    else
    {
        space_available = (allocation.x + allocation.width) -
                          (slider_allocation.x + slider_allocation.width);
    }

    /* We have space_available extra space that's not being used.  We
     * need space_needed space to make the button fit.  So we walk down
     * from the end, removing buttons until we get all the space we
     * need. */
    gtk_widget_get_allocation (BUTTON_DATA (up_button->data)->button, &button_allocation);
    while ((space_available < space_needed) &&
           (up_button != NULL))
    {
        space_available += button_allocation.width;
        up_button = up_button->prev;
        priv->first_scrolled_button = up_button;
    }
}

static void
nautilus_path_bar_scroll_up (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    GList *list;

    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->ignore_click)
    {
        priv->ignore_click = FALSE;
        return;
    }

    gtk_widget_queue_resize (GTK_WIDGET (self));

    for (list = g_list_last (priv->button_list); list; list = list->prev)
    {
        if (list->prev && gtk_widget_get_child_visible (BUTTON_DATA (list->prev->data)->container))
        {
            priv->first_scrolled_button = list;
            return;
        }
    }
}

static gboolean
nautilus_path_bar_scroll_timeout (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    gboolean retval = FALSE;

    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->timer)
    {
        if (gtk_widget_has_focus (priv->up_slider_button))
        {
            nautilus_path_bar_scroll_up (self);
        }
        else
        {
            if (gtk_widget_has_focus (priv->down_slider_button))
            {
                nautilus_path_bar_scroll_down (self);
            }
        }
        if (priv->need_timer)
        {
            priv->need_timer = FALSE;

            priv->timer =
                g_timeout_add (SCROLL_TIMEOUT,
                               (GSourceFunc) nautilus_path_bar_scroll_timeout,
                               self);
        }
        else
        {
            retval = TRUE;
        }
    }

    return retval;
}

static void
nautilus_path_bar_stop_scrolling (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;

    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->timer)
    {
        g_source_remove (priv->timer);
        priv->timer = 0;
        priv->need_timer = FALSE;
    }
}

static void
on_long_press_gesture_pressed (GtkGestureLongPress *gesture,
                               gdouble              x,
                               gdouble              y,
                               gpointer             user_data)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    GtkWidget *widget;

    self = NAUTILUS_PATH_BAR (user_data);
    priv = nautilus_path_bar_get_instance_private (self);
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

    if (!gtk_widget_has_focus (widget))
    {
        gtk_widget_grab_focus (widget);
    }

    priv->ignore_click = FALSE;

    if (widget == priv->up_slider_button)
    {
        nautilus_path_bar_scroll_up (self);
    }
    else
    {
        if (widget == priv->down_slider_button)
        {
            nautilus_path_bar_scroll_down (self);
        }
    }

    if (!priv->timer)
    {
        priv->need_timer = TRUE;
        priv->timer =
            g_timeout_add (INITIAL_SCROLL_TIMEOUT,
                           (GSourceFunc) nautilus_path_bar_scroll_timeout,
                           self);
    }
}

static void
on_long_press_gesture_cancelled (GtkGestureLongPress *gesture,
                                 gpointer             user_data)
{
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;

    self = NAUTILUS_PATH_BAR (user_data);
    priv = nautilus_path_bar_get_instance_private (self);

    priv->ignore_click = TRUE;
    nautilus_path_bar_stop_scrolling (self);
}


/* Changes the icons wherever it is needed */
static void
reload_icons (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    GList *list;

    priv = nautilus_path_bar_get_instance_private (self);

    for (list = priv->button_list; list; list = list->next)
    {
        ButtonData *button_data;

        button_data = BUTTON_DATA (list->data);
        if (button_data->type != NORMAL_BUTTON || button_data->is_root)
        {
            nautilus_path_bar_update_button_appearance (button_data);
        }
    }
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject         *object,
                    GParamSpec      *pspec,
                    NautilusPathBar *self)
{
    const char *name;

    name = g_param_spec_get_name (pspec);

    if (!strcmp (name, "gtk-icon-theme-name") || !strcmp (name, "gtk-icon-sizes"))
    {
        reload_icons (self);
    }
}

static void
nautilus_path_bar_check_icon_theme (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;
    GtkSettings *settings;

    priv = nautilus_path_bar_get_instance_private (self);

    if (priv->settings_signal_id)
    {
        return;
    }

    settings = gtk_settings_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));
    priv->settings_signal_id = g_signal_connect (settings, "notify", G_CALLBACK (settings_notify_cb), self);

    reload_icons (self);
}

static void
button_data_free (ButtonData *button_data)
{
    g_object_unref (button_data->path);
    g_free (button_data->dir_name);
    if (button_data->file != NULL)
    {
        g_signal_handler_disconnect (button_data->file,
                                     button_data->file_changed_signal_id);
        nautilus_file_monitor_remove (button_data->file, button_data);
        nautilus_file_unref (button_data->file);
    }

    g_free (button_data);
}

/* Public functions and their helpers */
static void
nautilus_path_bar_clear_buttons (NautilusPathBar *self)
{
    NautilusPathBarPrivate *priv;

    priv = nautilus_path_bar_get_instance_private (self);

    while (priv->button_list != NULL)
    {
        ButtonData *button_data;

        button_data = BUTTON_DATA (priv->button_list->data);

        gtk_container_remove (GTK_CONTAINER (self), button_data->container);

        button_data_free (button_data);
    }
    priv->first_scrolled_button = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
                   gpointer   data)
{
    ButtonData *button_data;
    NautilusPathBarPrivate *priv;
    NautilusPathBar *self;
    GdkEvent *event;
    GdkModifierType state;

    button_data = BUTTON_DATA (data);
    if (button_data->ignore_changes)
    {
        return;
    }

    self = button_data->path_bar;
    priv = nautilus_path_bar_get_instance_private (self);
    event = gtk_get_current_event ();

    gdk_event_get_state (event, &state);

    if ((state & GDK_CONTROL_MASK) != 0)
    {
        g_signal_emit (button_data->path_bar, path_bar_signals[OPEN_LOCATION], 0,
                       button_data->path,
                       GTK_PLACES_OPEN_NEW_WINDOW);
    }
    else
    {
        if (g_file_equal (button_data->path, priv->current_path))
        {
            gtk_popover_popup (priv->current_view_menu_popover);
        }
        else
        {
            g_signal_emit (self, path_bar_signals[OPEN_LOCATION], 0,
                           button_data->path,
                           0);
        }
    }
}

static void
on_multi_press_gesture_pressed (GtkGestureMultiPress *gesture,
                                gint                  n_press,
                                gdouble               x,
                                gdouble               y,
                                gpointer              user_data)
{
    GdkEventSequence *sequence;
    const GdkEvent *event;
    GdkModifierType state;

    if (n_press != 1)
    {
        return;
    }

    sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
    event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

    gdk_event_get_state (event, &state);

    state &= gtk_accelerator_get_default_mod_mask ();

    if (state == 0)
    {
        ButtonData *button_data;

        button_data = BUTTON_DATA (user_data);

        g_signal_emit (button_data->path_bar, path_bar_signals[OPEN_LOCATION], 0,
                       button_data->path,
                       GTK_PLACES_OPEN_NEW_TAB);
    }
}

static GIcon *
get_gicon_for_mount (ButtonData *button_data)
{
    GIcon *icon;
    GMount *mount;

    icon = NULL;
    mount = nautilus_get_mounted_mount_for_root (button_data->path);

    if (mount != NULL)
    {
        icon = g_mount_get_symbolic_icon (mount);
        g_object_unref (mount);
    }

    return icon;
}

static GIcon *
get_gicon (ButtonData *button_data)
{
    switch (button_data->type)
    {
        case ROOT_BUTTON:
        {
            return g_themed_icon_new (NAUTILUS_ICON_FILESYSTEM);
        }

        case HOME_BUTTON:
        {
            return g_themed_icon_new (NAUTILUS_ICON_HOME);
        }

        case MOUNT_BUTTON:
        {
            return get_gicon_for_mount (button_data);
        }

        case STARRED_BUTTON:
        {
            return g_themed_icon_new ("starred-symbolic");
        }

        case RECENT_BUTTON:
        {
            return g_themed_icon_new ("document-open-recent-symbolic");
        }

        case OTHER_LOCATIONS_BUTTON:
        {
            return g_themed_icon_new ("list-add-symbolic");
        }

        default:
            return NULL;
    }

    return NULL;
}

static void
nautilus_path_bar_update_button_appearance (ButtonData *button_data)
{
    const gchar *dir_name = get_dir_name (button_data);
    GIcon *icon;

    if (button_data->label != NULL)
    {
        char *markup;

        markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);

        if (gtk_label_get_use_markup (GTK_LABEL (button_data->label)))
        {
            gtk_label_set_markup (GTK_LABEL (button_data->label), markup);
        }
        else
        {
            gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);
        }

        gtk_label_set_markup (GTK_LABEL (button_data->bold_label), markup);
        g_free (markup);
    }

    icon = get_gicon (button_data);
    if (icon != NULL)
    {
        gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), icon);
        gtk_style_context_add_class (gtk_widget_get_style_context (button_data->button),
                                     "image-button");
        gtk_widget_show (GTK_WIDGET (button_data->image));
        g_object_unref (icon);
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (button_data->image));
        gtk_style_context_remove_class (gtk_widget_get_style_context (button_data->button),
                                        "image-button");
    }
}

static void
nautilus_path_bar_update_button_state (ButtonData *button_data,
                                       gboolean    current_dir)
{
    if (button_data->label != NULL)
    {
        gtk_label_set_label (GTK_LABEL (button_data->label), NULL);
        gtk_label_set_label (GTK_LABEL (button_data->bold_label), NULL);
        gtk_label_set_use_markup (GTK_LABEL (button_data->label), current_dir);
    }

    nautilus_path_bar_update_button_appearance (button_data);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)) != current_dir)
    {
        button_data->ignore_changes = TRUE;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_data->button), current_dir);
        button_data->ignore_changes = FALSE;
    }
}

static void
setup_button_type (ButtonData      *button_data,
                   NautilusPathBar *self,
                   GFile           *location)
{
    GMount *mount;
    gchar *uri;

    uri = g_file_get_uri (location);

    if (nautilus_is_root_directory (location))
    {
        button_data->type = ROOT_BUTTON;
    }
    else if (nautilus_is_home_directory (location))
    {
        button_data->type = HOME_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (nautilus_is_recent_directory (location))
    {
        button_data->type = RECENT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (nautilus_is_starred_directory (location))
    {
        button_data->type = STARRED_BUTTON;
        button_data->is_root = TRUE;
    }
    else if ((mount = nautilus_get_mounted_mount_for_root (location)) != NULL)
    {
        button_data->dir_name = g_mount_get_name (mount);
        button_data->type = MOUNT_BUTTON;
        button_data->is_root = TRUE;

        g_object_unref (mount);
    }
    else if (nautilus_is_other_locations_directory (location))
    {
        button_data->type = OTHER_LOCATIONS_BUTTON;
        button_data->is_root = TRUE;
    }
    else
    {
        button_data->type = NORMAL_BUTTON;
    }

    g_free (uri);
}

static void
button_data_file_changed (NautilusFile *file,
                          ButtonData   *button_data)
{
    GFile *location, *current_location, *parent, *button_parent;
    ButtonData *current_button_data;
    char *display_name;
    NautilusPathBar *self;
    NautilusPathBarPrivate *priv;
    gboolean renamed, child;

    self = (NautilusPathBar *) gtk_widget_get_ancestor (button_data->button,
                                                        NAUTILUS_TYPE_PATH_BAR);
    priv = nautilus_path_bar_get_instance_private (self);

    if (self == NULL)
    {
        return;
    }

    g_assert (priv->current_path != NULL);
    g_assert (priv->current_button_data != NULL);

    current_button_data = priv->current_button_data;

    location = nautilus_file_get_location (file);
    if (!g_file_equal (button_data->path, location))
    {
        parent = g_file_get_parent (location);
        button_parent = g_file_get_parent (button_data->path);

        renamed = (parent != NULL && button_parent != NULL) &&
                  g_file_equal (parent, button_parent);

        if (parent != NULL)
        {
            g_object_unref (parent);
        }
        if (button_parent != NULL)
        {
            g_object_unref (button_parent);
        }

        if (renamed)
        {
            button_data->path = g_object_ref (location);
        }
        else
        {
            /* the file has been moved.
             * If it was below the currently displayed location, remove it.
             * If it was not below the currently displayed location, update the path bar
             */
            child = g_file_has_prefix (button_data->path,
                                       priv->current_path);

            if (child)
            {
                /* moved file inside current path hierarchy */
                g_object_unref (location);
                location = g_file_get_parent (button_data->path);
                current_location = g_object_ref (priv->current_path);
            }
            else
            {
                /* moved current path, or file outside current path hierarchy.
                 * Update path bar to new locations.
                 */
                current_location = nautilus_file_get_location (current_button_data->file);
            }

            nautilus_path_bar_update_path (self, location);
            nautilus_path_bar_set_path (self, current_location);
            g_object_unref (location);
            g_object_unref (current_location);
            return;
        }
    }
    else if (nautilus_file_is_gone (file))
    {
        gint idx, position;

        /* if the current or a parent location are gone, clear all the buttons,
         * the view will set the new path.
         */
        current_location = nautilus_file_get_location (current_button_data->file);

        if (g_file_has_prefix (current_location, location) ||
            g_file_equal (current_location, location))
        {
            nautilus_path_bar_clear_buttons (self);
        }
        else if (g_file_has_prefix (location, current_location))
        {
            /* remove this and the following buttons */
            position = g_list_position (priv->button_list,
                                        g_list_find (priv->button_list, button_data));

            if (position != -1)
            {
                for (idx = 0; idx <= position; idx++)
                {
                    ButtonData *data;

                    data = BUTTON_DATA (priv->button_list->data);

                    gtk_container_remove (GTK_CONTAINER (self), data->container);

                    button_data_free (data);
                }
            }
        }

        g_object_unref (current_location);
        g_object_unref (location);
        return;
    }
    g_object_unref (location);

    /* MOUNTs use the GMount as the name, so don't update for those */
    if (button_data->type != MOUNT_BUTTON)
    {
        display_name = nautilus_file_get_display_name (file);
        if (g_strcmp0 (display_name, button_data->dir_name) != 0)
        {
            g_free (button_data->dir_name);
            button_data->dir_name = g_strdup (display_name);
        }

        g_free (display_name);
    }
    nautilus_path_bar_update_button_appearance (button_data);
}

static ButtonData *
make_button_data (NautilusPathBar *self,
                  NautilusFile    *file,
                  gboolean         current_dir)
{
    GFile *path;
    GtkWidget *child;
    ButtonData *button_data;
    NautilusPathBarPrivate *priv;
    GtkGesture *gesture;

    priv = nautilus_path_bar_get_instance_private (self);
    path = nautilus_file_get_location (file);
    child = NULL;

    /* Is it a special button? */
    button_data = g_new0 (ButtonData, 1);

    setup_button_type (button_data, self, path);
    button_data->button = gtk_toggle_button_new ();
    gtk_style_context_add_class (gtk_widget_get_style_context (button_data->button),
                                 "text-button");
    gtk_widget_set_focus_on_click (button_data->button, FALSE);
    /* TODO update button type when xdg directories change */

    button_data->image = gtk_image_new ();

    switch (button_data->type)
    {
        case ROOT_BUTTON:
        {
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
            button_data->label = gtk_label_new (NULL);
            button_data->disclosure_arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_box_pack_start (GTK_BOX (button_data->container), button_data->button);

            gtk_box_pack_start (GTK_BOX (child), button_data->image);
            gtk_box_pack_start (GTK_BOX (child), button_data->label);
            gtk_box_pack_start (GTK_BOX (child), button_data->disclosure_arrow);
        }
        break;

        case HOME_BUTTON:
        case MOUNT_BUTTON:
        case RECENT_BUTTON:
        case STARRED_BUTTON:
        case OTHER_LOCATIONS_BUTTON:
        {
            button_data->label = gtk_label_new (NULL);
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
            button_data->disclosure_arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_box_pack_start (GTK_BOX (button_data->container), button_data->button);

            gtk_box_pack_start (GTK_BOX (child), button_data->image);
            gtk_box_pack_start (GTK_BOX (child), button_data->label);
            gtk_box_pack_start (GTK_BOX (child), button_data->disclosure_arrow);
        }
        break;

        case NORMAL_BUTTON:
        /* Fall through */
        default:
        {
            button_data->label = gtk_label_new (NULL);
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
            button_data->disclosure_arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_pack_start (GTK_BOX (button_data->container), gtk_label_new (G_DIR_SEPARATOR_S));
            gtk_box_pack_start (GTK_BOX (button_data->container), button_data->button);

            gtk_box_pack_start (GTK_BOX (child), button_data->label);
            gtk_box_pack_start (GTK_BOX (child), button_data->disclosure_arrow);
        }
        break;
    }

    gtk_widget_set_visible (button_data->disclosure_arrow, current_dir);
    if (current_dir)
    {
        gtk_popover_set_relative_to (priv->current_view_menu_popover, button_data->button);
    }

    if (button_data->label != NULL)
    {
        gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_single_line_mode (GTK_LABEL (button_data->label), TRUE);

        button_data->bold_label = gtk_label_new (NULL);
        gtk_widget_hide (button_data->bold_label);
        gtk_label_set_single_line_mode (GTK_LABEL (button_data->bold_label), TRUE);
        gtk_box_pack_start (GTK_BOX (child), button_data->bold_label);
    }

    if (button_data->path == NULL)
    {
        button_data->path = g_object_ref (path);
    }
    if (button_data->dir_name == NULL)
    {
        button_data->dir_name = nautilus_file_get_display_name (file);
    }
    if (button_data->file == NULL)
    {
        button_data->file = nautilus_file_ref (file);
        nautilus_file_monitor_add (button_data->file, button_data,
                                   NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
        button_data->file_changed_signal_id =
            g_signal_connect (button_data->file, "changed",
                              G_CALLBACK (button_data_file_changed),
                              button_data);
    }

    gtk_container_add (GTK_CONTAINER (button_data->button), child);

    nautilus_path_bar_update_button_state (button_data, current_dir);

    button_data->path_bar = self;

    g_signal_connect (button_data->button, "clicked", G_CALLBACK (button_clicked_cb), button_data);

    /* A gesture is needed here, because GtkButton doesn’t react to middle-clicking.
     */
    gesture = gtk_gesture_multi_press_new ();

    gtk_widget_add_controller (button_data->button, GTK_EVENT_CONTROLLER (gesture));

    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_MIDDLE);

    g_signal_connect (gesture, "pressed",
                      G_CALLBACK (on_multi_press_gesture_pressed), button_data);

    nautilus_drag_slot_proxy_init (button_data->button, button_data->file, NULL);

    g_object_unref (path);

    return button_data;
}

static void
nautilus_path_bar_update_path (NautilusPathBar *self,
                               GFile           *file_path)
{
    NautilusFile *file;
    NautilusPathBarPrivate *priv;
    gboolean first_directory;
    GList *new_buttons, *l;
    ButtonData *button_data;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));
    g_return_if_fail (file_path != NULL);

    priv = nautilus_path_bar_get_instance_private (self);
    first_directory = TRUE;
    new_buttons = NULL;

    file = nautilus_file_get (file_path);

    while (file != NULL)
    {
        NautilusFile *parent_file;

        parent_file = nautilus_file_get_parent (file);
        button_data = make_button_data (self, file, first_directory);
        nautilus_file_unref (file);

        if (first_directory)
        {
            first_directory = FALSE;
        }

        new_buttons = g_list_prepend (new_buttons, button_data);

        if (parent_file != NULL &&
            button_data->is_root)
        {
            nautilus_file_unref (parent_file);
            break;
        }

        file = parent_file;
    }

    nautilus_path_bar_clear_buttons (self);
    priv->button_list = g_list_reverse (new_buttons);

    for (l = priv->button_list; l; l = l->next)
    {
        GtkWidget *container;
        container = BUTTON_DATA (l->data)->container;
        gtk_container_add (GTK_CONTAINER (self), container);
    }
}

void
nautilus_path_bar_set_path (NautilusPathBar *self,
                            GFile           *file_path)
{
    ButtonData *button_data;
    NautilusPathBarPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_PATH_BAR (self));
    g_return_if_fail (file_path != NULL);

    priv = nautilus_path_bar_get_instance_private (self);

    /* Check whether the new path is already present in the pathbar as buttons.
     * This could be a parent directory or a previous selected subdirectory. */
    nautilus_path_bar_update_path (self, file_path);
    button_data = g_list_nth_data (priv->button_list, 0);

    if (priv->current_path != NULL)
    {
        g_object_unref (priv->current_path);
    }

    priv->current_path = g_object_ref (file_path);
    priv->current_button_data = button_data;
}
