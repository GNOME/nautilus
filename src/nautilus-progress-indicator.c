/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-progress-indicator.h"

#include "nautilus-progress-info.h"
#include "nautilus-progress-info-manager.h"
#include "nautilus-progress-info-widget.h"
#include "nautilus-progress-paintable.h"
#include "nautilus-window.h"

#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 3 /*s */
#define MAX_ITEMS_IN_SIDEBAR 4

enum
{
    PROP_0,
    PROP_POPOVER_POSITION,
    PROP_REVEAL,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

struct _NautilusProgressIndicator
{
    AdwBin parent_instance;

    guint remove_finished_operations_timeout_id;
    guint operations_button_attention_timeout_id;

    GtkWidget *operations_button;
    GtkWidget *operations_popover;
    GtkWidget *operations_list;
    GListStore *progress_infos_model;
    GtkFilter *filter;
    GtkFilterListModel *filter_model;
    GtkWidget *sidebar_list;

    gboolean reveal;
    GtkPositionType popover_position;

    NautilusProgressInfoManager *progress_manager;
};

G_DEFINE_FINAL_TYPE (NautilusProgressIndicator, nautilus_progress_indicator, ADW_TYPE_BIN);


static void
nautilus_progress_indicator_set_reveal (NautilusProgressIndicator *self,
                                        gboolean                   reveal)
{
    if (reveal != self->reveal)
    {
        self->reveal = reveal;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REVEAL]);
    }
}

static void
on_remove_finished_operations_timeout (NautilusProgressIndicator *self)
{
    GListModel *model = G_LIST_MODEL (self->progress_infos_model);

    self->remove_finished_operations_timeout_id = 0;

    for (guint i = g_list_model_get_n_items (model); i > 0; i--)
    {
        g_autoptr (NautilusProgressInfo) info = g_list_model_get_item (model, i - 1);

        if (nautilus_progress_info_get_is_finished (info) ||
            nautilus_progress_info_get_is_cancelled (info))
        {
            g_list_store_remove (self->progress_infos_model, i - 1);
        }
    }
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
    /* Keep finished operations around while the user is looking at them */
    if (gtk_widget_get_visible (self->operations_popover))
    {
        return;
    }

    if (self->remove_finished_operations_timeout_id == 0)
    {
        self->remove_finished_operations_timeout_id =
            g_timeout_add_seconds_once (REMOVE_FINISHED_OPERATIONS_TIEMOUT,
                                        (GSourceOnceFunc) on_remove_finished_operations_timeout,
                                        self);
    }
}

static void
remove_operations_button_attention_style (NautilusProgressIndicator *self)
{
    gtk_widget_remove_css_class (GTK_WIDGET (self), "needs-attention");
}

static void
on_remove_operations_button_attention_style_timeout (NautilusProgressIndicator *self)
{
    remove_operations_button_attention_style (self);
    self->operations_button_attention_timeout_id = 0;
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

    gtk_widget_add_css_class (GTK_WIDGET (self), "needs-attention");
    self->operations_button_attention_timeout_id =
        g_timeout_add_once (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                            (GSourceOnceFunc) on_remove_operations_button_attention_style_timeout,
                            self);
}

static void
on_progress_info_cancelled (NautilusProgressIndicator *self)
{
    schedule_remove_finished_operations (self);
}

static void
on_progress_info_finished (NautilusProgressIndicator *self,
                           NautilusProgressInfo      *info)
{
    NautilusWindow *window;
    gchar *main_label;
    GFile *folder_to_open;

    window = NAUTILUS_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

    schedule_remove_finished_operations (self);

    folder_to_open = nautilus_progress_info_get_destination (info);
    /* If destination is null, don't show a notification. This happens when the
     * operation is a trash operation, which we already show a different kind of
     * notification */
    if (!gtk_widget_is_visible (self->operations_popover) &&
        folder_to_open != NULL)
    {
        gboolean was_quick = nautilus_progress_info_get_total_elapsed_time (info) <= OPERATION_MINIMUM_TIME;
        if (!was_quick)
        {
            add_operations_button_attention_style (self);
        }

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
on_filter_model_n_items_changed (NautilusProgressIndicator *self)
{
    gboolean reveal = g_list_model_get_n_items (G_LIST_MODEL (self->filter_model)) > 0;

    if (reveal && !self->reveal)
    {
        add_operations_button_attention_style (self);
    }

    nautilus_progress_indicator_set_reveal (self, reveal);
}

static void
on_progress_info_long_running_changed (NautilusProgressIndicator *self)
{
    /* Infos only ever become long running, never the opposite */
    gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_LESS_STRICT);
}

static void
add_progress_info (NautilusProgressIndicator *self,
                   NautilusProgressInfo      *info)
{
    g_list_store_insert (self->progress_infos_model, 0, info);

    g_signal_connect_object (info, "notify::long-running",
                             G_CALLBACK (on_progress_info_long_running_changed), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (info, "finished",
                             G_CALLBACK (on_progress_info_finished), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (info, "cancelled",
                             G_CALLBACK (on_progress_info_cancelled), self,
                             G_CONNECT_SWAPPED);
}

static void
on_new_progress_info (NautilusProgressInfoManager *manager,
                      NautilusProgressInfo        *info,
                      NautilusProgressIndicator   *self)
{
    add_progress_info (self, info);
}

static void
on_operations_popover_notify_visible (NautilusProgressIndicator *self,
                                      GParamSpec                *pspec,
                                      GObject                   *popover)
{
    if (gtk_widget_get_visible (GTK_WIDGET (popover)))
    {
        unschedule_remove_finished_operations (self);
    }
    else
    {
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
    gtk_widget_set_visible (widget, TRUE);

    return widget;
}

static void
direction_changed (GtkWidget        *self,
                   GtkTextDirection  previous_direction,
                   gpointer          user_data)
{
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPOVER_POSITION]);
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
        case (PROP_POPOVER_POSITION):
        {
            /* Handles the default value of zero coming from the breakpoint */
            if (self->popover_position <= GTK_POS_RIGHT)
            {
                guint popover_position = (gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL) ?
                                         GTK_POS_RIGHT : GTK_POS_LEFT;

                g_value_set_enum (value, popover_position);
            }
            else
            {
                g_value_set_enum (value, self->popover_position);
            }
        }
        break;

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
nautilus_progress_indicator_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
    NautilusProgressIndicator *self = NAUTILUS_PROGRESS_INDICATOR (object);

    switch (property_id)
    {
        case (PROP_POPOVER_POSITION):
        {
            self->popover_position = g_value_get_enum (value);
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

    G_OBJECT_CLASS (nautilus_progress_indicator_parent_class)->constructed (object);

    self->progress_manager = nautilus_progress_info_manager_dup_singleton ();
    g_signal_connect_object (self->progress_manager, "new-progress-info",
                             G_CALLBACK (on_new_progress_info), self,
                             G_CONNECT_DEFAULT);

    self->progress_infos_model = g_list_store_new (NAUTILUS_TYPE_PROGRESS_INFO);

    GtkExpression *long_running = gtk_property_expression_new (NAUTILUS_TYPE_PROGRESS_INFO, NULL,
                                                               "long-running");
    self->filter = GTK_FILTER (gtk_bool_filter_new (long_running));
    self->filter_model = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (self->progress_infos_model)),
                                                    g_object_ref (self->filter));
    g_signal_connect_swapped (self->filter_model, "notify::n-items",
                              G_CALLBACK (on_filter_model_n_items_changed), self);

    /* Pick up operations that started before this window was opened. The
     * manager's list is newest first, like ours. */
    for (GList *l = g_list_last (nautilus_progress_info_manager_get_all_infos (self->progress_manager));
         l != NULL; l = l->prev)
    {
        add_progress_info (self, l->data);
    }

    gtk_list_box_bind_model (GTK_LIST_BOX (self->operations_list),
                             G_LIST_MODEL (self->filter_model),
                             (GtkListBoxCreateWidgetFunc) operations_list_create_widget,
                             NULL,
                             NULL);

    GtkSliceListModel *slice = gtk_slice_list_model_new (g_object_ref (G_LIST_MODEL (self->filter_model)),
                                                         0, MAX_ITEMS_IN_SIDEBAR);
    g_autoptr (GtkNoSelection) selection_model = gtk_no_selection_new (G_LIST_MODEL (slice));

    gtk_list_view_set_model (GTK_LIST_VIEW (self->sidebar_list), GTK_SELECTION_MODEL (selection_model));

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
    unschedule_operations_button_attention_style (self);

    g_clear_object (&self->filter_model);
    g_clear_object (&self->filter);
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
    object_class->set_property = nautilus_progress_indicator_set_property;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-progress-indicator.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_popover);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, operations_list);
    gtk_widget_class_bind_template_child (widget_class, NautilusProgressIndicator, sidebar_list);

    gtk_widget_class_bind_template_callback (widget_class, get_paintable);
    gtk_widget_class_bind_template_callback (widget_class, on_operations_popover_notify_visible);

    properties[PROP_POPOVER_POSITION] = g_param_spec_enum ("popover-position", NULL, NULL,
                                                           GTK_TYPE_POSITION_TYPE, 0,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_REVEAL] = g_param_spec_boolean ("reveal", NULL, NULL, FALSE,
                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
nautilus_progress_indicator_init (NautilusProgressIndicator *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "direction-changed", G_CALLBACK (direction_changed), NULL);
}
