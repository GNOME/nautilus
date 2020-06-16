/*
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Paolo Borelli <pborelli@katamail.com>
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nautilus-trash-bar.h"

#include "nautilus-files-view.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-trash-monitor.h"

enum
{
    PROP_VIEW = 1,
    NUM_PROPERTIES
};

enum
{
    TRASH_BAR_RESPONSE_EMPTY = 1,
    TRASH_BAR_RESPONSE_RESTORE
};

struct _NautilusTrashBar
{
    GtkInfoBar parent_instance;

    NautilusFilesView *view;
    gulong selection_handler_id;
};

G_DEFINE_TYPE (NautilusTrashBar, nautilus_trash_bar, GTK_TYPE_INFO_BAR)

static void
selection_changed_cb (NautilusFilesView *view,
                      NautilusTrashBar  *bar)
{
    g_autolist (NautilusFile) selection = NULL;
    int count;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    count = g_list_length (selection);

    gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (bar),
                                         TRASH_BAR_RESPONSE_RESTORE,
                                         (count > 0));
}

static void
connect_view_and_update_button (NautilusTrashBar *bar)
{
    bar->selection_handler_id = g_signal_connect (bar->view,
                                                  "selection-changed",
                                                  G_CALLBACK (selection_changed_cb),
                                                  bar);

    selection_changed_cb (bar->view, bar);
}

static void
nautilus_trash_bar_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusTrashBar *bar = NAUTILUS_TRASH_BAR (object);

    switch (prop_id)
    {
        case PROP_VIEW:
        {
            bar->view = g_value_get_object (value);
            connect_view_and_update_button (NAUTILUS_TRASH_BAR (object));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
nautilus_trash_bar_dispose (GObject *obj)
{
    NautilusTrashBar *bar = NAUTILUS_TRASH_BAR (obj);

    g_clear_signal_handler (&bar->selection_handler_id, bar->view);

    G_OBJECT_CLASS (nautilus_trash_bar_parent_class)->dispose (obj);
}

static void
nautilus_trash_bar_trash_state_changed (NautilusTrashMonitor *trash_monitor,
                                        gboolean              state,
                                        gpointer              data)
{
    NautilusTrashBar *bar;

    bar = NAUTILUS_TRASH_BAR (data);

    gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (bar),
                                         TRASH_BAR_RESPONSE_EMPTY,
                                         !nautilus_trash_monitor_is_empty ());
}

static void
nautilus_trash_bar_class_init (NautilusTrashBarClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = nautilus_trash_bar_set_property;
    object_class->dispose = nautilus_trash_bar_dispose;

    g_object_class_install_property (object_class,
                                     PROP_VIEW,
                                     g_param_spec_object ("view",
                                                          "view",
                                                          "the NautilusFilesView",
                                                          NAUTILUS_TYPE_FILES_VIEW,
                                                          G_PARAM_WRITABLE |
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_STATIC_STRINGS));
}

static void
trash_bar_response_cb (GtkInfoBar *infobar,
                       gint        response_id,
                       gpointer    user_data)
{
    NautilusTrashBar *bar;
    GtkWidget *window;

    bar = NAUTILUS_TRASH_BAR (infobar);
    window = gtk_widget_get_toplevel (GTK_WIDGET (bar));

    switch (response_id)
    {
        case TRASH_BAR_RESPONSE_EMPTY:
        {
            nautilus_file_operations_empty_trash (window, TRUE, NULL);
        }
        break;

        case TRASH_BAR_RESPONSE_RESTORE:
        {
            g_autolist (NautilusFile) selection = NULL;
            selection = nautilus_view_get_selection (NAUTILUS_VIEW (bar->view));
            nautilus_restore_files_from_trash (selection, GTK_WINDOW (window));
        }
        break;

        default:
        {
        }
        break;
    }
}

static void
nautilus_trash_bar_init (NautilusTrashBar *bar)
{
    GtkWidget *content_area, *action_area, *w;
    GtkWidget *label;
    PangoAttrList *attrs;

    content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));
    action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));

    gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
                                    GTK_ORIENTATION_HORIZONTAL);

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    label = gtk_label_new (_("Trash"));
    gtk_label_set_attributes (GTK_LABEL (label), attrs);
    pango_attr_list_unref (attrs);

    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (content_area), label);

    w = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
                                 _("_Restore"),
                                 TRASH_BAR_RESPONSE_RESTORE);
    gtk_widget_set_tooltip_text (w,
                                 _("Restore selected items to their original position"));

    w = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
                                 /* Translators: "Empty" is an action (for the trash) , not a state */
                                 _("_Empty"),
                                 TRASH_BAR_RESPONSE_EMPTY);
    gtk_widget_set_tooltip_text (w,
                                 _("Delete all items in the Trash"));

    g_signal_connect_object (nautilus_trash_monitor_get (),
                             "trash-state-changed",
                             G_CALLBACK (nautilus_trash_bar_trash_state_changed),
                             bar,
                             0);
    nautilus_trash_bar_trash_state_changed (nautilus_trash_monitor_get (),
                                            FALSE, bar);

    g_signal_connect (bar, "response",
                      G_CALLBACK (trash_bar_response_cb), bar);
}

GtkWidget *
nautilus_trash_bar_new (NautilusFilesView *view)
{
    return g_object_new (NAUTILUS_TYPE_TRASH_BAR,
                         "view", view,
                         "message-type", GTK_MESSAGE_QUESTION,
                         NULL);
}
