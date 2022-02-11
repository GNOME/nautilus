#include "nautilus-gtk4-helpers.h"

void
adw_bin_set_child (AdwBin    *bin,
                   GtkWidget *child)
{
    g_assert (GTK_IS_BIN (bin));

    gtk_container_add (GTK_CONTAINER (bin), child);
}

void
gtk_button_set_child (GtkButton *button,
                      GtkWidget *child)
{
    g_assert (GTK_IS_BUTTON (button));

    gtk_container_add (GTK_CONTAINER (button), child);
}

void
gtk_menu_button_set_child (GtkMenuButton *menu_button,
                           GtkWidget     *child)
{
    g_assert (GTK_IS_MENU_BUTTON (menu_button));

    gtk_container_add (GTK_CONTAINER (menu_button), child);
}

void
gtk_box_append (GtkBox    *box,
                GtkWidget *child)
{
    g_assert (GTK_IS_BOX (box));

    gtk_container_add (GTK_CONTAINER (box), child);
}

void
gtk_box_remove (GtkBox    *box,
                GtkWidget *child)
{
    g_assert (GTK_IS_BOX (box));

    gtk_container_remove (GTK_CONTAINER (box), child);
}

void
gtk_overlay_set_child (GtkOverlay *overlay,
                       GtkWidget  *child)
{
    g_assert (GTK_IS_OVERLAY (overlay));

    gtk_container_add (GTK_CONTAINER (overlay), child);
}

void
gtk_scrolled_window_set_child (GtkScrolledWindow *scrolled_window,
                               GtkWidget         *child)
{
    g_assert (GTK_IS_SCROLLED_WINDOW (scrolled_window));

    gtk_container_add (GTK_CONTAINER (scrolled_window), child);
}

void
gtk_list_box_row_set_child (GtkListBoxRow *row,
                            GtkWidget     *child)
{
    g_assert (GTK_IS_LIST_BOX_ROW (row));

    gtk_container_add (GTK_CONTAINER (row), child);
}

void
gtk_info_bar_add_child (GtkInfoBar *info_bar,
                        GtkWidget  *widget)
{
    g_assert (GTK_IS_INFO_BAR (info_bar));

    gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (info_bar)),
                       widget);
}

void
gtk_revealer_set_child (GtkRevealer *revealer,
                        GtkWidget   *child)
{
    g_assert (GTK_IS_REVEALER (revealer));

    gtk_container_add (GTK_CONTAINER (revealer), child);
}

void
gtk_popover_set_child (GtkPopover *popover,
                       GtkWidget  *child)
{
    g_assert (GTK_IS_POPOVER (popover));

    gtk_container_add (GTK_CONTAINER (popover), child);
}

void
gtk_check_button_set_active (GtkCheckButton *button,
                             gboolean        setting)
{
    g_assert (GTK_IS_CHECK_BUTTON (button));

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), setting);
}


void
gtk_center_box_set_start_widget (GtkCenterBox *center_box,
                                 GtkWidget    *widget)
{
    g_assert (GTK_IS_BOX (center_box));

    gtk_box_pack_start (GTK_BOX (center_box), widget, FALSE, TRUE, 0);
}

void
gtk_center_box_set_center_widget (GtkCenterBox *center_box,
                                  GtkWidget    *widget)
{
    g_assert (GTK_IS_BOX (center_box));

    gtk_box_set_center_widget (GTK_BOX (center_box), widget);
}
void
gtk_center_box_set_end_widget (GtkCenterBox *center_box,
                               GtkWidget    *widget)
{
    g_assert (GTK_IS_BOX (center_box));

    gtk_box_pack_end (GTK_BOX (center_box), widget, FALSE, TRUE, 0);
}

GtkWidget *
gtk_center_box_new (void)
{
    return gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
}

gboolean
gtk_check_button_get_active (GtkCheckButton *button)
{
    g_assert (GTK_IS_CHECK_BUTTON (button));

    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
}

GtkWidget *
gtk_widget_get_first_child (GtkWidget *widget)
{
    g_autoptr (GList) children = NULL;

    g_assert (GTK_IS_CONTAINER (widget));

    children = gtk_container_get_children (GTK_CONTAINER (widget));
    if (children != NULL)
    {
        return GTK_WIDGET (children->data);
    }

    return NULL;
}

GtkWidget *
gtk_widget_get_focus_child (GtkWidget *widget)
{
    g_assert (GTK_IS_CONTAINER (widget));

    return gtk_container_get_focus_child (GTK_CONTAINER (widget));
}

GtkWidget *
gtk_scrolled_window_get_child (GtkScrolledWindow *scrolled)
{
    g_assert (GTK_IS_SCROLLED_WINDOW (scrolled));

    return gtk_bin_get_child (GTK_BIN (scrolled));
}

GdkDisplay *
gtk_root_get_display (GtkRoot *root)
{
    g_assert (GTK_IS_WINDOW (root));

    return gdk_screen_get_display (gtk_window_get_screen (GTK_WINDOW (root)));
}

void
gtk_window_set_display (GtkWindow  *window,
                        GdkDisplay *display)
{
    g_assert (GTK_IS_WINDOW (window));

    gtk_window_set_screen (window, gdk_display_get_default_screen (display));
}

void
gtk_style_context_add_provider_for_display (GdkDisplay       *display,
                                            GtkStyleProvider *provider,
                                            guint             priority)
{
    gtk_style_context_add_provider_for_screen (gdk_display_get_default_screen (display),
                                               provider,
                                               priority);
}

void
gtk_style_context_remove_provider_for_display (GdkDisplay       *display,
                                               GtkStyleProvider *provider)
{
    gtk_style_context_remove_provider_for_screen (gdk_display_get_default_screen (display),
                                                  provider);
}
