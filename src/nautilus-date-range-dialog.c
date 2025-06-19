/*
 * Copyright (C) 2025 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#include "nautilus-date-range-dialog.h"

#include "nautilus-date-entry-row.h"
#include "nautilus-date-utilities.h"

#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#define SPECIFY_RANGE_POS 5

struct _NautilusDateRangeDialog
{
    AdwWindow parent_instance;

    AdwComboRow *date_range_row;
    NautilusDateEntryRow *single_date_row;
    NautilusDateEntryRow *from_entry_row;
    NautilusDateEntryRow *to_entry_row;
    GtkButton *select_button;
};

G_DEFINE_TYPE (NautilusDateRangeDialog, nautilus_date_range_dialog, ADW_TYPE_DIALOG);

enum
{
    DATE_RANGE,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static guint
get_selected_span (guint selected_pos)
{
    switch (selected_pos)
    {
        case 0:
        {
            return 0;
        }

        case 1:
        {
            return 1;
        }

        case 2:
        {
            return 3;
        }

        case 3:
        {
            return 7;
        }

        case 4:
        {
            return 14;
        }

        case SPECIFY_RANGE_POS:
        {
            return G_MAXUINT;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static gboolean
in_date_range_mode (NautilusDateRangeDialog *self)
{
    return SPECIFY_RANGE_POS == adw_combo_row_get_selected (self->date_range_row);
}

static gboolean
has_valid_date_range (NautilusDateRangeDialog *self)
{
    if (in_date_range_mode (self))
    {
        g_autoptr (GDateTime) from = nautilus_date_entry_row_get_date_time (self->from_entry_row);
        g_autoptr (GDateTime) to = nautilus_date_entry_row_get_date_time (self->to_entry_row);

        return from != NULL && to != NULL &&
               /* "from" needs to be earlier than "to" */
               g_date_time_compare (from, to) < 1;
    }
    else
    {
        g_autoptr (GDateTime) date = nautilus_date_entry_row_get_date_time (self->single_date_row);
        return date != NULL;
    }
}

static void
update_confirm_button (NautilusDateRangeDialog *self)
{
    gtk_widget_set_sensitive (GTK_WIDGET (self->select_button), has_valid_date_range (self));
}

static void
changed_range_mode (NautilusDateRangeDialog *self)
{
    gtk_widget_set_visible (GTK_WIDGET (self->single_date_row), !in_date_range_mode (self));
    update_confirm_button (self);
}

static void
try_confirm (NautilusDateRangeDialog *self)
{
    /* Verify that range is actually valid */
    if (!has_valid_date_range (self))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (self->select_button), FALSE);
        return;
    }

    GDateTime *start, *end;

    guint selected_pos = adw_combo_row_get_selected (self->date_range_row);
    if (selected_pos == SPECIFY_RANGE_POS)
    {
        start = nautilus_date_entry_row_get_date_time (self->from_entry_row);
        end = nautilus_date_entry_row_get_date_time (self->to_entry_row);
    }
    else
    {
        g_autoptr (GDateTime) center = nautilus_date_entry_row_get_date_time (self->from_entry_row);

        guint span = get_selected_span (selected_pos);
        start = g_date_time_add_days (center, -span);
        end = g_date_time_add_days (center, span);
    }

    /* Limit date range to before current date */
    g_autoptr (GDateTime) now = g_date_time_new_now_local ();
    if (g_date_time_compare (start, now) == 1)
    {
        g_set_date_time (&start, now);
    }
    if (g_date_time_compare (end, now) == 1)
    {
        g_set_date_time (&end, now);
    }

    g_autoptr (GPtrArray) date_range = g_ptr_array_new_full (2, (GDestroyNotify) g_date_time_unref);
    g_ptr_array_add (date_range, start);
    g_ptr_array_add (date_range, end);

    g_signal_emit_by_name (self, "date-range", date_range);

    adw_dialog_close (ADW_DIALOG (self));
}

static void
set_initial_date_range (NautilusDateRangeDialog *self,
                        GPtrArray               *date_range)
{
    adw_combo_row_set_selected (self->date_range_row, SPECIFY_RANGE_POS);
    changed_range_mode (self);

    GDateTime *from_date_time = g_ptr_array_index (date_range, 0);
    GDateTime *to_date_time = g_ptr_array_index (date_range, 1);

    nautilus_date_entry_row_set_date_time (self->from_entry_row, from_date_time);
    nautilus_date_entry_row_set_date_time (self->to_entry_row, to_date_time);
    update_confirm_button (self);
}

static void
nautilus_date_range_dialog_init (NautilusDateRangeDialog *self)
{
    g_type_ensure (NAUTILUS_TYPE_DATE_ENTRY_ROW);

    gtk_widget_init_template (GTK_WIDGET (self));

    adw_combo_row_set_selected (self->date_range_row, 0);
}

static void
date_range_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_DATE_RANGE_DIALOG);

    G_OBJECT_CLASS (nautilus_date_range_dialog_parent_class)->dispose (object);
}

static void
date_range_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_date_range_dialog_parent_class)->finalize (object);
}

static void
nautilus_date_range_dialog_class_init (NautilusDateRangeDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = date_range_dialog_finalize;
    object_class->dispose = date_range_dialog_dispose;

    signals[DATE_RANGE] = g_signal_new ("date-range",
                                        NAUTILUS_TYPE_DATE_RANGE_DIALOG,
                                        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                        g_cclosure_marshal_VOID__POINTER,
                                        G_TYPE_NONE, 1, G_TYPE_POINTER);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-date-range-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusDateRangeDialog, date_range_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusDateRangeDialog, single_date_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusDateRangeDialog, from_entry_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusDateRangeDialog, to_entry_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusDateRangeDialog, select_button);

    gtk_widget_class_bind_template_callback (widget_class, changed_range_mode);
    gtk_widget_class_bind_template_callback (widget_class, try_confirm);
    gtk_widget_class_bind_template_callback (widget_class, update_confirm_button);
}

/**
 * Creates a new #NautilusDateRangeDialog. This dialog allows to select a date
 * range by either specifying a day with a span around it or by specifying two
 * days. For date entry #NautilusDateEntryRow is used.
 *
 * If a valid date range is confirmed, the "date-range" property is notified.
 *
 * @initial_date_range: a #GPtrArray containing two #GDateTime dates
 */
NautilusDateRangeDialog *
nautilus_date_range_dialog_new (GPtrArray *initial_date_range)
{
    NautilusDateRangeDialog *self = g_object_new (NAUTILUS_TYPE_DATE_RANGE_DIALOG, NULL);

    if (initial_date_range != NULL)
    {
        set_initial_date_range (self, initial_date_range);
    }

    gtk_widget_grab_focus (GTK_WIDGET (self->from_entry_row));

    return self;
}
