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

struct _NautilusNotebook
{
    GtkNotebook parent_instance;
};

G_DEFINE_TYPE (NautilusNotebook, nautilus_notebook, GTK_TYPE_NOTEBOOK);

static void
nautilus_notebook_dispose (GObject *object)
{
    G_OBJECT_CLASS (nautilus_notebook_parent_class)->dispose (object);
}

static void
nautilus_notebook_class_init (NautilusNotebookClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_notebook_dispose;
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
on_page_removed (GtkNotebook *notebook,
                 GtkWidget   *child,
                 guint        page_num,
                 gpointer     user_data)
{
    gtk_notebook_set_show_tabs (notebook,
                                gtk_notebook_get_n_pages (notebook) > 1);
}

static void
on_page_added (GtkNotebook *notebook,
               GtkWidget   *child,
               guint        page_num,
               gpointer     user_data)
{
    gtk_notebook_set_show_tabs (notebook,
                                gtk_notebook_get_n_pages (notebook) > 1);
    gtk_notebook_set_tab_reorderable (notebook, child, TRUE);
    gtk_notebook_set_tab_detachable (notebook, child, TRUE);
}

static void
nautilus_notebook_init (NautilusNotebook *notebook)
{
    gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

    g_signal_connect (notebook, "page-removed", G_CALLBACK (on_page_removed), NULL);
    g_signal_connect (notebook, "page-added", G_CALLBACK (on_page_added), NULL);
}

gboolean
nautilus_notebook_contains_slot (NautilusNotebook   *notebook,
                                 NautilusWindowSlot *slot)
{
    GtkNotebook *container = GTK_NOTEBOOK (notebook);
    GtkWidget *child;
    gint n_pages;
    gboolean found = FALSE;

    g_return_val_if_fail (slot != NULL, FALSE);

    n_pages = gtk_notebook_get_n_pages (container);
    for (gint i = 0; i < n_pages; i++)
    {
        child = gtk_notebook_get_nth_page (container, i);
        if ((gpointer) child == (gpointer) slot)
        {
            found = TRUE;
            break;
        }
    }

    return found;
}

gboolean
nautilus_notebook_get_tab_clicked (NautilusNotebook *notebook,
                                   gint              x,
                                   gint              y,
                                   gint             *position)
{
    gint tab_num;

    tab_num = find_tab_num_at_pos (notebook, x, y);

    if (position != NULL)
    {
        *position = tab_num;
    }
    return tab_num != -1;
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

static GtkWidget *
build_tab_label (NautilusNotebook   *notebook,
                 NautilusWindowSlot *slot)
{
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *close_button;
    GtkWidget *spinner;
    GtkWidget *icon;

    /* When porting to Gtk+4, use GtkCenterBox instead */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_show (box);

    /* Spinner to be shown as load feedback */
    spinner = gtk_spinner_new ();
    gtk_box_pack_start (GTK_BOX (box), spinner, FALSE, FALSE, 0);

    /* Dummy icon to allocate space for spinner */
    icon = gtk_image_new ();
    gtk_box_pack_start (GTK_BOX (box), icon, FALSE, FALSE, 0);
    /* don't show the icon */

    /* Tab title */
    label = gtk_label_new (NULL);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
    gtk_label_set_width_chars (GTK_LABEL (label), 6);
    gtk_box_set_center_widget (GTK_BOX (box), label);
    gtk_widget_show (label);

    /* Tab close button */
    close_button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (close_button)),
                                 "flat");
    /* don't allow focus on the close button */
    gtk_widget_set_focus_on_click (close_button, FALSE);

    gtk_widget_set_name (close_button, "nautilus-tab-close-button");

    gtk_widget_set_tooltip_text (close_button, _("Close tab"));
    gtk_actionable_set_action_name (GTK_ACTIONABLE (close_button), "win.close-current-view");

    gtk_box_pack_end (GTK_BOX (box), close_button, FALSE, FALSE, 0);
    gtk_widget_show (close_button);

    g_object_set_data (G_OBJECT (box), "nautilus-notebook-tab", GINT_TO_POINTER (1));
#if 0 && NAUTILUS_DND_NEEDS_GTK4_REIMPLEMENTATION
    nautilus_drag_slot_proxy_init (box, NULL, slot);
#endif

    g_object_set_data (G_OBJECT (box), "label", label);
    g_object_set_data (G_OBJECT (box), "spinner", spinner);
    g_object_set_data (G_OBJECT (box), "icon", icon);
    g_object_set_data (G_OBJECT (box), "close-button", close_button);

    return box;
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
