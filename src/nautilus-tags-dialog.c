/* nautilus-tags-dialog.c
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "nautilus-tags-dialog.h"
#include "nautilus-tag-manager.h"
#include "nautilus-tag-widget.h"

typedef enum
{
    NO_ACTION,
    UPDATE_TAGS
} ChosenAction;

struct _NautilusTagsDialog
{
    GtkDialog parent;

    NautilusWindow *window;
    GList *selection;

    GtkWidget *cancel_button;
    GtkWidget *update_tags_button;
    GtkWidget *tags_entry;
    GtkWidget *color_button;
    GtkWidget *tags_listbox;
    GtkWidget *selection_tags_box;
    GtkWidget *add_tag_button;

    NautilusTagManager *tag_manager;
    GQueue *all_tags;
    GQueue *selection_tags;
    GQueue *new_selection_tags;

    /* queues that represent all the tags */
    GQueue *list_box_rows;
    GQueue *all_tags_widgets;
    /* initilay it has tags widgets that the selection
     * has, but updates when a tag is removed/added */
    GQueue *selection_tags_widgets;

    ChosenAction action;

    GCancellable *cancellable_selection;
    GCancellable *cancellable_update;
    GCancellable *cancellable_get_files;
};

G_DEFINE_TYPE (NautilusTagsDialog, nautilus_tags_dialog, GTK_TYPE_DIALOG);

static void on_selection_tags_obtained (GObject      *object,
                                        GAsyncResult *res,
                                        gpointer      user_data);

static void on_tags_updated (GObject      *object,
                             GAsyncResult *res,
                             gpointer      user_data);

static void
update_tags (NautilusTagsDialog *dialog)
{
    GQueue *all_tags_check;
    GList *l;
    GList *l_check;
    TagData *tag_data;
    TagData *tag_data_check;

    all_tags_check = nautilus_tag_manager_get_all_tags (dialog->tag_manager);

    for (l = g_queue_peek_head_link (dialog->all_tags), l_check = g_queue_peek_head_link (all_tags_check);
        l != NULL && l_check != NULL; l = l->next, l_check = l_check->next)
    {
        tag_data = l->data;
        tag_data_check = l_check->data;

        if (g_strcmp0 (tag_data->id, tag_data_check->id) != 0 ||
            g_strcmp0 (tag_data->name, tag_data_check->name) != 0)
        {
            return;
        }
    }

    nautilus_tag_manager_get_selection_tags (G_OBJECT (dialog),
                                             dialog->selection,
                                             on_selection_tags_obtained,
                                             dialog->cancellable_selection);
}

static gint
get_tag_position_in_listbox (NautilusTagsDialog *dialog,
                             const gchar        *tag_name)
{
    GList *l;
    gint index = 0;
    g_autofree gchar *casefold_tag_name = NULL;
    gchar *casefold_widget_tag_name;

    casefold_tag_name = g_utf8_casefold (tag_name, -1);

    for (l = g_queue_peek_head_link (dialog->all_tags_widgets), index = 0;
        l != NULL;
        l = l->next, index++)
    {
        casefold_widget_tag_name = g_utf8_casefold (nautilus_tag_widget_get_tag_name (NAUTILUS_TAG_WIDGET (l->data)), -1);

        if (g_strcmp0 (casefold_tag_name, casefold_widget_tag_name) < 0)
        {
            g_free (casefold_widget_tag_name);

            return index;
        }

        g_free (casefold_widget_tag_name);
    }

    return index;
}

static gboolean
on_tag_widget_button_press_event (GtkWidget *old_tag_widget,
                                  GdkEvent  *event,
                                  gpointer   user_data)
{
    NautilusTagsDialog *dialog;
    gboolean got_button;
    guint button;
    GtkWidget *row;
    GtkWidget *tag_widget;
    gint index;

    dialog = NAUTILUS_TAGS_DIALOG (user_data);

    got_button = gdk_event_get_button (event, &button);

    if (got_button && button == GDK_BUTTON_PRIMARY)
    {
        if (nautilus_tag_queue_has_tag (dialog->all_tags,
            nautilus_tag_widget_get_tag_name (NAUTILUS_TAG_WIDGET (old_tag_widget))))
        {
            row = gtk_list_box_row_new ();

            tag_widget = nautilus_tag_widget_new (nautilus_tag_widget_get_tag_name (NAUTILUS_TAG_WIDGET (old_tag_widget)),
                                                  nautilus_tag_widget_get_tag_id (NAUTILUS_TAG_WIDGET (old_tag_widget)),
                                                  FALSE);

            g_queue_push_tail (dialog->list_box_rows, row);

            gtk_container_add (GTK_CONTAINER (row), tag_widget);
            gtk_widget_show_all (row);

            index = get_tag_position_in_listbox (dialog,
                                                 nautilus_tag_widget_get_tag_name (NAUTILUS_TAG_WIDGET (old_tag_widget)));

            gtk_list_box_insert (GTK_LIST_BOX (dialog->tags_listbox),
                                 row,
                                 index);

            g_queue_push_nth (dialog->all_tags_widgets, tag_widget, index);
        }

        g_queue_remove (dialog->selection_tags_widgets, old_tag_widget);

        gtk_widget_destroy (old_tag_widget);
    }

    return FALSE;
}

static void
on_row_activated (GtkListBox    *box,
                  GtkListBoxRow *row,
                  gpointer       user_data)
{
    NautilusTagsDialog *dialog;

    dialog = NAUTILUS_TAGS_DIALOG (user_data);

    GtkWidget *old_tag_widget;
    GtkWidget *tag_widget;

    old_tag_widget = gtk_bin_get_child (GTK_BIN (row));

    tag_widget = nautilus_tag_widget_new (nautilus_tag_widget_get_tag_name (NAUTILUS_TAG_WIDGET (old_tag_widget)),
                                          nautilus_tag_widget_get_tag_id (NAUTILUS_TAG_WIDGET (old_tag_widget)),
                                          TRUE);

    g_signal_connect (tag_widget,
                      "button-press-event",
                      G_CALLBACK (on_tag_widget_button_press_event),
                      dialog);

    gtk_container_add (GTK_CONTAINER (dialog->selection_tags_box), tag_widget);
    gtk_widget_show_all (dialog->selection_tags_box);

    g_queue_push_tail (dialog->selection_tags_widgets, tag_widget);

    g_queue_remove (dialog->all_tags_widgets, old_tag_widget);
    g_queue_remove (dialog->list_box_rows, row);
    gtk_widget_destroy (GTK_WIDGET (row));
}

static void
fill_selection_box (NautilusTagsDialog *dialog)
{
    GList *l;
    GtkWidget *tag_widget;
    TagData *tag_data;
    gint index;
    GtkListBoxRow *row;

    dialog->selection_tags_widgets = g_queue_new ();

    for (l = g_queue_peek_head_link (dialog->selection_tags); l != NULL; l = l->next)
    {
        tag_data = l->data;

        if (nautilus_tag_widget_queue_contains_tag (dialog->selection_tags_widgets,
            tag_data->name) || tag_data->name == NULL)
        {
            continue;
        }

        if (nautilus_tag_widget_queue_contains_tag (dialog->all_tags_widgets,
            tag_data->name))
        {
            tag_widget = nautilus_tag_widget_queue_get_tag_with_name (dialog->all_tags_widgets,
                                                                      tag_data->name);

            index = g_queue_index (dialog->all_tags_widgets, tag_widget);
            g_queue_remove (dialog->all_tags_widgets, tag_widget);

            row = gtk_list_box_get_row_at_index (GTK_LIST_BOX(dialog->tags_listbox), index);
            g_queue_remove (dialog->list_box_rows, row);
            gtk_widget_destroy (GTK_WIDGET (row));
        }

        tag_widget = nautilus_tag_widget_new (tag_data->name, tag_data->id, TRUE);

        g_signal_connect (tag_widget,
                          "button-press-event",
                          G_CALLBACK (on_tag_widget_button_press_event),
                          dialog);

        gtk_container_add (GTK_CONTAINER (dialog->selection_tags_box), tag_widget);

        g_queue_push_tail (dialog->selection_tags_widgets, tag_widget);
    }

    gtk_widget_show_all (dialog->selection_tags_box);


    if (!nautilus_tag_widget_queue_contains_tag (dialog->selection_tags_widgets,
        gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry))) &&
        g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry)), "") != 0)
    {
        gtk_widget_set_sensitive (dialog->add_tag_button, TRUE);
    }
}

static void
fill_list_box (NautilusTagsDialog *dialog)
{
    GList *l;
    TagData *tag_data;
    GtkWidget *row;
    GtkWidget *tag_widget;

    dialog->all_tags_widgets = g_queue_new ();

    for (l = g_queue_peek_head_link (dialog->all_tags); l != NULL; l = l->next)
    {
        tag_data = l->data;

        row = gtk_list_box_row_new ();

        tag_widget = nautilus_tag_widget_new (tag_data->name, tag_data->id, FALSE);

        g_queue_push_tail (dialog->list_box_rows, row);

        gtk_container_add (GTK_CONTAINER (row), tag_widget);
        gtk_widget_show_all (row);

        gtk_container_add (GTK_CONTAINER (dialog->tags_listbox), row);

        g_queue_push_tail (dialog->all_tags_widgets, tag_widget);
    }
}

static void
nautilus_tags_dialog_on_response (NautilusTagsDialog *dialog,
                                  gint                response_id,
                                  gpointer            user_data)
{
    if (response_id == GTK_RESPONSE_OK)
    {
        dialog->action = UPDATE_TAGS;

        update_tags (dialog);
    }
    else
    {
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
on_selection_tags_obtained (GObject      *object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    NautilusTagsDialog *dialog;
    g_autoptr (GError) error = NULL;
    TagData *tag_data;
    GQueue *selection_tags_check;
    GList *l_check;
    GList *l;
    TagData *tag_data_check;
    NautilusTagWidget *tag_widget;
    GTask *task;

    dialog = NAUTILUS_TAGS_DIALOG (object);

    if (dialog->action == NO_ACTION)
    {
        dialog->selection_tags = nautilus_tag_manager_gpointer_task_finish (object, res, &error);

        fill_selection_box (dialog);
    }
    else
    {
        selection_tags_check = nautilus_tag_manager_gpointer_task_finish (object, res, &error);

        for (l = g_queue_peek_head_link (dialog->selection_tags), l_check = g_queue_peek_head_link (selection_tags_check);
            l != NULL && l_check != NULL; l = l->next, l_check = l_check->next)
        {
            tag_data = l->data;
            tag_data_check = l_check->data;

            if (g_strcmp0 (tag_data->id, tag_data_check->id) != 0 ||
                g_strcmp0 (tag_data->name, tag_data_check->name) != 0)
            {
                return;
            }
        }

        dialog->new_selection_tags = g_queue_new ();

        for (l = g_queue_peek_head_link (dialog->selection_tags_widgets); l != NULL; l = l->next)
        {
            tag_widget = NAUTILUS_TAG_WIDGET (l->data);

            tag_data = nautilus_tag_data_new (nautilus_tag_widget_get_tag_id (tag_widget),
                                              nautilus_tag_widget_get_tag_name (tag_widget),
                                              NULL);

            g_queue_push_tail (dialog->new_selection_tags, tag_data);
        }

        nautilus_tag_manager_update_tags (dialog->tag_manager,
                                          G_OBJECT (dialog),
                                          dialog->selection,
                                          dialog->selection_tags,
                                          dialog->new_selection_tags,
                                          on_tags_updated,
                                          dialog->cancellable_update);
    }

    task = user_data;
    g_clear_object (&task);
}

static void
on_tags_updated (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
    NautilusTagsDialog *dialog;
    g_autoptr (GError) error = NULL;
    gboolean result;
    GTask *task;

    dialog = NAUTILUS_TAGS_DIALOG (object);

    result = nautilus_tag_manager_update_tags_finish (object, res, &error);

    task = user_data;
    g_clear_object (&task);

    if (!result)
    {
        g_warning ("something went wrong while updating tags");
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));

}

static void
on_tags_entry_activate (NautilusTagsDialog *dialog)
{
    if (gtk_widget_get_sensitive (dialog->add_tag_button))
    {
        g_signal_emit_by_name (dialog->add_tag_button, "clicked", dialog);
    }
}

static void
on_tags_entry_changed (NautilusTagsDialog *dialog)
{
    /* check if selection has this tag and update add button acordingly */

    if (nautilus_tag_widget_queue_contains_tag (dialog->selection_tags_widgets,
        gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry))) ||
        g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry)), "") == 0 ||
        dialog->selection_tags_widgets == NULL)
    {
        gtk_widget_set_sensitive (dialog->add_tag_button, FALSE);
    }
    else
    {
        gtk_widget_set_sensitive (dialog->add_tag_button, TRUE);
    }
}

static void
on_add_button_clicked (GtkButton *button,
                       gpointer   user_data)
{
    NautilusTagsDialog *dialog;
    g_autofree gchar *tag_id = NULL;
    g_autofree gchar *color_rgb = NULL;
    const gchar *const_tag_id;
    GdkRGBA color;
    GtkWidget *tag_widget;
    GtkWidget *old_tag_widget;
    GtkListBoxRow *row;
    gint index;

    dialog = NAUTILUS_TAGS_DIALOG (user_data);

    /* if tag exists but it's not in selection: move from listbox to box */
    if (nautilus_tag_queue_has_tag (dialog->all_tags,
        gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry))))
    {
        old_tag_widget = nautilus_tag_widget_queue_get_tag_with_name (dialog->all_tags_widgets,
                                                                      gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry)));

        const_tag_id = nautilus_tag_widget_get_tag_id (NAUTILUS_TAG_WIDGET (old_tag_widget));

        tag_widget = nautilus_tag_widget_new (gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry)),
                                              const_tag_id,
                                              TRUE);

        index = g_queue_index (dialog->all_tags_widgets, old_tag_widget);
        g_queue_remove (dialog->all_tags_widgets, old_tag_widget);

        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX(dialog->tags_listbox), index);
        g_queue_remove (dialog->list_box_rows, row);
        gtk_widget_destroy (GTK_WIDGET (row));
    }
    else
    {
        /* if tag doesn't exist and it's not in selection: create new widget for box */
        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog->color_button), &color);
        color_rgb = gdk_rgba_to_string (&color);
        tag_id = g_strdup_printf ("org:gnome:nautilus:tag:%s:%s",
                                  gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry)),
                                  color_rgb);

        tag_widget = nautilus_tag_widget_new (gtk_entry_get_text (GTK_ENTRY (dialog->tags_entry)), tag_id, TRUE);
    }

    g_signal_connect (tag_widget,
                      "button-press-event",
                      G_CALLBACK (on_tag_widget_button_press_event),
                      dialog);

    gtk_container_add (GTK_CONTAINER (dialog->selection_tags_box), tag_widget);

    g_queue_push_tail (dialog->selection_tags_widgets, tag_widget);

    gtk_widget_show_all (dialog->selection_tags_box);

    gtk_entry_set_text (GTK_ENTRY (dialog->tags_entry), "");
}

static void
nautilus_tags_dialog_finalize (GObject *object)
{
    NautilusTagsDialog *dialog;

    dialog = NAUTILUS_TAGS_DIALOG (object);

    g_cancellable_cancel (dialog->cancellable_selection);
    g_clear_object (&dialog->cancellable_selection);

    g_cancellable_cancel (dialog->cancellable_update);
    g_clear_object (&dialog->cancellable_update);

    g_cancellable_cancel (dialog->cancellable_get_files);
    g_clear_object (&dialog->cancellable_get_files);

    if (dialog->all_tags)
    {
        g_queue_free_full (dialog->all_tags, nautilus_tag_data_free);
    }
    if (dialog->selection_tags)
    {
        g_queue_free_full (dialog->selection_tags, nautilus_tag_data_free);
    }
    if (dialog->new_selection_tags)
    {
        g_queue_free_full (dialog->new_selection_tags, nautilus_tag_data_free);
    }
    if (dialog->selection_tags_widgets)
    {
        g_queue_free (dialog->selection_tags_widgets);
    }
    if (dialog->all_tags_widgets)
    {
        g_queue_free (dialog->all_tags_widgets);
    }
    if (dialog->list_box_rows)
    {
        g_queue_free (dialog->list_box_rows);
    }

    g_clear_object (&dialog->tag_manager);

    G_OBJECT_CLASS (nautilus_tags_dialog_parent_class)->finalize (object);
}

static void
nautilus_tags_dialog_class_init (NautilusTagsDialogClass *klass)
{
    GtkWidgetClass *widget_class;
    GObjectClass *oclass;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_tags_dialog_finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-tags-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, cancel_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, update_tags_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, tags_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, color_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, tags_listbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, selection_tags_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusTagsDialog, add_tag_button);

    gtk_widget_class_bind_template_callback (widget_class, nautilus_tags_dialog_on_response);
    gtk_widget_class_bind_template_callback (widget_class, on_tags_entry_activate);
    gtk_widget_class_bind_template_callback (widget_class, on_tags_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked);
}

GtkWidget* nautilus_tags_dialog_new (GList          *selection,
                                     NautilusWindow *window)
{
    NautilusTagsDialog *dialog;

    dialog = g_object_new (NAUTILUS_TYPE_TAGS_DIALOG, "use-header-bar", TRUE, NULL);

    dialog->window = window;
    dialog->selection = selection;

    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (window));

    g_signal_connect (dialog->tags_listbox, "row-activated", G_CALLBACK (on_row_activated), dialog);

    dialog->cancellable_selection = g_cancellable_new ();
    dialog->cancellable_update = g_cancellable_new ();

    dialog->tag_manager = nautilus_tag_manager_new (NULL, NULL, NULL);

    dialog->all_tags = nautilus_tag_manager_get_all_tags (dialog->tag_manager);

    fill_list_box (dialog);

    dialog->action = NO_ACTION;

    nautilus_tag_manager_get_selection_tags (G_OBJECT (dialog),
                                             selection,
                                             on_selection_tags_obtained,
                                             dialog->cancellable_selection);

    return GTK_WIDGET (dialog);
}

static void
nautilus_tags_dialog_init (NautilusTagsDialog *self)
{
    GdkRGBA color;

    gtk_widget_init_template (GTK_WIDGET (self));

    self->list_box_rows = g_queue_new ();

    gdk_rgba_parse (&color, "rgb(220,220,220)");
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->color_button),
                                &color);
}