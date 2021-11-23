#include "nautilus-gtk4-helpers.h"

void
gtk_button_set_child (GtkButton *button,
                      GtkWidget *child)
{
    g_assert (GTK_IS_BUTTON (button));

    gtk_container_add (GTK_CONTAINER (button), child);
}

void
gtk_box_append (GtkBox    *box,
                GtkWidget *child)
{
    g_assert (GTK_IS_BOX (box));

    gtk_container_add (GTK_CONTAINER (box), child);
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
