/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-progress-indicator.h"

#include <glib/gi18n.h>

#include "nautilus-file-operations.h"
#include "nautilus-progress-info-manager.h"
#include "nautilus-progress-paintable.h"
#include "nautilus-progress-info-widget.h"
#include "nautilus-window.h"

#define REMOVE_FINISHED_OPERATIONS_TIMEOUT 3 /*s */
#define MAX_ITEMS_IN_SIDEBAR 3

enum
{
    PROP_0,
    PROP_REVEAL,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

struct _NautilusProgressIndicator
{
    AdwBin parent_instance;

    guint start_operations_timeout_id;
    guint remove_finished_operations_timeout_id;
    guint operations_button_attention_timeout_id;

    GtkWidget *operations_popover;
    GtkWidget *operations_list;
    GListStore *progress_infos_model;
    GtkWidget *sidebar_list;

    gboolean reveal;

    NautilusProgressInfoManager *progress_manager;
};

G_DEFINE_FINAL_TYPE (NautilusProgressIndicator, nautilus_progress_indicator, ADW_TYPE_BIN);


static void update_operations (NautilusProgressIndicator *self);

static inline gboolean
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
on_remove_finished_operations_timeout (NautilusProgressIndicator *self)
{
    nautilus_progress_info_manager_remove_finished_or_cancelled_infos (self->progress_manager);
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
            g_timeout_add_seconds (REMOVE_FINISHED_OPERATIONS_TIMEOUT,
                                   (GSourceFunc) on_remove_finished_operations_timeout,
                                   self);
    }
}

static void
on_progress_info_cancelled (NautilusProgressIndicator *self)
{
    if (!nautilus_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }
}

static void
on_progress_info_finished (NautilusProgressIndicator *self,
                           NautilusProgressInfo      *info)
{
    NautilusWindow *window;
    gchar *main_label;
    GFile *folder_to_open;

    window = NAUTILUS_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

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
        gboolean was_quick = nautilus_progress_info_get_total_elapsed_time (info) <= OPERATION_MINIMUM_TIME;

        main_label = nautilus_progress_info_get_status (info);
        nautilus_window_show_operation_notification (window,
                                                     main_label,
                                                     folder_to_open,
                                                     was_quick);
        g_free (main_label);
    }

    g_clear_object (&folder_to_open);
}

static GdkPaintable *
get_paintable (GtkListItem          *listitem,
               NautilusProgressInfo *info)
{
    if (info == NULL)
    {
        return NULL;
    }

    GtkWidget *box = gtk_list_item_get_child (listitem);
    GtkWidget *image = gtk_widget_get_first_child (box);

    GdkPaintable *paintable = nautilus_progress_paintable_new (image);

    g_object_bind_property (info, "icon-name", paintable, "icon-name", G_BINDING_SYNC_CREATE);
    g_signal_connect_object (info, "finished", G_CALLBACK (nautilus_progress_paintable_animate_done),
                             paintable, G_CONNECT_SWAPPED);

    g_object_bind_property (info, "progress", paintable, "progress", G_BINDING_SYNC_CREATE);

    return paintable;
}

static void
update_operations (NautilusProgressIndicator *self)
{
    g_autoptr (GList) progress_infos = get_filtered_progress_infos (self);

    g_list_store_remove_all (self->progress_infos_model);

    for (GList *l = progress_infos; l != NULL; l = l->next)
    {
        g_list_store_append (self->progress_infos_model, l->data);
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
on_progress_info_started (NautilusProgressIndicator *self)
{
    schedule_operations_start (self);
}

static void
on_new_progress_info (NautilusProgressInfoManager *manager,
                      NautilusProgressInfo        *info,
                      NautilusProgressIndicator   *self)
{
    g_signal_connect_object (info, "started",
                             G_CALLBACK (on_progress_info_started), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (info, "finished",
                             G_CALLBACK (on_progress_info_finished), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (info, "cancelled",
                             G_CALLBACK (on_progress_info_cancelled), self,
                             G_CONNECT_SWAPPED);
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
on_sidebar_list_activate (GtkListView *view,
                          guint        pos,
                          gpointer     user_data)
{
    NautilusProgressIndicator *self = user_data;
    graphene_point_t p;
    GtkWidget *window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));
    int height = gtk_widget_get_height (self->sidebar_list) / 2;
    int width = gtk_widget_get_width (self->sidebar_list);

    gboolean success = gtk_widget_compute_point (self->sidebar_list, window,
                                                 &GRAPHENE_POINT_INIT (width, height), &p);

    if (success)
    {
        gtk_popover_set_pointing_to (GTK_POPOVER (self->operations_popover),
                                     &(GdkRectangle) {p.x, p.y, 0, 0});
    }

    gtk_popover_popup (GTK_POPOVER (self->operations_popover));
}

static void
on_list_model_n_items_changed (GListModel *model,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
    NautilusProgressIndicator *self = user_data;
    guint n_items = g_list_model_get_n_items (model);
    gboolean reveal = n_items > 0;

    if (reveal != self->reveal)
    {
        self->reveal = reveal;
        g_object_notify (G_OBJECT (self), "reveal");
    }

    if (n_items > MAX_ITEMS_IN_SIDEBAR)
    {
        gtk_widget_set_tooltip_text (self->sidebar_list, ngettext ("Show %d File Operations",
                                                                   "Show %d File Operations",
                                                                   n_items));
    }
    else
    {
        gtk_widget_set_tooltip_text (self->sidebar_list, _("Show File Operations"));
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

GtkWidget *
nautilus_progress_indicator_get_operations_popover (NautilusProgressIndicator *self)
{
    g_return_val_if_fail (NAUTILUS_IS_PROGRESS_INDICATOR (self), NULL);

    return self->operations_popover;
}

static GtkWidget *
operations_list_create_widget (GObject  *item,
                               gpointer  user_data)
{
    NautilusProgressInfo *info = NAUTILUS_PROGRESS_INFO (item);
    GtkWidget *widget;

    widget = nautilus_progress_info_widget_new (info);
    gtk_widget_set_visible (widget, TRUE);

    return widget;
}

static void
nautilus_progress_indicator_get_property (GObject    *object,
                                          guint       property_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (object);

    switch (property_id)
    {
        case (PROP_REVEAL):
        {
            g_value_set_boolean (value, self->reveal);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_progress_indicator_constructed (GObject *object)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (object);

    self->progress_manager = nautilus_progress_info_manager_dup_singleton ();
    g_signal_connect_object (self->progress_manager, "new-progress-info",
                             G_CALLBACK (on_new_progress_info), self,
                             G_CONNECT_DEFAULT);
    g_signal_connect_object (self->progress_manager, "has-viewers-changed",
                             G_CALLBACK (on_progress_has_viewers_changed), self,
                             G_CONNECT_DEFAULT);

    self->progress_infos_model = g_list_store_new (NAUTILUS_TYPE_PROGRESS_INFO);
    g_signal_connect (self->progress_infos_model, "notify::n-items",
                      G_CALLBACK (on_list_model_n_items_changed), self);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->operations_list),
                             G_LIST_MODEL (self->progress_infos_model),
                             (GtkListBoxCreateWidgetFunc) operations_list_create_widget,
                             NULL,
                             NULL);

    update_operations (self);

    GtkSliceListModel *slice_model = gtk_slice_list_model_new (G_LIST_MODEL (self->progress_infos_model), 0,
                                                               MAX_ITEMS_IN_SIDEBAR);
    g_autoptr (GtkNoSelection) selection_model = gtk_no_selection_new (G_LIST_MODEL (slice_model));
    gtk_list_view_set_model (GTK_LIST_VIEW (self->sidebar_list), GTK_SELECTION_MODEL (selection_model));

    GList *all_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    for (GList *l = all_infos; l != NULL; l = l->next)
    {
        on_new_progress_info (self->progress_manager, l->data, self);
    }

    g_signal_connect (self->operations_popover, "show",
                      (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (self->operations_popover, "closed",
                              (GCallback) gtk_widget_grab_focus, self);
}

static void
nautilus_progress_indicator_dispose (GObject *obj)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (obj);

    adw_bin_set_child (ADW_BIN (self), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_PROGRESS_INDICATOR);

    G_OBJECT_CLASS (nautilus_progress_indicator_parent_class)->dispose (obj);
}

static void
nautilus_progress_indicator_finalize (GObject *obj)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (obj);

    unschedule_remove_finished_operations (self);

    g_clear_object (&self->progress_infos_model);
    g_clear_object (&self->progress_manager);

    G_OBJECT_CLASS (nautilus_progress_indicator_parent_class)->finalize (obj);
}

static void
nautilus_progress_indicator_class_init (NautilusProgressIndicatorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = nautilus_progress_indicator_constructed;
    object_class->dispose = nautilus_progress_indicator_dispose;
    object_class->finalize = nautilus_progress_indicator_finalize;
    object_class->get_property = nautilus_progress_indicator_get_property;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-progress-indicator.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_popover);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_list);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, sidebar_list);

    gtk_widget_class_bind_template_callback (widget_class, get_paintable);
    gtk_widget_class_bind_template_callback (widget_class, on_operations_popover_notify_visible);
    gtk_widget_class_bind_template_callback (widget_class, on_sidebar_list_activate);

    properties[PROP_REVEAL] = g_param_spec_boolean ("reveal", NULL, NULL, FALSE,
                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
nautilus_progress_indicator_init (NautilusProgressIndicator *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
