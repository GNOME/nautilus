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

#include "nautilus-enum-types.h"
#include "nautilus-search-popover.h"
#include "nautilus-mime-actions.h"

#include <glib/gi18n.h>
#include "nautilus-file.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-global-preferences.h"

 #define SEARCH_FILTER_MAX_YEARS 5

struct _NautilusSearchPopover
{
    GtkPopover parent;

    GtkWidget *around_revealer;
    GtkWidget *around_stack;
    GtkWidget *calendar;
    GtkWidget *clear_date_button;
    GtkWidget *dates_listbox;
    GtkWidget *date_entry;
    GtkWidget *date_stack;
    GtkWidget *select_date_button;
    GtkWidget *select_date_button_label;
    GtkWidget *type_label;
    GtkWidget *type_listbox;
    GtkWidget *type_stack;
    GtkWidget *last_used_button;
    GtkWidget *last_modified_button;
    GtkWidget *created_button;
    GtkWidget *full_text_search_button;
    GtkWidget *filename_search_button;

    NautilusQuery *query;
    GtkSingleSelection *other_types_model;

    gboolean fts_enabled;
};

static void          show_date_selection_widgets (NautilusSearchPopover *popover,
                                                  gboolean               visible);

static void          show_other_types_dialog (NautilusSearchPopover *popover);

static void          update_date_label (NautilusSearchPopover *popover,
                                        GPtrArray             *date_range);

G_DEFINE_TYPE (NautilusSearchPopover, nautilus_search_popover, GTK_TYPE_POPOVER)

enum
{
    PROP_0,
    PROP_QUERY,
    PROP_FTS_ENABLED,
    LAST_PROP
};

enum
{
    MIME_TYPE,
    TIME_TYPE,
    DATE_RANGE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


/* Callbacks */

static void
calendar_day_selected (GtkCalendar           *calendar,
                       NautilusSearchPopover *popover)
{
    GDateTime *date;
    GPtrArray *date_range;

    date = gtk_calendar_get_date (calendar);

    date_range = g_ptr_array_new_full (2, (GDestroyNotify) g_date_time_unref);
    g_ptr_array_add (date_range, g_date_time_ref (date));
    g_ptr_array_add (date_range, g_date_time_ref (date));
    update_date_label (popover, date_range);
    g_signal_emit_by_name (popover, "date-range", date_range);

    g_ptr_array_unref (date_range);
    g_date_time_unref (date);
}

/* Range on dates are partially implemented. For now just use it for differentation
 * between a exact day or a range of a first day until now.
 */
static void
setup_date (NautilusSearchPopover *popover,
            NautilusQuery         *query)
{
    GPtrArray *date_range;
    GDateTime *date_initial;

    date_range = nautilus_query_get_date_range (query);

    if (date_range)
    {
        date_initial = g_ptr_array_index (date_range, 0);

        g_signal_handlers_block_by_func (popover->calendar, calendar_day_selected, popover);

        gtk_calendar_select_day (GTK_CALENDAR (popover->calendar), date_initial);

        update_date_label (popover, date_range);

        g_signal_handlers_unblock_by_func (popover->calendar, calendar_day_selected, popover);
    }
}

static void
query_date_changed (GObject               *object,
                    GParamSpec            *pspec,
                    NautilusSearchPopover *popover)
{
    setup_date (popover, NAUTILUS_QUERY (object));
}

static void
clear_date_button_clicked (GtkButton             *button,
                           NautilusSearchPopover *popover)
{
    nautilus_search_popover_reset_date_range (popover);
}

static void
date_entry_activate (GtkEntry              *entry,
                     NautilusSearchPopover *popover)
{
    if (gtk_entry_get_text_length (entry) > 0)
    {
        GDateTime *now;
        GDateTime *date_time;
        GDate *date;

        date = g_date_new ();
        g_date_set_parse (date, gtk_editable_get_text (GTK_EDITABLE (entry)));

        /* Invalid date silently does nothing */
        if (!g_date_valid (date))
        {
            g_date_free (date);
            return;
        }

        now = g_date_time_new_now_local ();
        date_time = g_date_time_new_local (g_date_get_year (date),
                                           g_date_get_month (date),
                                           g_date_get_day (date),
                                           0,
                                           0,
                                           0);

        /* Future dates also silently fails */
        if (g_date_time_compare (date_time, now) != 1)
        {
            GPtrArray *date_range;

            date_range = g_ptr_array_new_full (2, (GDestroyNotify) g_date_time_unref);
            g_ptr_array_add (date_range, g_date_time_ref (date_time));
            g_ptr_array_add (date_range, g_date_time_ref (date_time));
            update_date_label (popover, date_range);
            show_date_selection_widgets (popover, FALSE);
            g_signal_emit_by_name (popover, "date-range", date_range);

            g_ptr_array_unref (date_range);
        }

        g_date_time_unref (now);
        g_date_time_unref (date_time);
        g_date_free (date);
    }
}

static void
dates_listbox_row_activated (GtkListBox            *listbox,
                             GtkListBoxRow         *row,
                             NautilusSearchPopover *popover)
{
    GDateTime *date;
    GDateTime *now;
    GPtrArray *date_range = NULL;

    now = g_date_time_new_now_local ();
    date = g_object_get_data (G_OBJECT (row), "date");
    if (date)
    {
        date_range = g_ptr_array_new_full (2, (GDestroyNotify) g_date_time_unref);
        g_ptr_array_add (date_range, g_date_time_ref (date));
        g_ptr_array_add (date_range, g_date_time_ref (now));
    }
    update_date_label (popover, date_range);
    show_date_selection_widgets (popover, FALSE);
    g_signal_emit_by_name (popover, "date-range", date_range);

    if (date_range)
    {
        g_ptr_array_unref (date_range);
    }
    g_date_time_unref (now);
}

static void
listbox_header_func (GtkListBoxRow         *row,
                     GtkListBoxRow         *before,
                     NautilusSearchPopover *popover)
{
    gboolean show_separator;

    show_separator = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "show-separator"));

    if (show_separator)
    {
        GtkWidget *separator;

        separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

        gtk_list_box_row_set_header (row, separator);
    }
}

static void
select_date_button_clicked (GtkButton             *button,
                            NautilusSearchPopover *popover)
{
    /* Hide the type selection widgets when date selection
     * widgets are shown.
     */
    gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");

    show_date_selection_widgets (popover, TRUE);
}

static void
select_type_button_clicked (GtkButton             *button,
                            NautilusSearchPopover *popover)
{
    GtkListBoxRow *selected_row;

    selected_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (popover->type_listbox));

    gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-list");
    if (selected_row != NULL)
    {
        gtk_widget_grab_focus (GTK_WIDGET (selected_row));
    }

    /* Hide the date selection widgets when the type selection
     * listbox is shown.
     */
    show_date_selection_widgets (popover, FALSE);
}

static void
toggle_calendar_icon_clicked (GtkEntry              *entry,
                              GtkEntryIconPosition   position,
                              NautilusSearchPopover *popover)
{
    const gchar *current_visible_child;
    const gchar *child;
    const gchar *icon_name;
    const gchar *tooltip;

    current_visible_child = gtk_stack_get_visible_child_name (GTK_STACK (popover->around_stack));

    if (g_strcmp0 (current_visible_child, "date-list") == 0)
    {
        child = "date-calendar";
        icon_name = "view-list-symbolic";
        tooltip = _("Show Time Ranges");
    }
    else
    {
        child = "date-list";
        icon_name = "x-office-calendar-symbolic";
        tooltip = _("Use Calendar");
    }

    gtk_stack_set_visible_child_name (GTK_STACK (popover->around_stack), child);
    gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, icon_name);
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, tooltip);
}

static void
types_listbox_row_activated (GtkListBox            *listbox,
                             GtkListBoxRow         *row,
                             NautilusSearchPopover *popover)
{
    gint group;

    group = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "mimetype-group"));

    /* The -1 group stands for the "Other Types" group, for which
     * we should show the mimetype dialog.
     */
    if (group == -1)
    {
        show_other_types_dialog (popover);
    }
    else
    {
        gtk_label_set_label (GTK_LABEL (popover->type_label),
                             nautilus_mime_types_group_get_name (group));

        g_signal_emit_by_name (popover, "mime-type", group, NULL);
    }

    gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");
}

static void
search_time_type_changed (GtkCheckButton        *button,
                          NautilusSearchPopover *popover)
{
    NautilusQuerySearchType type = -1;

    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (popover->last_modified_button)))
    {
        type = NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED;
    }
    else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (popover->last_used_button)))
    {
        type = NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS;
    }
    else
    {
        type = NAUTILUS_QUERY_SEARCH_TYPE_CREATED;
    }

    g_settings_set_enum (nautilus_preferences, "search-filter-time-type", type);

    g_signal_emit_by_name (popover, "time-type", type, NULL);
}

static void
search_fts_mode_changed (GtkToggleButton       *button,
                         NautilusSearchPopover *popover)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (popover->full_text_search_button)) &&
        popover->fts_enabled == FALSE)
    {
        popover->fts_enabled = TRUE;
        g_settings_set_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_FTS_ENABLED, TRUE);
        g_object_notify (G_OBJECT (popover), "fts-enabled");
    }
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (popover->filename_search_button)) &&
             popover->fts_enabled == TRUE)
    {
        popover->fts_enabled = FALSE;
        g_settings_set_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_FTS_ENABLED, FALSE);
        g_object_notify (G_OBJECT (popover), "fts-enabled");
    }
}

/* Auxiliary methods */

static GtkWidget *
create_row_for_label (const gchar *text,
                      gboolean     show_separator)
{
    GtkWidget *row;
    GtkWidget *label;

    row = gtk_list_box_row_new ();

    g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

    label = g_object_new (GTK_TYPE_LABEL,
                          "label", text,
                          "hexpand", TRUE,
                          "xalign", 0.0,
                          "margin-start", 6,
                          NULL);

    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);

    return row;
}

static void
fill_fuzzy_dates_listbox (NautilusSearchPopover *popover)
{
    GDateTime *maximum_dt, *now;
    GtkWidget *row;
    GDateTime *current_date;
    GPtrArray *date_range;
    gint days, max_days;

    days = 1;
    maximum_dt = g_date_time_new_from_unix_local (0);
    now = g_date_time_new_now_local ();
    max_days = SEARCH_FILTER_MAX_YEARS * 365;

    /* Add the no date filter element first */
    row = create_row_for_label (_("Any time"), TRUE);
    gtk_list_box_insert (GTK_LIST_BOX (popover->dates_listbox), row, -1);

    /* This is a tricky loop. The main intention here is that each
     * timeslice (day, week, month) have 2 or 3 entries.
     *
     * For the first appearance of each timeslice, there is made a
     * check in order to be sure that there is no offset added to days.
     */
    while (days <= max_days)
    {
        gchar *label;
        gint normalized;
        gint step;

        if (days < 7)
        {
            /* days */
            normalized = days;
            step = 2;
        }
        else if (days < 30)
        {
            /* weeks */
            normalized = days / 7;
            if (normalized == 1)
            {
                days = 7;
            }
            step = 7;
        }
        else if (days < 365)
        {
            /* months */
            normalized = days / 30;
            if (normalized == 1)
            {
                days = 30;
            }
            step = 90;
        }
        else
        {
            /* years */
            normalized = days / 365;
            if (normalized == 1)
            {
                days = 365;
            }
            step = 365;
        }

        current_date = g_date_time_add_days (now, -days);
        date_range = g_ptr_array_new_full (2, (GDestroyNotify) g_date_time_unref);
        g_ptr_array_add (date_range, g_date_time_ref (current_date));
        g_ptr_array_add (date_range, g_date_time_ref (now));
        label = get_text_for_date_range (date_range, FALSE);
        row = create_row_for_label (label, normalized == 1);
        g_object_set_data_full (G_OBJECT (row),
                                "date",
                                g_date_time_ref (current_date),
                                (GDestroyNotify) g_date_time_unref);

        gtk_list_box_insert (GTK_LIST_BOX (popover->dates_listbox), row, -1);

        g_free (label);
        g_date_time_unref (current_date);
        g_ptr_array_unref (date_range);

        days += step;
    }

    g_date_time_unref (maximum_dt);
    g_date_time_unref (now);
}

static void
fill_types_listbox (NautilusSearchPopover *popover)
{
    GtkWidget *row;
    guint n_groups = nautilus_mime_types_get_number_of_groups ();

    /* Mimetypes */
    for (guint i = 0; i < n_groups; i++)
    {
        /* On the third row, which is right below "Folders", there should be an
         * separator to logically group the types.
         */
        row = create_row_for_label (nautilus_mime_types_group_get_name (i), i == 3);
        g_object_set_data (G_OBJECT (row), "mimetype-group", GINT_TO_POINTER (i));

        gtk_list_box_insert (GTK_LIST_BOX (popover->type_listbox), row, -1);
    }

    /* Other types */
    row = create_row_for_label (_("Other Type…"), TRUE);
    g_object_set_data (G_OBJECT (row), "mimetype-group", GINT_TO_POINTER (-1));
    gtk_list_box_insert (GTK_LIST_BOX (popover->type_listbox), row, -1);
}

static void
show_date_selection_widgets (NautilusSearchPopover *popover,
                             gboolean               visible)
{
    gtk_stack_set_visible_child_name (GTK_STACK (popover->date_stack),
                                      visible ? "date-entry" : "date-button");
    gtk_stack_set_visible_child_name (GTK_STACK (popover->around_stack), "date-list");
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (popover->date_entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       "x-office-calendar-symbolic");

    gtk_widget_set_visible (popover->around_revealer, visible);

    gtk_revealer_set_reveal_child (GTK_REVEALER (popover->around_revealer), visible);
}

static void
on_other_types_dialog_response (GtkDialog             *dialog,
                                gint                   response_id,
                                NautilusSearchPopover *popover)
{
    if (response_id == GTK_RESPONSE_OK)
    {
        GtkStringObject *item;
        const char *mimetype;
        g_autofree gchar *description = NULL;

        item = gtk_single_selection_get_selected_item (popover->other_types_model);
        mimetype = gtk_string_object_get_string (item);
        description = g_content_type_get_description (mimetype);

        gtk_label_set_label (GTK_LABEL (popover->type_label), description);

        g_signal_emit_by_name (popover, "mime-type", -1, mimetype);

        gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");
    }

    g_clear_object (&popover->other_types_model);
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
on_other_types_activate (GtkListView *self,
                         guint        position,
                         gpointer     user_data)
{
    GtkDialog *dialog = GTK_DIALOG (user_data);

    gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
on_other_types_bind (GtkSignalListItemFactory *factory,
                     GtkListItem              *listitem,
                     gpointer                  user_data)
{
    GtkLabel *label;
    GtkStringObject *item;
    g_autofree gchar *description = NULL;

    label = GTK_LABEL (gtk_list_item_get_child (listitem));
    item = GTK_STRING_OBJECT (gtk_list_item_get_item (listitem));

    description = g_content_type_get_description (gtk_string_object_get_string (item));
    gtk_label_set_text (label, description);
}

static void
on_other_types_setup (GtkSignalListItemFactory *factory,
                      GtkListItem              *listitem,
                      gpointer                  user_data)
{
    GtkWidget *label;

    label = gtk_label_new (NULL);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_margin_start (label, 12);
    gtk_widget_set_margin_end (label, 12);
    gtk_widget_set_margin_top (label, 6);
    gtk_widget_set_margin_bottom (label, 6);
    gtk_list_item_set_child (listitem, label);
}

static gchar *
join_type_and_description (GtkStringObject *object,
                           gpointer         user_data)
{
    const gchar *content_type = gtk_string_object_get_string (object);
    g_autofree gchar *description = g_content_type_get_description (content_type);

    return g_strdup_printf ("%s %s", content_type, description);
}

static void
show_other_types_dialog (NautilusSearchPopover *popover)
{
    GtkStringList *string_model;
    GtkStringSorter *sorter;
    GtkSortListModel *sort_model;
    GtkStringFilter *filter;
    GtkFilterListModel *filter_model;
    g_autoptr (GList) mime_infos = NULL;
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *search_entry;
    GtkWidget *scrolled;
    GtkListItemFactory *factory;
    GtkWidget *listview;
    GtkRoot *toplevel;

    gtk_popover_popdown (GTK_POPOVER (popover));

    string_model = gtk_string_list_new (NULL);
    mime_infos = g_content_types_get_registered ();
    for (GList *l = mime_infos; l != NULL; l = l->next)
    {
        gtk_string_list_append (string_model, l->data);
    }
    sorter = gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string"));
    sort_model = gtk_sort_list_model_new (G_LIST_MODEL (string_model), GTK_SORTER (sorter));
    filter = gtk_string_filter_new (gtk_cclosure_expression_new (G_TYPE_STRING,
                                                                 NULL, 0, NULL,
                                                                 G_CALLBACK (join_type_and_description),
                                                                 NULL, NULL));
    filter_model = gtk_filter_list_model_new (G_LIST_MODEL (sort_model), GTK_FILTER (filter));
    popover->other_types_model = gtk_single_selection_new (G_LIST_MODEL (filter_model));

    toplevel = gtk_widget_get_root (GTK_WIDGET (popover));
    dialog = gtk_dialog_new_with_buttons (_("Select type"),
                                          GTK_WINDOW (toplevel),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("Select"), GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 600);

    /* If there are 0 results, make action insensitive */
    g_object_bind_property (filter_model,
                            "n-items",
                            gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK),
                            "sensitive",
                            G_BINDING_DEFAULT);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    search_entry = gtk_search_entry_new ();
    gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (search_entry), content_area);
    g_object_bind_property (search_entry, "text", filter, "search", G_BINDING_SYNC_CREATE);
    gtk_box_append (GTK_BOX (content_area), search_entry);
    gtk_widget_set_margin_start (search_entry, 12);
    gtk_widget_set_margin_end (search_entry, 12);
    gtk_widget_set_margin_top (search_entry, 6);
    gtk_widget_set_margin_bottom (search_entry, 6);

    gtk_box_append (GTK_BOX (content_area), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

    scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    gtk_widget_set_vexpand (scrolled, TRUE);
    gtk_box_append (GTK_BOX (content_area), scrolled);

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (on_other_types_setup), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (on_other_types_bind), NULL);

    listview = gtk_list_view_new (GTK_SELECTION_MODEL (g_object_ref (popover->other_types_model)),
                                  factory);
    g_signal_connect (listview, "activate", G_CALLBACK (on_other_types_activate), dialog);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), listview);

    g_signal_connect (dialog, "response", G_CALLBACK (on_other_types_dialog_response), popover);
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
update_date_label (NautilusSearchPopover *popover,
                   GPtrArray             *date_range)
{
    if (date_range)
    {
        gint days;
        GDateTime *initial_date;
        GDateTime *end_date;
        GDateTime *now;
        gchar *label;

        now = g_date_time_new_now_local ();
        initial_date = g_ptr_array_index (date_range, 0);
        end_date = g_ptr_array_index (date_range, 0);
        days = g_date_time_difference (end_date, initial_date) / G_TIME_SPAN_DAY;

        label = get_text_for_date_range (date_range, TRUE);

        gtk_editable_set_text (GTK_EDITABLE (popover->date_entry), days < 1 ? label : "");

        gtk_widget_set_visible (popover->clear_date_button, TRUE);
        gtk_label_set_label (GTK_LABEL (popover->select_date_button_label), label);

        g_date_time_unref (now);
        g_free (label);
    }
    else
    {
        gtk_label_set_label (GTK_LABEL (popover->select_date_button_label),
                             _("Select Dates…"));
        gtk_editable_set_text (GTK_EDITABLE (popover->date_entry), "");
        gtk_widget_set_visible (popover->clear_date_button, FALSE);
    }
}

void
nautilus_search_popover_set_fts_sensitive (NautilusSearchPopover *popover,
                                           gboolean               sensitive)
{
    gtk_widget_set_sensitive (popover->full_text_search_button, sensitive);
    gtk_widget_set_sensitive (popover->filename_search_button, sensitive);
}

static void
nautilus_search_popover_closed (GtkPopover *popover)
{
    NautilusSearchPopover *self = NAUTILUS_SEARCH_POPOVER (popover);
    GDateTime *now;

    /* Always switch back to the initial states */
    gtk_stack_set_visible_child_name (GTK_STACK (self->type_stack), "type-button");
    show_date_selection_widgets (self, FALSE);

    /* If we're closing an ongoing query, the popover must not
     * clear the current settings.
     */
    if (self->query)
    {
        return;
    }

    now = g_date_time_new_now_local ();

    /* Reselect today at the calendar */
    g_signal_handlers_block_by_func (self->calendar, calendar_day_selected, self);

    gtk_calendar_select_day (GTK_CALENDAR (self->calendar), now);

    g_signal_handlers_unblock_by_func (self->calendar, calendar_day_selected, self);
}

static void
nautilus_search_popover_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    NautilusSearchPopover *self;

    self = NAUTILUS_SEARCH_POPOVER (object);

    switch (prop_id)
    {
        case PROP_QUERY:
        {
            g_value_set_object (value, self->query);
        }
        break;

        case PROP_FTS_ENABLED:
        {
            g_value_set_boolean (value, self->fts_enabled);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_search_popover_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    NautilusSearchPopover *self;

    self = NAUTILUS_SEARCH_POPOVER (object);

    switch (prop_id)
    {
        case PROP_QUERY:
        {
            nautilus_search_popover_set_query (self, g_value_get_object (value));
        }
        break;

        case PROP_FTS_ENABLED:
        {
            self->fts_enabled = g_value_get_boolean (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_search_popover_dispose (GObject *obj)
{
    NautilusSearchPopover *self = NAUTILUS_SEARCH_POPOVER (obj);

    gtk_popover_set_child (GTK_POPOVER (obj), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_SEARCH_POPOVER);

    G_OBJECT_CLASS (nautilus_search_popover_parent_class)->dispose (obj);
}

static void
nautilus_search_popover_class_init (NautilusSearchPopoverClass *klass)
{
    GtkPopoverClass *popover_class = GTK_POPOVER_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = nautilus_search_popover_get_property;
    object_class->set_property = nautilus_search_popover_set_property;
    object_class->dispose = nautilus_search_popover_dispose;

    popover_class->closed = nautilus_search_popover_closed;

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

    /**
     * NautilusSearchPopover::query:
     *
     * The current #NautilusQuery being edited.
     */
    g_object_class_install_property (object_class,
                                     PROP_QUERY,
                                     g_param_spec_object ("query",
                                                          "Query of the popover",
                                                          "The current query being edited",
                                                          NAUTILUS_TYPE_QUERY,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                                     PROP_FTS_ENABLED,
                                     g_param_spec_boolean ("fts-enabled",
                                                           "fts enabled",
                                                           "fts enabled",
                                                           FALSE,
                                                           G_PARAM_READWRITE));

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-search-popover.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, around_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, around_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, clear_date_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, calendar);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, dates_listbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, date_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, date_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, select_date_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, select_date_button_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_listbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, last_used_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, last_modified_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, created_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, full_text_search_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, filename_search_button);

    gtk_widget_class_bind_template_callback (widget_class, calendar_day_selected);
    gtk_widget_class_bind_template_callback (widget_class, clear_date_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, date_entry_activate);
    gtk_widget_class_bind_template_callback (widget_class, dates_listbox_row_activated);
    gtk_widget_class_bind_template_callback (widget_class, select_date_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, select_type_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, toggle_calendar_icon_clicked);
    gtk_widget_class_bind_template_callback (widget_class, types_listbox_row_activated);
    gtk_widget_class_bind_template_callback (widget_class, search_time_type_changed);
    gtk_widget_class_bind_template_callback (widget_class, search_fts_mode_changed);
}

static void
nautilus_search_popover_init (NautilusSearchPopover *self)
{
    NautilusQuerySearchType filter_time_type;

    gtk_widget_init_template (GTK_WIDGET (self));

    /* Fuzzy dates listbox */
    gtk_list_box_set_header_func (GTK_LIST_BOX (self->dates_listbox),
                                  (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                  self,
                                  NULL);

    fill_fuzzy_dates_listbox (self);

    /* Types listbox */
    gtk_list_box_set_header_func (GTK_LIST_BOX (self->type_listbox),
                                  (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                  self,
                                  NULL);

    fill_types_listbox (self);

    gtk_list_box_select_row (GTK_LIST_BOX (self->type_listbox),
                             gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->type_listbox), 0));

    filter_time_type = g_settings_get_enum (nautilus_preferences, "search-filter-time-type");
    if (filter_time_type == NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED)
    {
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->last_modified_button), TRUE);
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->last_used_button), FALSE);
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->created_button), FALSE);
    }
    else if (filter_time_type == NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS)
    {
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->last_modified_button), FALSE);
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->last_used_button), TRUE);
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->created_button), FALSE);
    }
    else
    {
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->last_modified_button), FALSE);
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->last_used_button), FALSE);
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->created_button), TRUE);
    }

    self->fts_enabled = g_settings_get_boolean (nautilus_preferences,
                                                NAUTILUS_PREFERENCES_FTS_ENABLED);
    if (self->fts_enabled)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->full_text_search_button), TRUE);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->filename_search_button), TRUE);
    }
}

GtkWidget *
nautilus_search_popover_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_POPOVER, NULL);
}

/**
 * nautilus_search_popover_get_query:
 * @popover: a #NautilusSearchPopover
 *
 * Gets the current query for @popover.
 *
 * Returns: (transfer none): the current #NautilusQuery from @popover.
 */
NautilusQuery *
nautilus_search_popover_get_query (NautilusSearchPopover *popover)
{
    g_return_val_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover), NULL);

    return popover->query;
}

/**
 * nautilus_search_popover_set_query:
 * @popover: a #NautilusSearchPopover
 * @query (nullable): a #NautilusQuery
 *
 * Sets the current query for @popover.
 *
 * Returns:
 */
void
nautilus_search_popover_set_query (NautilusSearchPopover *popover,
                                   NautilusQuery         *query)
{
    NautilusQuery *previous_query;

    g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

    previous_query = popover->query;

    if (popover->query != query)
    {
        /* Disconnect signals and bindings from the old query */
        if (previous_query)
        {
            g_signal_handlers_disconnect_by_func (previous_query, query_date_changed, popover);
        }

        g_set_object (&popover->query, query);

        if (query)
        {
            /* Date */
            setup_date (popover, query);

            g_signal_connect (query,
                              "notify::date",
                              G_CALLBACK (query_date_changed),
                              popover);
        }
        else
        {
            nautilus_search_popover_reset_mime_types (popover);
            nautilus_search_popover_reset_date_range (popover);
        }
    }
}

void
nautilus_search_popover_reset_mime_types (NautilusSearchPopover *popover)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

    gtk_list_box_select_row (GTK_LIST_BOX (popover->type_listbox),
                             gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->type_listbox), 0));

    gtk_label_set_label (GTK_LABEL (popover->type_label),
                         nautilus_mime_types_group_get_name (0));
    g_signal_emit_by_name (popover, "mime-type", 0, NULL);
    gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");
}

void
nautilus_search_popover_reset_date_range (NautilusSearchPopover *popover)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

    gtk_list_box_select_row (GTK_LIST_BOX (popover->dates_listbox),
                             gtk_list_box_get_row_at_index (GTK_LIST_BOX (popover->dates_listbox), 0));

    update_date_label (popover, NULL);
    show_date_selection_widgets (popover, FALSE);
    g_signal_emit_by_name (popover, "date-range", NULL);
}

gboolean
nautilus_search_popover_get_fts_enabled (NautilusSearchPopover *popover)
{
    return popover->fts_enabled;
}
