#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS
#if GTK_MAJOR_VERSION < 4

#define AdwBin GtkBin
#define ADW_BIN GTK_BIN
#define GtkCenterBox GtkBox
#define GTK_CENTER_BOX GTK_BOX

void adw_bin_set_child             (AdwBin            *bin,
                                    GtkWidget         *child);
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
void gtk_check_button_set_active   (GtkCheckButton    *button,
                                    gboolean           setting);
void gtk_center_box_set_start_widget (GtkCenterBox    *center_box,
                                      GtkWidget       *widget);
void gtk_center_box_set_center_widget (GtkCenterBox   *center_box,
                                       GtkWidget      *widget);
void gtk_center_box_set_end_widget (GtkCenterBox      *center_box,
                                    GtkWidget         *widget);

GtkWidget *gtk_center_box_new (void);

gboolean gtk_check_button_get_active (GtkCheckButton  *button);
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
