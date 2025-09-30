/*
 * Copyright (C) 2025 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#include "nautilus-date-entry-row.h"

#include "nautilus-date-utilities.h"

#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

struct _NautilusDateEntryRow
{
    AdwEntryRow parent_instance;

    GtkCalendar *calendar;
    GtkPopover *popover;

    GDateTime *date_time;
};

G_DEFINE_FINAL_TYPE (NautilusDateEntryRow, nautilus_date_entry_row, ADW_TYPE_ENTRY_ROW);

enum
{
    PROP_DATE_TIME = 1,
    NUM_PROPERTIES
};
static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static void
date_entry_row_set_date_time (NautilusDateEntryRow *self,
                              GDateTime            *date_time,
                              gboolean              update_entry)
{
    g_autoptr (GDateTime) fallback = NULL;

    if (date_time == NULL)
    {
        fallback = g_date_time_new_now_local ();
        date_time = fallback;
    }

    if (!g_set_date_time (&self->date_time, date_time))
    {
        return;
    }

    gtk_calendar_select_day (self->calendar, self->date_time);

    if (update_entry)
    {
        g_autofree gchar *date_string = g_date_time_format (self->date_time, "%x");
        gtk_editable_set_text (GTK_EDITABLE (self), date_string);
    }

    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DATE_TIME]);
}

static void
text_changed (NautilusDateEntryRow *self)
{
    const gchar *date_text = gtk_editable_get_text (GTK_EDITABLE (self));

    g_autoptr (GDate) date = g_date_new ();
    g_date_set_parse (date, date_text);

    if (!g_date_valid (date))
    {
        gtk_widget_add_css_class (GTK_WIDGET (self), "warning");
        g_clear_pointer (&self->date_time, g_date_time_unref);
        return;
    }
    else
    {
        gtk_widget_remove_css_class (GTK_WIDGET (self), "warning");
        g_autoptr (GDateTime) date_time = g_date_time_new_local (g_date_get_year (date),
                                                                 g_date_get_month (date),
                                                                 g_date_get_day (date),
                                                                 0, 0, 0);
        date_entry_row_set_date_time (self, date_time, FALSE);
    }
}

static void
calendar_date_selected (NautilusDateEntryRow *self,
                        GtkCalendar          *calendar)
{
    g_autoptr (GDateTime) date_time =
        g_date_time_new_local (gtk_calendar_get_year (calendar),
                               gtk_calendar_get_month (calendar) + 1,
                               gtk_calendar_get_day (calendar),
                               0, 0, 0);

    date_entry_row_set_date_time (self, date_time, TRUE);
    gtk_popover_popdown (self->popover);
}

/**
 * Returns: a #GDateTime or NULL if no valid date time set
 */
GDateTime *
nautilus_date_entry_row_get_date_time (NautilusDateEntryRow *self)
{
    g_return_val_if_fail (NAUTILUS_IS_DATE_ENTRY_ROW (self), NULL);

    if (self->date_time != NULL)
    {
        g_date_time_ref (self->date_time);
    }

    return self->date_time;
}

void
nautilus_date_entry_row_set_date_time (NautilusDateEntryRow *self,
                                       GDateTime            *date_time)
{
    g_return_if_fail (NAUTILUS_IS_DATE_ENTRY_ROW (self));

    date_entry_row_set_date_time (self, date_time, TRUE);
}

static void
date_entry_row_set_property (GObject      *object,
                             guint         arg_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    NautilusDateEntryRow *self = NAUTILUS_DATE_ENTRY_ROW (object);

    switch (arg_id)
    {
        case PROP_DATE_TIME:
        {
            GDateTime *date_time = g_value_get_pointer (value);
            date_entry_row_set_date_time (self, date_time, TRUE);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
        }
        break;
    }
}

static void
date_entry_row_get_property (GObject    *object,
                             guint       arg_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    NautilusDateEntryRow *self = NAUTILUS_DATE_ENTRY_ROW (object);

    switch (arg_id)
    {
        case PROP_DATE_TIME:
        {
            if (self->date_time)
            {
                g_date_time_ref (self->date_time);
            }
            g_value_set_pointer (value, self->date_time);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
        }
        break;
    }
}

static void
nautilus_date_entry_row_init (NautilusDateEntryRow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
date_entry_row_dispose (GObject *object)
{
    NautilusDateEntryRow *self = NAUTILUS_DATE_ENTRY_ROW (object);

    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_DATE_ENTRY_ROW);

    g_clear_pointer (&self->date_time, g_date_time_unref);

    G_OBJECT_CLASS (nautilus_date_entry_row_parent_class)->dispose (object);
}

static void
date_entry_row_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_date_entry_row_parent_class)->finalize (object);
}

static void
nautilus_date_entry_row_class_init (NautilusDateEntryRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = date_entry_row_dispose;
    object_class->finalize = date_entry_row_finalize;
    object_class->get_property = date_entry_row_get_property;
    object_class->set_property = date_entry_row_set_property;

    props[PROP_DATE_TIME] =
        g_param_spec_pointer ("date-time", "", "",
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-date-entry-row.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusDateEntryRow, calendar);
    gtk_widget_class_bind_template_child (widget_class, NautilusDateEntryRow, popover);

    gtk_widget_class_bind_template_callback (widget_class, text_changed);
    gtk_widget_class_bind_template_callback (widget_class, calendar_date_selected);
}

/**
 * Creates a new #NautilusDateEntryRow which allows to select a date
 * by either typing or selectiong one from a calendar popover. It
 * provides a "date-time" property that can be listened to for changes.
 *
 * @initial: a #GDateTime to display initially
 */
NautilusDateEntryRow *
nautilus_date_entry_row_new (GDateTime *initial)
{
    return g_object_new (NAUTILUS_TYPE_DATE_ENTRY_ROW,
                         "date-time", initial,
                         NULL);
}
