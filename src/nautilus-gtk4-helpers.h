#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS
#if GTK_MAJOR_VERSION < 4

void gtk_button_set_child          (GtkButton         *button,
                                    GtkWidget         *child);
void gtk_menu_button_set_child     (GtkMenuButton     *menu_button,
                                    GtkWidget         *child);
void gtk_box_append                (GtkBox            *box,
                                    GtkWidget         *child);
void gtk_box_remove                (GtkBox            *box,
                                    GtkWidget         *child);
void gtk_overlay_set_child         (GtkOverlay        *overlay,
                                    GtkWidget         *child);
void gtk_scrolled_window_set_child (GtkScrolledWindow *scrolled_window,
                                    GtkWidget         *child);
void gtk_list_box_row_set_child    (GtkListBoxRow     *row,
                                    GtkWidget         *child);
void gtk_info_bar_add_child        (GtkInfoBar        *info_bar,
                                    GtkWidget         *widget);
void gtk_revealer_set_child        (GtkRevealer       *revealer,
                                    GtkWidget         *child);
void gtk_popover_set_child         (GtkPopover        *popover,
                                    GtkWidget         *child);

GtkWidget *gtk_widget_get_first_child (GtkWidget *widget);
GtkWidget *gtk_widget_get_focus_child (GtkWidget *widget);
GtkWidget *gtk_scrolled_window_get_child (GtkScrolledWindow *scrolled);

void gtk_style_context_add_provider_for_display    (GdkDisplay       *display,
                                                    GtkStyleProvider *provider,
                                                    guint             priority);
void gtk_style_context_remove_provider_for_display (GdkDisplay       *display,
                                                    GtkStyleProvider *provider);

#define GTK_ROOT(root) ((GtkRoot *) GTK_WINDOW (root))
typedef GtkWindow GtkRoot;
GdkDisplay *gtk_root_get_display   (GtkRoot           *root);
void        gtk_window_set_display (GtkWindow         *window,
                                    GdkDisplay        *display);

#endif
G_END_DECLS
