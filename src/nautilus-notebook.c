/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *    (ephy-notebook.c)
 *
 *  Copyright © 2008 Free Software Foundation, Inc.
 *    (nautilus-notebook.c)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "nautilus-notebook.h"

#include "nautilus-window.h"
#include "nautilus-window-slot.h"
#include "nautilus-window-slot-dnd.h"

#include <eel/eel-vfs-extensions.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define AFTER_ALL_TABS -1

static int  nautilus_notebook_insert_page (GtkNotebook *notebook,
                                           GtkWidget   *child,
                                           GtkWidget   *tab_label,
                                           GtkWidget   *menu_label,
                                           int          position);
static void nautilus_notebook_remove (GtkContainer *container,
                                      GtkWidget    *tab_widget);

enum
{
    TAB_CLOSE_REQUEST,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _NautilusNotebook
{
    GtkNotebook parent_instance;
};

G_DEFINE_TYPE (NautilusNotebook, nautilus_notebook, GTK_TYPE_NOTEBOOK);

static void
nautilus_notebook_class_init (NautilusNotebookClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
    GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

    container_class->remove = nautilus_notebook_remove;

    notebook_class->insert_page = nautilus_notebook_insert_page;

    signals[TAB_CLOSE_REQUEST] =
        g_signal_new ("tab-close-request",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      NAUTILUS_TYPE_WINDOW_SLOT);
}

static gint
find_tab_num_at_pos (NautilusNotebook *notebook,
                     gint              abs_x,
                     gint              abs_y)
{
    int page_num = 0;
    GtkNotebook *nb = GTK_NOTEBOOK (notebook);
    GtkWidget *page;
    GtkAllocation allocation;

    while ((page = gtk_notebook_get_nth_page (nb, page_num)))
    {
        GtkWidget *tab;
        gint max_x, max_y;

        tab = gtk_notebook_get_tab_label (nb, page);
        g_return_val_if_fail (tab != NULL, -1);

        if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
        {
            page_num++;
            continue;
        }

        gtk_widget_get_allocation (tab, &allocation);

        max_x = allocation.x + allocation.width;
        max_y = allocation.y + allocation.height;

        if (abs_x <= max_x && abs_y <= max_y)
        {
            return page_num;
        }

        page_num++;
    }
    return AFTER_ALL_TABS;
}

static void
button_press_cb (GtkGestureMultiPress *gesture,
                 gint                  n_press,
                 gdouble               x,
                 gdouble               y,
                 gpointer              user_data)
{
    guint button;
    GdkEventSequence *sequence;
    const GdkEvent *event;
    GtkWidget *widget;
    NautilusNotebook *notebook;
    int tab_clicked;
    GdkModifierType state;

    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
    event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    notebook = NAUTILUS_NOTEBOOK (widget);
    tab_clicked = find_tab_num_at_pos (notebook, x, y);

    gdk_event_get_state (event, &state);

    if (n_press != 1)
    {
        return;
    }

    if (tab_clicked == -1)
    {
        return;
    }

    if (button == GDK_BUTTON_SECONDARY &&
        (state & gtk_accelerator_get_default_mod_mask ()) == 0)
    {
        /* switch to the page the mouse is over, but don't consume the event */
        gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_clicked);
    }
    else if (button == GDK_BUTTON_MIDDLE)
    {
        GtkWidget *slot;

        slot = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), tab_clicked);
        g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, slot);
    }
}

static void
nautilus_notebook_init (NautilusNotebook *notebook)
{
    GtkGesture *gesture;

    gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

    gesture = gtk_gesture_multi_press_new ();

    gtk_widget_add_controller (GTK_WIDGET (notebook), GTK_EVENT_CONTROLLER (gesture));

    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);

    g_signal_connect (gesture, "pressed", G_CALLBACK (button_press_cb), NULL);
}

gboolean
nautilus_notebook_contains_slot (NautilusNotebook   *notebook,
                                 NautilusWindowSlot *slot)
{
    GList *children;
    GList *l;
    gboolean found = FALSE;

    children = gtk_container_get_children (GTK_CONTAINER (notebook));
    for (l = children; l != NULL && !found; l = l->next)
    {
        found = l->data == slot;
    }

    g_list_free (children);

    return found;
}

gboolean
nautilus_notebook_content_area_hit (NautilusNotebook *notebook,
                                    gint              x,
                                    gint              y)
{
    return find_tab_num_at_pos (notebook, x, y) == -1;
}

void
nautilus_notebook_sync_loading (NautilusNotebook   *notebook,
                                NautilusWindowSlot *slot)
{
    GtkWidget *tab_label, *spinner, *icon;
    gboolean active, allow_stop;

    g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));
    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

    tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook),
                                            GTK_WIDGET (slot));
    g_return_if_fail (GTK_IS_WIDGET (tab_label));

    spinner = GTK_WIDGET (g_object_get_data (G_OBJECT (tab_label), "spinner"));
    icon = GTK_WIDGET (g_object_get_data (G_OBJECT (tab_label), "icon"));
    g_return_if_fail (spinner != NULL && icon != NULL);

    active = FALSE;
    g_object_get (spinner, "active", &active, NULL);
    allow_stop = nautilus_window_slot_get_allow_stop (slot);

    if (active == allow_stop)
    {
        return;
    }

    if (allow_stop)
    {
        gtk_widget_hide (icon);
        gtk_widget_show (spinner);
        gtk_spinner_start (GTK_SPINNER (spinner));
    }
    else
    {
        gtk_spinner_stop (GTK_SPINNER (spinner));
        gtk_widget_hide (spinner);
        gtk_widget_show (icon);
    }
}

void
nautilus_notebook_sync_tab_label (NautilusNotebook   *notebook,
                                  NautilusWindowSlot *slot)
{
    GtkWidget *hbox, *label;
    char *location_name;
    GFile *location;
    const gchar *title_name;

    g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));
    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

    hbox = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), GTK_WIDGET (slot));
    g_return_if_fail (GTK_IS_WIDGET (hbox));

    label = GTK_WIDGET (g_object_get_data (G_OBJECT (hbox), "label"));
    g_return_if_fail (GTK_IS_WIDGET (label));

    gtk_label_set_text (GTK_LABEL (label), nautilus_window_slot_get_title (slot));
    location = nautilus_window_slot_get_location (slot);

    if (location != NULL)
    {
        /* Set the tooltip on the label's parent (the tab label hbox),
         * so it covers all of the tab label.
         */
        location_name = g_file_get_parse_name (location);
        title_name = nautilus_window_slot_get_title (slot);
        if (eel_uri_is_search (location_name))
        {
            gtk_widget_set_tooltip_text (gtk_widget_get_parent (label), title_name);
        }
        else
        {
            gtk_widget_set_tooltip_text (gtk_widget_get_parent (label), location_name);
        }
        g_free (location_name);
    }
    else
    {
        gtk_widget_set_tooltip_text (gtk_widget_get_parent (label), NULL);
    }
}

static void
close_button_clicked_cb (GtkWidget          *widget,
                         NautilusWindowSlot *slot)
{
    GtkWidget *notebook;

    notebook = gtk_widget_get_ancestor (GTK_WIDGET (slot), NAUTILUS_TYPE_NOTEBOOK);
    if (notebook != NULL)
    {
        g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, slot);
    }
}

static GtkWidget *
build_tab_label (NautilusNotebook   *notebook,
                 NautilusWindowSlot *slot)
{
    GtkWidget *center_box;
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *close_button;
    GtkWidget *image;
    GtkWidget *spinner;
    GtkWidget *icon;

    center_box = gtk_center_box_new ();

    grid = gtk_grid_new ();
    gtk_center_box_set_start_widget (GTK_CENTER_BOX (center_box), grid);

    /* Spinner to be shown as load feedback */
    spinner = gtk_spinner_new ();
    g_object_set (spinner, "expand", TRUE, NULL);
    gtk_container_add (GTK_CONTAINER (grid), spinner);

    /* Dummy icon to allocate space for spinner */
    icon = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (grid), icon);
    /* don't show the icon */
    gtk_widget_hide (icon);

    /* Tab title */
    label = gtk_label_new (NULL);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
    gtk_label_set_width_chars (GTK_LABEL (label), 6);
    gtk_center_box_set_center_widget (GTK_CENTER_BOX (center_box), label);
    gtk_widget_show (label);

    /* Tab close button */
    close_button = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (close_button),
                           GTK_RELIEF_NONE);
    /* don't allow focus on the close button */
    gtk_widget_set_focus_on_click (close_button, FALSE);

    gtk_widget_set_name (close_button, "nautilus-tab-close-button");

    image = gtk_image_new_from_icon_name ("window-close-symbolic");
    gtk_widget_set_tooltip_text (close_button, _("Close tab"));
    g_signal_connect_object (close_button, "clicked",
                             G_CALLBACK (close_button_clicked_cb), slot, 0);

    gtk_container_add (GTK_CONTAINER (close_button), image);
    gtk_widget_show (image);

    gtk_center_box_set_end_widget (GTK_CENTER_BOX (center_box), close_button);
    gtk_widget_show (close_button);

    g_object_set_data (G_OBJECT (center_box), "nautilus-notebook-tab", GINT_TO_POINTER (1));
    nautilus_drag_slot_proxy_init (center_box, NULL, slot);

    g_object_set_data (G_OBJECT (center_box), "label", label);
    g_object_set_data (G_OBJECT (center_box), "spinner", spinner);
    g_object_set_data (G_OBJECT (center_box), "icon", icon);
    g_object_set_data (G_OBJECT (center_box), "close-button", close_button);

    return center_box;
}

static int
nautilus_notebook_insert_page (GtkNotebook *gnotebook,
                               GtkWidget   *tab_widget,
                               GtkWidget   *tab_label,
                               GtkWidget   *menu_label,
                               int          position)
{
    g_assert (GTK_IS_WIDGET (tab_widget));

    position = GTK_NOTEBOOK_CLASS (nautilus_notebook_parent_class)->insert_page (gnotebook,
                                                                                 tab_widget,
                                                                                 tab_label,
                                                                                 menu_label,
                                                                                 position);

    gtk_notebook_set_show_tabs (gnotebook,
                                gtk_notebook_get_n_pages (gnotebook) > 1);
    gtk_notebook_set_tab_reorderable (gnotebook, tab_widget, TRUE);
    gtk_notebook_set_tab_detachable (gnotebook, tab_widget, TRUE);

    return position;
}

int
nautilus_notebook_add_tab (NautilusNotebook   *notebook,
                           NautilusWindowSlot *slot,
                           int                 position,
                           gboolean            jump_to)
{
    GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);
    GtkWidget *tab_label;

    g_return_val_if_fail (NAUTILUS_IS_NOTEBOOK (notebook), -1);
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot), -1);

    tab_label = build_tab_label (notebook, slot);

    position = gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
                                         GTK_WIDGET (slot),
                                         tab_label,
                                         position);

    gtk_container_child_set (GTK_CONTAINER (notebook),
                             GTK_WIDGET (slot),
                             "tab-expand", TRUE,
                             "detachable", FALSE,
                             NULL);

    nautilus_notebook_sync_tab_label (notebook, slot);
    nautilus_notebook_sync_loading (notebook, slot);

    if (jump_to)
    {
        gtk_notebook_set_current_page (gnotebook, position);
    }

    return position;
}

static void
nautilus_notebook_remove (GtkContainer *container,
                          GtkWidget    *tab_widget)
{
    GtkNotebook *gnotebook = GTK_NOTEBOOK (container);
    GTK_CONTAINER_CLASS (nautilus_notebook_parent_class)->remove (container, tab_widget);

    gtk_notebook_set_show_tabs (gnotebook,
                                gtk_notebook_get_n_pages (gnotebook) > 1);
}

void
nautilus_notebook_reorder_current_child_relative (NautilusNotebook *notebook,
                                                  int               offset)
{
    GtkNotebook *gnotebook;
    GtkWidget *child;
    int page;

    g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));

    if (!nautilus_notebook_can_reorder_current_child_relative (notebook, offset))
    {
        return;
    }

    gnotebook = GTK_NOTEBOOK (notebook);

    page = gtk_notebook_get_current_page (gnotebook);
    child = gtk_notebook_get_nth_page (gnotebook, page);
    gtk_notebook_reorder_child (gnotebook, child, page + offset);
}

static gboolean
nautilus_notebook_is_valid_relative_position (NautilusNotebook *notebook,
                                              int               offset)
{
    GtkNotebook *gnotebook;
    int page;
    int n_pages;

    gnotebook = GTK_NOTEBOOK (notebook);

    page = gtk_notebook_get_current_page (gnotebook);
    n_pages = gtk_notebook_get_n_pages (gnotebook) - 1;
    if (page < 0 ||
        (offset < 0 && page < -offset) ||
        (offset > 0 && page > n_pages - offset))
    {
        return FALSE;
    }

    return TRUE;
}

gboolean
nautilus_notebook_can_reorder_current_child_relative (NautilusNotebook *notebook,
                                                      int               offset)
{
    g_return_val_if_fail (NAUTILUS_IS_NOTEBOOK (notebook), FALSE);

    return nautilus_notebook_is_valid_relative_position (notebook, offset);
}

void
nautilus_notebook_next_page (NautilusNotebook *notebook)
{
    gint current_page, n_pages;

    g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));

    current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
    n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

    if (current_page < n_pages - 1)
    {
        gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
    }
    else
    {
        gboolean wrap_around;

        g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
                      "gtk-keynav-wrap-around", &wrap_around,
                      NULL);

        if (wrap_around)
        {
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
        }
    }
}

void
nautilus_notebook_prev_page (NautilusNotebook *notebook)
{
    gint current_page;

    g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));

    current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

    if (current_page > 0)
    {
        gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
    }
    else
    {
        gboolean wrap_around;

        g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
                      "gtk-keynav-wrap-around", &wrap_around,
                      NULL);

        if (wrap_around)
        {
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), -1);
        }
    }
}
