/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-progress-indicator.h"

#include "nautilus-file-operations.h"
#include "nautilus-progress-info-manager.h"
#include "nautilus-progress-info-widget.h"
#include "nautilus-window.h"

#define OPERATION_MINIMUM_TIME 2 /*s */
#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 3 /*s */

struct _NautilusProgressIndicator
{
    AdwBin parent_instance;

    guint start_operations_timeout_id;
    guint remove_finished_operations_timeout_id;
    guint operations_button_attention_timeout_id;

    GtkWidget *operations_button;
    GtkWidget *operations_popover;
    GtkWidget *operations_list;
    GListStore *progress_infos_model;
    GtkWidget *operations_revealer;
    GtkWidget *operations_icon;

    NautilusProgressInfoManager *progress_manager;
};

G_DEFINE_FINAL_TYPE (NautilusProgressIndicator, nautilus_progress_indicator, ADW_TYPE_BIN);


static void update_operations (NautilusProgressIndicator *self);

static gboolean
should_show_progress_info (NautilusProgressInfo *info)
{
    return nautilus_progress_info_get_total_elapsed_time (info) +
           nautilus_progress_info_get_remaining_time (info) > OPERATION_MINIMUM_TIME;
}

static GList *
get_filtered_progress_infos (NautilusProgressIndicator *self)
{
    GList *l;
    GList *filtered_progress_infos;
    GList *progress_infos;

    progress_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    filtered_progress_infos = NULL;

    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (should_show_progress_info (l->data))
        {
            filtered_progress_infos = g_list_append (filtered_progress_infos, l->data);
        }
    }

    return filtered_progress_infos;
}

static gboolean
should_hide_operations_button (NautilusProgressIndicator *self)
{
    GList *progress_infos;
    GList *l;

    progress_infos = get_filtered_progress_infos (self);

    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (nautilus_progress_info_get_total_elapsed_time (l->data) +
            nautilus_progress_info_get_remaining_time (l->data) > OPERATION_MINIMUM_TIME &&
            !nautilus_progress_info_get_is_cancelled (l->data) &&
            !nautilus_progress_info_get_is_finished (l->data))
        {
            return FALSE;
        }
    }

    g_list_free (progress_infos);

    return TRUE;
}

static gboolean
on_remove_finished_operations_timeout (NautilusProgressIndicator *self)
{
    nautilus_progress_info_manager_remove_finished_or_cancelled_infos (self->progress_manager);
    if (should_hide_operations_button (self))
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->operations_revealer),
                                       FALSE);
    }
    else
    {
        update_operations (self);
    }

    self->remove_finished_operations_timeout_id = 0;

    return G_SOURCE_REMOVE;
}

static void
unschedule_remove_finished_operations (NautilusProgressIndicator *self)
{
    if (self->remove_finished_operations_timeout_id != 0)
    {
        g_source_remove (self->remove_finished_operations_timeout_id);
        self->remove_finished_operations_timeout_id = 0;
    }
}

static void
schedule_remove_finished_operations (NautilusProgressIndicator *self)
{
    if (self->remove_finished_operations_timeout_id == 0)
    {
        self->remove_finished_operations_timeout_id =
            g_timeout_add_seconds (REMOVE_FINISHED_OPERATIONS_TIEMOUT,
                                   (GSourceFunc) on_remove_finished_operations_timeout,
                                   self);
    }
}

static void
remove_operations_button_attention_style (NautilusProgressIndicator *self)
{
    gtk_widget_remove_css_class (self->operations_button,
                                 "nautilus-operations-button-needs-attention");
}

static gboolean
on_remove_operations_button_attention_style_timeout (NautilusProgressIndicator *self)
{
    remove_operations_button_attention_style (self);
    self->operations_button_attention_timeout_id = 0;

    return G_SOURCE_REMOVE;
}

static void
unschedule_operations_button_attention_style (NautilusProgressIndicator *self)
{
    if (self->operations_button_attention_timeout_id != 0)
    {
        g_source_remove (self->operations_button_attention_timeout_id);
        self->operations_button_attention_timeout_id = 0;
    }
}

static void
add_operations_button_attention_style (NautilusProgressIndicator *self)
{
    unschedule_operations_button_attention_style (self);
    remove_operations_button_attention_style (self);

    gtk_widget_add_css_class (self->operations_button,
                              "nautilus-operations-button-needs-attention");
    self->operations_button_attention_timeout_id = g_timeout_add (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                                                                  (GSourceFunc) on_remove_operations_button_attention_style_timeout,
                                                                  self);
}

static void
on_progress_info_cancelled (NautilusProgressIndicator *self)
{
    /* Update the pie chart progress */
    gtk_widget_queue_draw (self->operations_icon);

    if (!nautilus_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }
}

static void
on_progress_info_progress_changed (NautilusProgressIndicator *self)
{
    /* Update the pie chart progress */
    gtk_widget_queue_draw (self->operations_icon);
}

static void
on_progress_info_finished (NautilusProgressIndicator *self,
                           NautilusProgressInfo      *info)
{
    NautilusWindow *window;
    gchar *main_label;
    GFile *folder_to_open;

    window = NAUTILUS_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

    /* Update the pie chart progress */
    gtk_widget_queue_draw (self->operations_icon);

    if (!nautilus_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }

    folder_to_open = nautilus_progress_info_get_destination (info);
    /* If destination is null, don't show a notification. This happens when the
     * operation is a trash operation, which we already show a diferent kind of
     * notification */
    if (!gtk_widget_is_visible (self->operations_popover) &&
        folder_to_open != NULL)
    {
        add_operations_button_attention_style (self);
        main_label = nautilus_progress_info_get_status (info);
        nautilus_window_show_operation_notification (window,
                                                     main_label,
                                                     folder_to_open);
        g_free (main_label);
    }

    g_clear_object (&folder_to_open);
}

static void
disconnect_progress_infos (NautilusProgressIndicator *self)
{
    GList *progress_infos;
    GList *l;

    progress_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    for (l = progress_infos; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (l->data, self);
    }
}

static void
update_operations (NautilusProgressIndicator *self)
{
    GList *progress_infos;
    GList *l;
    gboolean should_show_progress_button = FALSE;

    disconnect_progress_infos (self);
    g_list_store_remove_all (self->progress_infos_model);

    progress_infos = get_filtered_progress_infos (self);
    for (l = progress_infos; l != NULL; l = l->next)
    {
        should_show_progress_button = should_show_progress_button ||
                                      should_show_progress_info (l->data);

        g_signal_connect_swapped (l->data, "finished",
                                  G_CALLBACK (on_progress_info_finished), self);
        g_signal_connect_swapped (l->data, "cancelled",
                                  G_CALLBACK (on_progress_info_cancelled), self);
        g_signal_connect_swapped (l->data, "progress-changed",
                                  G_CALLBACK (on_progress_info_progress_changed), self);
        g_list_store_append (self->progress_infos_model, l->data);
    }

    g_list_free (progress_infos);

    if (should_show_progress_button &&
        !gtk_revealer_get_reveal_child (GTK_REVEALER (self->operations_revealer)))
    {
        add_operations_button_attention_style (self);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->operations_revealer),
                                       TRUE);
        gtk_widget_queue_draw (self->operations_icon);
    }

    /* Since we removed the info widgets, we need to restore the focus */
    if (gtk_widget_get_visible (self->operations_popover))
    {
        gtk_widget_grab_focus (self->operations_popover);
    }
}

static gboolean
on_progress_info_started_timeout (NautilusProgressIndicator *self)
{
    GList *progress_infos;
    GList *filtered_progress_infos;

    update_operations (self);

    /* In case we didn't show the operations button because the operation total
     * time stimation is not good enough, update again to make sure we don't miss
     * a long time operation because of that */

    progress_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    filtered_progress_infos = get_filtered_progress_infos (self);
    if (!nautilus_progress_manager_are_all_infos_finished_or_cancelled (self->progress_manager) &&
        g_list_length (progress_infos) != g_list_length (filtered_progress_infos))
    {
        g_list_free (filtered_progress_infos);
        return G_SOURCE_CONTINUE;
    }
    else
    {
        g_list_free (filtered_progress_infos);
        self->start_operations_timeout_id = 0;
        return G_SOURCE_REMOVE;
    }
}

static void
schedule_operations_start (NautilusProgressIndicator *self)
{
    if (self->start_operations_timeout_id == 0)
    {
        /* Timeout is a little more than what we require for a stimated operation
         * total time, to make sure the stimated total time is correct */
        self->start_operations_timeout_id =
            g_timeout_add (SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE * 1000 + 500,
                           (GSourceFunc) on_progress_info_started_timeout,
                           self);
    }
}

static void
unschedule_operations_start (NautilusProgressIndicator *self)
{
    if (self->start_operations_timeout_id != 0)
    {
        g_source_remove (self->start_operations_timeout_id);
        self->start_operations_timeout_id = 0;
    }
}

static void
on_progress_info_started (NautilusProgressInfo      *info,
                          NautilusProgressIndicator *self)
{
    g_signal_handlers_disconnect_by_data (info, self);
    schedule_operations_start (self);
}

static void
on_new_progress_info (NautilusProgressInfoManager *manager,
                      NautilusProgressInfo        *info,
                      NautilusProgressIndicator   *self)
{
    g_signal_connect (info, "started",
                      G_CALLBACK (on_progress_info_started), self);
}

static void
on_operations_icon_draw (GtkDrawingArea            *drawing_area,
                         cairo_t                   *cr,
                         int                        width,
                         int                        height,
                         NautilusProgressIndicator *self)
{
    GtkWidget *widget = GTK_WIDGET (drawing_area);
    gfloat elapsed_progress = 0;
    gint remaining_progress = 0;
    gint total_progress;
    gdouble ratio;
    GList *progress_infos;
    GList *l;
    gboolean all_cancelled;
    GdkRGBA background;
    GdkRGBA foreground;
    GtkStyleContext *style_context;

    style_context = gtk_widget_get_style_context (widget);
    gtk_style_context_get_color (style_context, &foreground);
    background = foreground;
    background.alpha *= 0.3;

    all_cancelled = TRUE;
    progress_infos = get_filtered_progress_infos (self);
    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (!nautilus_progress_info_get_is_cancelled (l->data))
        {
            all_cancelled = FALSE;
            remaining_progress += nautilus_progress_info_get_remaining_time (l->data);
            elapsed_progress += nautilus_progress_info_get_elapsed_time (l->data);
        }
    }

    g_list_free (progress_infos);

    total_progress = remaining_progress + elapsed_progress;

    if (all_cancelled)
    {
        ratio = 1.0;
    }
    else
    {
        if (total_progress > 0)
        {
            ratio = MAX (0.05, elapsed_progress / total_progress);
        }
        else
        {
            ratio = 0.05;
        }
    }


    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

    gdk_cairo_set_source_rgba (cr, &background);
    cairo_arc (cr,
               width / 2.0, height / 2.0,
               MIN (width, height) / 2.0,
               0, 2 * G_PI);
    cairo_fill (cr);
    cairo_move_to (cr, width / 2.0, height / 2.0);
    gdk_cairo_set_source_rgba (cr, &foreground);
    cairo_arc (cr,
               width / 2.0, height / 2.0,
               MIN (width, height) / 2.0,
               -G_PI / 2.0, ratio * 2 * G_PI - G_PI / 2.0);

    cairo_fill (cr);
}

static void
on_operations_popover_notify_visible (NautilusProgressIndicator *self,
                                      GParamSpec                *pspec,
                                      GObject                   *popover)
{
    if (gtk_widget_get_visible (GTK_WIDGET (popover)))
    {
        unschedule_remove_finished_operations (self);
        nautilus_progress_manager_add_viewer (self->progress_manager,
                                              G_OBJECT (self));
    }
    else
    {
        nautilus_progress_manager_remove_viewer (self->progress_manager,
                                                 G_OBJECT (self));
    }
}

static void
on_progress_has_viewers_changed (NautilusProgressInfoManager *manager,
                                 NautilusProgressIndicator   *self)
{
    if (nautilus_progress_manager_has_viewers (manager))
    {
        unschedule_remove_finished_operations (self);
        return;
    }

    if (nautilus_progress_manager_are_all_infos_finished_or_cancelled (manager))
    {
        unschedule_remove_finished_operations (self);
        schedule_remove_finished_operations (self);
    }
}

static GtkWidget *
operations_list_create_widget (GObject  *item,
                               gpointer  user_data)
{
    NautilusProgressInfo *info = NAUTILUS_PROGRESS_INFO (item);
    GtkWidget *widget;

    widget = nautilus_progress_info_widget_new (info);
    gtk_widget_show (widget);

    return widget;
}

static void
nautilus_progress_indicator_constructed (GObject *object)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (object);

    self->progress_manager = nautilus_progress_info_manager_dup_singleton ();
    g_signal_connect (self->progress_manager, "new-progress-info",
                      G_CALLBACK (on_new_progress_info), self);
    g_signal_connect (self->progress_manager, "has-viewers-changed",
                      G_CALLBACK (on_progress_has_viewers_changed), self);

    self->progress_infos_model = g_list_store_new (NAUTILUS_TYPE_PROGRESS_INFO);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->operations_list),
                             G_LIST_MODEL (self->progress_infos_model),
                             (GtkListBoxCreateWidgetFunc) operations_list_create_widget,
                             NULL,
                             NULL);
    update_operations (self);

    g_signal_connect (self->operations_popover, "show",
                      (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (self->operations_popover, "closed",
                              (GCallback) gtk_widget_grab_focus, self);
}

static void
nautilus_progress_indicator_finalize (GObject *obj)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (obj);

    disconnect_progress_infos (self);
    unschedule_remove_finished_operations (self);
    unschedule_operations_start (self);
    unschedule_operations_button_attention_style (self);

    g_clear_object (&self->progress_infos_model);
    g_signal_handlers_disconnect_by_data (self->progress_manager, self);
    g_clear_object (&self->progress_manager);

    G_OBJECT_CLASS (nautilus_progress_indicator_parent_class)->finalize (obj);
}

static void
nautilus_progress_indicator_class_init (NautilusProgressIndicatorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = nautilus_progress_indicator_constructed;
    object_class->finalize = nautilus_progress_indicator_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-progress-indicator.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_popover);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_list);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_revealer);

    gtk_widget_class_bind_template_callback (widget_class, on_operations_popover_notify_visible);
}

static void
nautilus_progress_indicator_init (NautilusProgressIndicator *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->operations_icon),
                                    (GtkDrawingAreaDrawFunc) on_operations_icon_draw,
                                    self,
                                    NULL);
}
