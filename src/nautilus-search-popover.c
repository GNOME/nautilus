/* nautilus-search-popover.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <adwaita.h>

#include "nautilus-date-range-dialog.h"
#include "nautilus-date-utilities.h"
#include "nautilus-enum-types.h"
#include "nautilus-enums.h"
#include "nautilus-search-popover.h"
#include "nautilus-mime-actions.h"

#include <glib/gi18n.h>
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-minimal-cell.h"
#include "nautilus-query.h"
#include "nautilus-ui-utilities.h"

 #define SEARCH_FILTER_MAX_YEARS 5

struct _NautilusSearchPopover
{
    GtkPopover parent;

    /* File Type Filter */
    AdwWrapBox *type_wrap_box;

    GtkButton *audio_button;
    GtkButton *documents_button;
    GtkButton *folders_button;
    GtkButton *images_button;
    GtkButton *pdf_button;
    GtkButton *spreadsheets_button;
    GtkButton *text_button;
    GtkButton *videos_button;
    GtkButton *other_types_button;

    /* Specific type only gets shown when set */
    GtkButton *specific_type_button;
    char *specific_mimetype;

    GtkButton *active_type_button;

    /* Time Type */
    GtkLabel *time_type_label;

    /* Date Filter */
    GtkButton *today_button;
    GtkButton *yesterday_button;
    GtkButton *week_button;
    GtkButton *month_button;
    GtkButton *year_button;

    /* Specific date range only gets shown when set */
    GtkButton *specific_date_button;
    GPtrArray *specific_date_range;

    GtkButton *active_date_button;

    /* Other Filter Options */
    GtkWidget *other_options_box;

    gboolean fts_is_available;
    GtkButton *content_and_filename_button;
    GtkButton *filename_only_button;
    GtkButton *content_only_button;

    GtkButton *active_fts_button;

    AdwDialog *type_dialog;
    GtkStack *type_dialog_stack;

    GtkSingleSelection *other_types_model;
};

static void          show_other_types_dialog (NautilusSearchPopover *popover);

G_DEFINE_TYPE (NautilusSearchPopover, nautilus_search_popover, GTK_TYPE_POPOVER)

enum
{
    FTS_CHANGED,
    MIME_TYPE,
    TIME_TYPE,
    DATE_RANGE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
set_active_button (GtkButton **active_button_pointer,
                   GtkButton  *button)
{
    if (*active_button_pointer != NULL)
    {
        gtk_widget_remove_css_class (GTK_WIDGET (*active_button_pointer), "accent");
    }

    if (button != NULL)
    {
        gtk_widget_add_css_class (GTK_WIDGET (button), "accent");
    }

    *active_button_pointer = button;
}

/* Callbacks */

static void
date_button_clicked (NautilusSearchPopover *popover,
                     GtkButton             *button)
{
    if (button == popover->active_date_button)
    {
        set_active_button (&popover->active_date_button, NULL);
        g_signal_emit_by_name (popover, "date-range", NULL);
        return;
    }
    else if (button == popover->specific_date_button)
    {
        set_active_button (&popover->active_date_button, popover->specific_date_button);
        g_signal_emit_by_name (popover, "date-range", popover->specific_date_range);
        return;
    }

    GDateTime *date;
    GDateTime *now;
    GPtrArray *date_range = NULL;

    now = g_date_time_new_now_local ();
    date = g_object_get_data (G_OBJECT (button), "date");
    if (date)
    {
        date_range = g_ptr_array_new_full (2, (GDestroyNotify) g_date_time_unref);
        g_ptr_array_add (date_range, g_date_time_ref (date));
        g_ptr_array_add (date_range, g_date_time_ref (now));
    }
    set_active_button (&popover->active_date_button, button);
    g_signal_emit_by_name (popover, "date-range", date_range);

    if (date_range)
    {
        g_ptr_array_unref (date_range);
    }
    g_date_time_unref (now);
}

static void
date_range_dialog_selected_cb (NautilusSearchPopover *self,
                               GPtrArray             *date_range)
{
    g_set_ptr_array (&self->specific_date_range, date_range);

    g_autofree char *description = nautilus_date_range_to_str (self->specific_date_range, FALSE);
    gtk_button_set_label (self->specific_date_button, description);
    gtk_widget_set_visible (GTK_WIDGET (self->specific_date_button), TRUE);
    set_active_button (&self->active_date_button, self->specific_date_button);

    g_signal_emit_by_name (self, "date-range", self->specific_date_range);
}

static void
show_date_range_dialog_cb (NautilusSearchPopover *self)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

    NautilusDateRangeDialog *dialog = nautilus_date_range_dialog_new (self->specific_date_range);
    adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));

    g_signal_connect_object (dialog, "date-range",
                             G_CALLBACK (date_range_dialog_selected_cb),
                             self, G_CONNECT_SWAPPED);

    gtk_popover_popdown (GTK_POPOVER (self));
}

static void
file_types_button_clicked (NautilusSearchPopover *popover,
                           GtkButton             *button)
{
    g_assert (NAUTILUS_IS_SEARCH_POPOVER (popover));

    if (button == popover->active_type_button)
    {
        set_active_button (&popover->active_type_button, NULL);
        g_signal_emit_by_name (popover, "mime-type", 0, NULL);
    }
    else if (button == popover->specific_type_button)
    {
        set_active_button (&popover->active_type_button, popover->specific_type_button);
        g_signal_emit_by_name (popover, "mime-type", -1, popover->specific_mimetype);
    }
    else
    {
        gint group = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "mimetype-group"));

        /* The -1 group stands for the "Other Types" group, for which
         * we should show the mimetype dialog.
         */
        if (group == -1)
        {
            show_other_types_dialog (popover);
        }
        else
        {
            set_active_button (&popover->active_type_button, button);
            g_signal_emit_by_name (popover, "mime-type", group, NULL);
        }
    }
}

static void
fts_option_clicked (NautilusSearchPopover *self,
                    GtkButton             *button)
{
    if (!self->fts_is_available)
    {
        /* Ignore everything FTS related */
        return;
    }

    gboolean fts_active = (button == self->content_and_filename_button ||
                           button == self->content_only_button);
    gboolean fts_was_active = g_settings_get_boolean (nautilus_preferences,
                                                      NAUTILUS_PREFERENCES_FTS_ENABLED);

    if (fts_active != fts_was_active)
    {
        g_settings_set_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_FTS_ENABLED,
                                fts_active);
        set_active_button (&self->active_fts_button, button);
        g_signal_emit_by_name (self, "fts-changed", fts_active);
    }
}

/* Auxiliary methods */

static void
on_other_types_dialog_response (NautilusSearchPopover *popover)
{
    NautilusMinimalCell *item = gtk_single_selection_get_selected_item (popover->other_types_model);
    const gchar *mimetype = nautilus_minimal_cell_get_subtitle (item);

    g_autofree gchar *display_name = g_content_type_get_description (mimetype);
    gtk_button_set_label (popover->specific_type_button, display_name);
    gtk_widget_set_visible (GTK_WIDGET (popover->specific_type_button), TRUE);
    g_set_str (&popover->specific_mimetype, mimetype);
    set_active_button (&popover->active_type_button, popover->specific_type_button);

    g_signal_emit_by_name (popover, "mime-type", -1, mimetype);

    g_clear_object (&popover->other_types_model);

    adw_dialog_close (popover->type_dialog);
}

static gchar *
join_type_and_description (NautilusMinimalCell *cell)
{
    const gchar *description = nautilus_minimal_cell_get_title (cell);
    const gchar *content_type = nautilus_minimal_cell_get_subtitle (cell);

    return g_strdup_printf ("%s %s", content_type, description);
}

static void
file_type_search_changed (GtkEditable           *search_entry,
                          GParamSpec            *pspec,
                          NautilusSearchPopover *self)
{
    const gchar *string = gtk_editable_get_text (search_entry);

    if (string == NULL || *string == '\0')
    {
        gtk_stack_set_visible_child_name (self->type_dialog_stack, "start");
        gtk_widget_set_sensitive (GTK_WIDGET (adw_dialog_get_default_widget (self->type_dialog)),
                                  FALSE);

        return;
    }

    guint result_count = g_list_model_get_n_items (G_LIST_MODEL (self->other_types_model));

    if (result_count == 0)
    {
        gtk_stack_set_visible_child_name (self->type_dialog_stack, "empty");
        gtk_widget_set_sensitive (GTK_WIDGET (adw_dialog_get_default_widget (self->type_dialog)),
                                  FALSE);
    }
    else
    {
        gtk_stack_set_visible_child_name (self->type_dialog_stack, "results");
        gtk_widget_set_sensitive (GTK_WIDGET (adw_dialog_get_default_widget (self->type_dialog)),
                                  TRUE);
    }
}

static gboolean
click_policy_mapping_get (GValue   *gvalue,
                          GVariant *variant,
                          gpointer  listview)
{
    int click_policy = g_settings_get_enum (nautilus_preferences,
                                            NAUTILUS_PREFERENCES_CLICK_POLICY);

    g_value_set_boolean (gvalue, click_policy == NAUTILUS_CLICK_POLICY_SINGLE);

    return TRUE;
}

static void
show_other_types_dialog (NautilusSearchPopover *popover)
{
    GtkStringFilter *filter;
    GtkFilterListModel *filter_model;
    g_autoptr (GList) mime_infos = NULL;
    GListStore *file_type_list = g_list_store_new (NAUTILUS_TYPE_MINIMAL_CELL);
    g_autoptr (GtkBuilder) builder = NULL;
    AdwToolbarView *toolbar_view;
    GtkWidget *content_area;
    GtkWidget *search_entry;
    GtkListView *listview;
    GtkRoot *toplevel = gtk_widget_get_root (GTK_WIDGET (popover));

    gtk_popover_popdown (GTK_POPOVER (popover));

    mime_infos = g_content_types_get_registered ();
    mime_infos = g_list_sort (mime_infos, (GCompareFunc) g_strcmp0);
    gint scale = gtk_widget_get_scale_factor (GTK_WIDGET (toplevel));
    for (GList *l = mime_infos; l != NULL; l = l->next)
    {
        g_autofree gchar *content_type = l->data;
        g_autofree gchar *description = g_content_type_get_description (content_type);
        g_autoptr (GIcon) icon = g_content_type_get_icon (content_type);
        g_autoptr (NautilusIconInfo) icon_info = nautilus_icon_info_lookup (icon, 32, scale);
        GdkPaintable *paintable = nautilus_icon_info_get_paintable (icon_info);

        g_list_store_append (file_type_list, nautilus_minimal_cell_new (description,
                                                                        content_type,
                                                                        GDK_PAINTABLE (paintable)));
    }

    filter = gtk_string_filter_new (gtk_cclosure_expression_new (G_TYPE_STRING,
                                                                 NULL, 0, NULL,
                                                                 G_CALLBACK (join_type_and_description),
                                                                 NULL, NULL));
    filter_model = gtk_filter_list_model_new (G_LIST_MODEL (file_type_list), GTK_FILTER (filter));
    popover->other_types_model = gtk_single_selection_new (G_LIST_MODEL (filter_model));

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-search-types-dialog.ui");

    popover->type_dialog = ADW_DIALOG (gtk_builder_get_object (builder, "file_types_dialog"));
    search_entry = GTK_WIDGET (gtk_builder_get_object (builder, "search_entry"));
    toolbar_view = ADW_TOOLBAR_VIEW (gtk_builder_get_object (builder, "toolbar_view"));
    popover->type_dialog_stack = GTK_STACK (gtk_builder_get_object (builder, "search_stack"));
    listview = GTK_LIST_VIEW (gtk_builder_get_object (builder, "listview"));

    content_area = adw_toolbar_view_get_content (toolbar_view);
    gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (search_entry), content_area);
    g_object_bind_property (search_entry, "text", filter, "search", G_BINDING_SYNC_CREATE);
    g_signal_connect_after (search_entry, "notify::text",
                            G_CALLBACK (file_type_search_changed), popover);

    gtk_list_view_set_model (listview,
                             GTK_SELECTION_MODEL (g_object_ref (popover->other_types_model)));
    g_settings_bind_with_mapping (nautilus_preferences, NAUTILUS_PREFERENCES_CLICK_POLICY,
                                  listview, "single-click-activate", G_SETTINGS_BIND_GET,
                                  click_policy_mapping_get, NULL, listview, NULL);

    g_signal_connect_swapped (adw_dialog_get_default_widget (popover->type_dialog), "clicked",
                              G_CALLBACK (on_other_types_dialog_response), popover);
    g_signal_connect_swapped (popover->type_dialog, "close-attempt",
                              G_CALLBACK (on_other_types_dialog_response), popover);
    g_signal_connect_swapped (listview, "activate",
                              G_CALLBACK (on_other_types_dialog_response), popover);

    adw_dialog_present (popover->type_dialog, GTK_WIDGET (toplevel));
}

void
nautilus_search_popover_set_fts_available (NautilusSearchPopover *self,
                                           gboolean               available)
{
    self->fts_is_available = available;
    gtk_widget_set_visible (self->other_options_box, available);
}

static void
nautilus_search_popover_dispose (GObject *obj)
{
    NautilusSearchPopover *self = NAUTILUS_SEARCH_POPOVER (obj);

    g_free (self->specific_mimetype);
    g_clear_pointer (&self->specific_date_range, g_ptr_array_unref);

    gtk_popover_set_child (GTK_POPOVER (obj), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_SEARCH_POPOVER);

    G_OBJECT_CLASS (nautilus_search_popover_parent_class)->dispose (obj);
}

static void
nautilus_search_popover_class_init (NautilusSearchPopoverClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_search_popover_dispose;

    signals[DATE_RANGE] = g_signal_new ("date-range",
                                        NAUTILUS_TYPE_SEARCH_POPOVER,
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL,
                                        NULL,
                                        g_cclosure_marshal_generic,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_PTR_ARRAY);

    signals[FTS_CHANGED] = g_signal_new ("fts-changed",
                                         NAUTILUS_TYPE_SEARCH_POPOVER,
                                         G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                         g_cclosure_marshal_generic,
                                         G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals[MIME_TYPE] = g_signal_new ("mime-type",
                                       NAUTILUS_TYPE_SEARCH_POPOVER,
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL,
                                       NULL,
                                       g_cclosure_marshal_generic,
                                       G_TYPE_NONE,
                                       2,
                                       G_TYPE_INT,
                                       G_TYPE_STRING);

    signals[TIME_TYPE] = g_signal_new ("time-type",
                                       NAUTILUS_TYPE_SEARCH_POPOVER,
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL,
                                       NULL,
                                       g_cclosure_marshal_generic,
                                       G_TYPE_NONE,
                                       1,
                                       G_TYPE_INT);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-search-popover.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_wrap_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, audio_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, documents_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, folders_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, images_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, pdf_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, spreadsheets_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, text_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, videos_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, specific_type_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, other_types_button);

    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, time_type_label);

    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, today_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, yesterday_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, week_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, month_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, year_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, specific_date_button);

    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, other_options_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, content_and_filename_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, filename_only_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, content_only_button);

    gtk_widget_class_bind_template_callback (widget_class, file_types_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, date_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, show_date_range_dialog_cb);
    gtk_widget_class_bind_template_callback (widget_class, fts_option_clicked);
}

static gint
sort_mime_type_tag (GtkButton **button_ptr1,
                    GtkButton **button_ptr2)
{
    g_return_val_if_fail (GTK_IS_BUTTON (*button_ptr1), -1);

    AdwButtonContent *content1 = (AdwButtonContent *) gtk_button_get_child (*button_ptr1);
    AdwButtonContent *content2 = (AdwButtonContent *) gtk_button_get_child (*button_ptr2);

    const char *text1 = adw_button_content_get_label (content1);
    const char *text2 = adw_button_content_get_label (content2);

    /* Buttons get placed in reverse, so give inverted sort results */
    return -1 * g_utf8_collate (text1, text2);
}

static void
reposition_mime_type_tag (GtkWidget             *type_tag,
                          NautilusSearchPopover *self)
{
    adw_wrap_box_reorder_child_after (self->type_wrap_box, type_tag, NULL);
}

static void
mime_tag_set_data (GtkButton *mime_tag,
                   gint       mime_group)
{
    g_object_set_data (G_OBJECT (mime_tag), "mimetype-group",
                       GINT_TO_POINTER (mime_group));
}

static void
date_tag_set_data (GtkButton *date_tag,
                   GDateTime *time)
{
    g_object_set_data_full (G_OBJECT (date_tag), "date",
                            time, (GDestroyNotify) g_date_time_unref);
}

static const char *
time_type_to_label (NautilusSearchTimeType time_type)
{
    if (time_type == NAUTILUS_SEARCH_TIME_TYPE_LAST_ACCESS)
    {
        return _("Date Used");
    }
    else if (time_type == NAUTILUS_SEARCH_TIME_TYPE_CREATED)
    {
        return _("Date Created");
    }
    else /* time_type == NAUTILUS_SEARCH_TIME_TYPE_LAST_MODIFIED */
    {
        return _("Date Modified");
    }
}

static void
time_type_changed (GAction               *action,
                   GVariant              *value,
                   NautilusSearchPopover *self)
{
    NautilusSearchTimeType time_type = g_variant_get_uint16 (value);
    NautilusSearchTimeType old_time_type = g_settings_get_enum (nautilus_preferences,
                                                                "search-filter-time-type");

    if (time_type == old_time_type)
    {
        return;
    }

    g_simple_action_set_state (G_SIMPLE_ACTION (action), value);

    g_settings_set_enum (nautilus_preferences, "search-filter-time-type", time_type);
    const char *label = time_type_to_label (time_type);
    gtk_label_set_label (self->time_type_label, label);

    g_signal_emit_by_name (self, "time-type", time_type);
}

static void
nautilus_search_popover_init (NautilusSearchPopover *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    /* File Type Filter */
    mime_tag_set_data (self->audio_button, 5);
    mime_tag_set_data (self->documents_button, 3);
    mime_tag_set_data (self->folders_button, 2);
    mime_tag_set_data (self->images_button, 7);
    mime_tag_set_data (self->pdf_button, 6);
    mime_tag_set_data (self->spreadsheets_button, 9);
    mime_tag_set_data (self->text_button, 10);
    mime_tag_set_data (self->videos_button, 11);
    mime_tag_set_data (self->other_types_button, -1);
    set_active_button (&self->active_type_button, NULL);

    /* sort type buttons alphabetically */
    g_autoptr (GPtrArray) mime_type_array = g_ptr_array_new ();
    g_ptr_array_add (mime_type_array, self->audio_button);
    g_ptr_array_add (mime_type_array, self->documents_button);
    g_ptr_array_add (mime_type_array, self->folders_button);
    g_ptr_array_add (mime_type_array, self->images_button);
    g_ptr_array_add (mime_type_array, self->pdf_button);
    g_ptr_array_add (mime_type_array, self->spreadsheets_button);
    g_ptr_array_add (mime_type_array, self->text_button);
    g_ptr_array_add (mime_type_array, self->videos_button);

    g_ptr_array_sort (mime_type_array, (GCompareFunc) sort_mime_type_tag);
    g_ptr_array_foreach (mime_type_array, (GFunc) reposition_mime_type_tag, self);

    /* Time Type */
    NautilusSearchTimeType time_type = g_settings_get_enum (nautilus_preferences,
                                                            "search-filter-time-type");
    const char *label = time_type_to_label (time_type);
    gtk_label_set_label (self->time_type_label, label);

    g_autoptr (GSimpleAction) action = g_simple_action_new_stateful (
        "time-type-changed", G_VARIANT_TYPE_UINT16, g_variant_new_uint16 (time_type));
    g_simple_action_set_enabled (action, TRUE);
    g_signal_connect_object (action, "change-state", G_CALLBACK (time_type_changed), self, 0);

    GSimpleActionGroup *action_group = g_simple_action_group_new ();
    g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
    gtk_widget_insert_action_group (GTK_WIDGET (self), "search-popover", G_ACTION_GROUP (action_group));

    /* Date Filter */
    g_autoptr (GDateTime) now = g_date_time_new_now_local ();
    g_autoptr (GDateTime) today =
        g_date_time_new_local (g_date_time_get_year (now),
                               g_date_time_get_month (now),
                               g_date_time_get_day_of_month (now),
                               0, 0, 0);

    date_tag_set_data (self->today_button, g_date_time_add_days (today, 0));
    date_tag_set_data (self->yesterday_button, g_date_time_add_days (today, -1));
    date_tag_set_data (self->week_button, g_date_time_add_weeks (today, -1));
    date_tag_set_data (self->month_button, g_date_time_add_months (today, -1));
    date_tag_set_data (self->year_button, g_date_time_add_years (today, -1));
    set_active_button (&self->active_date_button, NULL);

    /* Other Filter Options */
    if (g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_FTS_ENABLED))
    {
        set_active_button (&self->active_fts_button, self->content_and_filename_button);
    }
    else
    {
        set_active_button (&self->active_fts_button, self->filename_only_button);
    }
}

GtkWidget *
nautilus_search_popover_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_POPOVER, NULL);
}

void
nautilus_search_popover_reset_mime_types (NautilusSearchPopover *popover)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

    set_active_button (&popover->active_type_button, NULL);

    g_signal_emit_by_name (popover, "mime-type", 0, NULL);
}

void
nautilus_search_popover_set_date_range (NautilusSearchPopover *popover,
                                        GPtrArray             *date_range)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

    if (date_range != NULL)
    {
        g_set_ptr_array (&popover->specific_date_range, date_range);
        gtk_widget_set_visible (GTK_WIDGET (popover->specific_date_button), TRUE);
        set_active_button (&popover->active_date_button, popover->specific_date_button);
        g_signal_emit_by_name (popover, "date-range", date_range);
    }
    else
    {
        nautilus_search_popover_reset_date_range (popover);
    }
}

void
nautilus_search_popover_reset_date_range (NautilusSearchPopover *popover)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

    set_active_button (&popover->active_date_button, NULL);
    g_signal_emit_by_name (popover, "date-range", NULL);
}
