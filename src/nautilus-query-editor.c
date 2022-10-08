/*
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *
 */

#include "nautilus-query-editor.h"

#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-search-directory.h"
#include "nautilus-search-popover.h"
#include "nautilus-mime-actions.h"
#include "nautilus-ui-utilities.h"

struct _NautilusQueryEditor
{
    GtkWidget parent_instance;

    GtkWidget *tags_box;
    GtkWidget *text;
    GtkWidget *clear_icon;
    GtkWidget *popover;
    GtkWidget *dropdown_button;

    GtkWidget *mime_types_tag;
    GtkWidget *date_range_tag;

    guint search_changed_timeout_id;
    gboolean change_frozen;

    GFile *location;

    NautilusQuery *query;
};

enum
{
    ACTIVATED,
    FOCUS_VIEW,
    CHANGED,
    CANCEL,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_LOCATION,
    PROP_QUERY,
    LAST_PROP
};

static guint signals[LAST_SIGNAL];

static void entry_activate_cb (GtkWidget           *entry,
                               NautilusQueryEditor *editor);
static void entry_changed_cb (GtkWidget           *entry,
                              NautilusQueryEditor *editor);
static void nautilus_query_editor_changed (NautilusQueryEditor *editor);

G_DEFINE_TYPE (NautilusQueryEditor, nautilus_query_editor, GTK_TYPE_WIDGET);

/* A hunt-and-peck typist types at 25-35 words per minute, which means 342 to 480ms between strokes.
 * An average touch typist types at 50-70 wpm, which means 171 to 240ms "under ideal conditions".
 * A 150ms default search triggering delay is too short even for fast typists in general,
 * so wait 400ms after typing, to improve performance by not spamming search engines: */
#define SEARCH_CHANGED_TIMEOUT 400

static void
update_fts_sensitivity (NautilusQueryEditor *editor)
{
    gboolean fts_sensitive = TRUE;

    if (editor->location)
    {
        g_autoptr (NautilusFile) file = NULL;
        g_autofree gchar *uri = NULL;

        file = nautilus_file_get (editor->location);
        uri = g_file_get_uri (editor->location);

        fts_sensitive = !nautilus_file_is_other_locations (file) &&
                        !g_str_has_prefix (uri, "network://") &&
                        !(nautilus_file_is_remote (file) &&
                          location_settings_search_get_recursive_for_location (editor->location) == NAUTILUS_QUERY_RECURSIVE_NEVER);
        nautilus_search_popover_set_fts_sensitive (NAUTILUS_SEARCH_POPOVER (editor->popover),
                                                   fts_sensitive);
    }
}

static void
recursive_search_preferences_changed (GSettings           *settings,
                                      gchar               *key,
                                      NautilusQueryEditor *editor)
{
    NautilusQueryRecursive recursive;

    if (!editor->query)
    {
        return;
    }

    recursive = location_settings_search_get_recursive ();
    if (recursive != nautilus_query_get_recursive (editor->query))
    {
        nautilus_query_set_recursive (editor->query, recursive);
        nautilus_query_editor_changed (editor);
    }

    update_fts_sensitivity (editor);
}


static void
nautilus_query_editor_dispose (GObject *object)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (object);

    g_clear_handle_id (&editor->search_changed_timeout_id, g_source_remove);

    gtk_widget_unparent (gtk_widget_get_first_child (GTK_WIDGET (editor)));
    g_clear_pointer (&editor->tags_box, gtk_widget_unparent);
    g_clear_pointer (&editor->text, gtk_widget_unparent);
    g_clear_pointer (&editor->dropdown_button, gtk_widget_unparent);
    g_clear_pointer (&editor->clear_icon, gtk_widget_unparent);

    g_clear_object (&editor->location);
    g_clear_object (&editor->query);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          recursive_search_preferences_changed,
                                          object);

    G_OBJECT_CLASS (nautilus_query_editor_parent_class)->dispose (object);
}

static gboolean
nautilus_query_editor_grab_focus (GtkWidget *widget)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (widget);

    if (gtk_widget_get_visible (widget) && !gtk_widget_is_focus (editor->text))
    {
        return gtk_text_grab_focus_without_selecting (GTK_TEXT (editor->text));
    }

    return FALSE;
}

static void
nautilus_query_editor_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            g_value_set_object (value, editor->location);
        }
        break;

        case PROP_QUERY:
        {
            g_value_set_object (value, editor->query);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_query_editor_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NautilusQueryEditor *self;

    self = NAUTILUS_QUERY_EDITOR (object);

    switch (prop_id)
    {
        case PROP_LOCATION:
        {
            nautilus_query_editor_set_location (self, g_value_get_object (value));
        }
        break;

        case PROP_QUERY:
        {
            nautilus_query_editor_set_query (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_query_editor_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_query_editor_parent_class)->finalize (object);
}

static void
nautilus_query_editor_class_init (NautilusQueryEditorClass *class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;
    g_autoptr (GtkShortcut) shortcut = NULL;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = nautilus_query_editor_finalize;
    gobject_class->dispose = nautilus_query_editor_dispose;
    gobject_class->get_property = nautilus_query_editor_get_property;
    gobject_class->set_property = nautilus_query_editor_set_property;

    widget_class = GTK_WIDGET_CLASS (class);
    widget_class->grab_focus = nautilus_query_editor_grab_focus;

    signals[CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 2, NAUTILUS_TYPE_QUERY, G_TYPE_BOOLEAN);

    signals[CANCEL] =
        g_signal_new ("cancel",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[ACTIVATED] =
        g_signal_new ("activated",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[FOCUS_VIEW] =
        g_signal_new ("focus-view",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_Down, 0),
                                 gtk_signal_action_new ("focus-view"));
    gtk_widget_class_add_shortcut (widget_class, shortcut);

    gtk_widget_class_add_binding_signal (widget_class,
                                         GDK_KEY_Escape, 0, "cancel",
                                         NULL);

    /**
     * NautilusQueryEditor::location:
     *
     * The current location of the query editor.
     */
    g_object_class_install_property (gobject_class,
                                     PROP_LOCATION,
                                     g_param_spec_object ("location",
                                                          "Location of the search",
                                                          "The current location of the editor",
                                                          G_TYPE_FILE,
                                                          G_PARAM_READWRITE));

    /**
     * NautilusQueryEditor::query:
     *
     * The current query of the query editor. It it always synchronized
     * with the filter popover's query.
     */
    g_object_class_install_property (gobject_class,
                                     PROP_QUERY,
                                     g_param_spec_object ("query",
                                                          "Query of the search",
                                                          "The query that the editor is handling",
                                                          NAUTILUS_TYPE_QUERY,
                                                          G_PARAM_READWRITE));

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
    gtk_widget_class_set_css_name (widget_class, "entry");
    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEARCH_BOX);
}

GFile *
nautilus_query_editor_get_location (NautilusQueryEditor *editor)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY_EDITOR (editor), NULL);

    return g_object_ref (editor->location);
}

static void
create_query (NautilusQueryEditor *editor)
{
    NautilusQuery *query;
    g_autoptr (NautilusFile) file = NULL;
    gboolean fts_enabled;

    g_return_if_fail (editor->query == NULL);

    fts_enabled = nautilus_search_popover_get_fts_enabled (NAUTILUS_SEARCH_POPOVER (editor->popover));

    if (editor->location == NULL)
    {
        return;
    }

    file = nautilus_file_get (editor->location);
    query = nautilus_query_new ();

    nautilus_query_set_search_content (query, fts_enabled);

    nautilus_query_set_text (query, gtk_editable_get_text (GTK_EDITABLE (editor->text)));
    nautilus_query_set_location (query, editor->location);

    /* We only set the query using the global setting for recursivity here,
     * it's up to the search engine to check weather it can proceed with
     * deep search in the current directory or not. */
    nautilus_query_set_recursive (query, location_settings_search_get_recursive ());

    nautilus_query_editor_set_query (editor, query);
}

static void
entry_activate_cb (GtkWidget           *entry,
                   NautilusQueryEditor *editor)
{
    g_signal_emit (editor, signals[ACTIVATED], 0);
}

static gboolean
entry_changed_internal (NautilusQueryEditor *editor)
{
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE (editor->text));
    gboolean is_empty = (text == NULL && *text == '\0');

    editor->search_changed_timeout_id = 0;

    gtk_widget_set_child_visible (editor->clear_icon, !is_empty);

    if (editor->query == NULL)
    {
        create_query (editor);
    }
    else
    {
        g_autofree gchar *stripped_text = g_strstrip (g_strdup (text));
        nautilus_query_set_text (editor->query, stripped_text);
    }

    nautilus_query_editor_changed (editor);

    return G_SOURCE_REMOVE;
}

static void
entry_changed_cb (GtkWidget           *entry,
                  NautilusQueryEditor *editor)
{
    if (editor->change_frozen)
    {
        return;
    }

    g_clear_handle_id (&editor->search_changed_timeout_id, g_source_remove);
    editor->search_changed_timeout_id = g_timeout_add (SEARCH_CHANGED_TIMEOUT,
                                                       G_SOURCE_FUNC (entry_changed_internal),
                                                       editor);
}

static GtkWidget *
create_tag (NautilusQueryEditor *self,
            const gchar         *text,
            GCallback            reset_callback)
{
    GtkWidget *tag;
    GtkWidget *label;
    GtkWidget *button;

    tag = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_end (tag, 6);
    gtk_widget_set_name (tag, "NautilusQueryEditorTag");

    label = gtk_label_new (text);
    gtk_widget_add_css_class (label, "caption-heading");
    gtk_widget_set_margin_start (label, 12);
    gtk_box_append (GTK_BOX (tag), label);

    button = gtk_button_new ();
    gtk_button_set_icon_name (GTK_BUTTON (button), "window-close-symbolic");
    gtk_widget_add_css_class (button, "flat");
    gtk_widget_add_css_class (button, "circular");
    g_signal_connect_object (button, "clicked",
                             reset_callback, self->popover, G_CONNECT_SWAPPED);
    gtk_box_append (GTK_BOX (tag), button);

    return tag;
}

static void
search_popover_date_range_changed_cb (NautilusSearchPopover *popover,
                                      GPtrArray             *date_range,
                                      gpointer               user_data)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (user_data);

    if (editor->query == NULL)
    {
        create_query (editor);
    }

    if (editor->date_range_tag != NULL)
    {
        gtk_box_remove (GTK_BOX (editor->tags_box), editor->date_range_tag);
        editor->date_range_tag = NULL;
    }

    if (date_range)
    {
        g_autofree gchar *text_for_date_range = NULL;

        text_for_date_range = get_text_for_date_range (date_range, TRUE);
        editor->date_range_tag = create_tag (editor,
                                             text_for_date_range,
                                             G_CALLBACK (nautilus_search_popover_reset_date_range));
        gtk_box_append (GTK_BOX (editor->tags_box), editor->date_range_tag);
    }

    nautilus_query_set_date_range (editor->query, date_range);

    nautilus_query_editor_changed (editor);
    gtk_widget_set_visible (editor->tags_box,
                            (gtk_widget_get_first_child (editor->tags_box) != NULL));
}

static void
search_popover_mime_type_changed_cb (NautilusSearchPopover *popover,
                                     gint                   mimetype_group,
                                     const gchar           *mimetype,
                                     gpointer               user_data)
{
    NautilusQueryEditor *editor;
    g_autoptr (GPtrArray) mimetypes = NULL;

    editor = NAUTILUS_QUERY_EDITOR (user_data);

    if (editor->query == NULL)
    {
        create_query (editor);
    }

    if (editor->mime_types_tag != NULL)
    {
        gtk_box_remove (GTK_BOX (editor->tags_box), editor->mime_types_tag);
        editor->mime_types_tag = NULL;
    }

    /* group 0 is anything */
    if (mimetype_group == 0)
    {
        mimetypes = nautilus_mime_types_group_get_mimetypes (mimetype_group);
    }
    else if (mimetype_group > 0)
    {
        mimetypes = nautilus_mime_types_group_get_mimetypes (mimetype_group);
        editor->mime_types_tag = create_tag (editor,
                                             nautilus_mime_types_group_get_name (mimetype_group),
                                             G_CALLBACK (nautilus_search_popover_reset_mime_types));
        gtk_box_append (GTK_BOX (editor->tags_box), editor->mime_types_tag);
    }
    else
    {
        g_autofree gchar *display_name = NULL;

        mimetypes = g_ptr_array_new_full (1, g_free);
        g_ptr_array_add (mimetypes, g_strdup (mimetype));

        display_name = g_content_type_get_description (mimetype);
        editor->mime_types_tag = create_tag (editor,
                                             display_name,
                                             G_CALLBACK (nautilus_search_popover_reset_mime_types));
        gtk_box_append (GTK_BOX (editor->tags_box), editor->mime_types_tag);
    }
    nautilus_query_set_mime_types (editor->query, mimetypes);

    nautilus_query_editor_changed (editor);
    gtk_widget_set_visible (editor->tags_box,
                            (gtk_widget_get_first_child (editor->tags_box) != NULL));
}

static void
search_popover_time_type_changed_cb (NautilusSearchPopover   *popover,
                                     NautilusQuerySearchType  data,
                                     gpointer                 user_data)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (user_data);

    if (editor->query == NULL)
    {
        create_query (editor);
    }

    nautilus_query_set_search_type (editor->query, data);

    nautilus_query_editor_changed (editor);
}

static void
search_popover_fts_changed_cb (GObject    *popover,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (user_data);

    if (editor->query == NULL)
    {
        create_query (editor);
    }

    nautilus_query_set_search_content (editor->query,
                                       nautilus_search_popover_get_fts_enabled (NAUTILUS_SEARCH_POPOVER (popover)));

    nautilus_query_editor_changed (editor);
}

static void
on_clear_icon_pressed (GtkGestureClick     *gesture,
                       int                  n_press,
                       double               x,
                       double               y,
                       NautilusQueryEditor *self)
{
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_clear_icon_released (GtkGestureClick     *gesture,
                        int                  n_press,
                        double               x,
                        double               y,
                        NautilusQueryEditor *self)
{
    gtk_editable_set_text (GTK_EDITABLE (self->text), "");
}

static void
nautilus_query_editor_init (NautilusQueryEditor *editor)
{
    gboolean rtl = (gtk_widget_get_direction (GTK_WIDGET (editor)) == GTK_TEXT_DIR_RTL);
    GtkWidget *image;
    GtkEventController *controller;

    gtk_widget_set_name (GTK_WIDGET (editor), "NautilusQueryEditor");
    gtk_widget_add_css_class (GTK_WIDGET (editor), "search");

    g_signal_connect (nautilus_preferences,
                      "changed::recursive-search",
                      G_CALLBACK (recursive_search_preferences_changed),
                      editor);

    /* create the search entry */
    image = gtk_image_new_from_icon_name ("system-search-symbolic");
    g_object_set (image, "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION, NULL);
    gtk_widget_set_margin_start (image, 4);
    gtk_widget_set_margin_end (image, 6);
    gtk_widget_set_parent (image, GTK_WIDGET (editor));

    editor->tags_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_hide (editor->tags_box);
    gtk_widget_set_parent (editor->tags_box, GTK_WIDGET (editor));

    editor->text = gtk_text_new ();
    gtk_widget_set_hexpand (editor->text, TRUE);
    gtk_widget_set_parent (editor->text, GTK_WIDGET (editor));

    editor->clear_icon = gtk_image_new_from_icon_name (rtl ? "edit-clear-rtl-symbolic" :
                                                             "edit-clear-symbolic");
    g_object_set (editor->clear_icon, "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION, NULL);
    gtk_widget_set_tooltip_text (editor->clear_icon, _("Clear entry"));
    gtk_widget_set_child_visible (editor->clear_icon, FALSE);
    gtk_widget_set_parent (editor->clear_icon, GTK_WIDGET (editor));

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    g_signal_connect (controller, "pressed", G_CALLBACK (on_clear_icon_pressed), editor);
    g_signal_connect (controller, "released", G_CALLBACK (on_clear_icon_released), editor);
    gtk_widget_add_controller (editor->clear_icon, controller);

    /* setup the search popover */
    editor->popover = nautilus_search_popover_new ();

    g_signal_connect (editor->popover, "show",
                      G_CALLBACK (gtk_widget_grab_focus), NULL);
    g_signal_connect_swapped (editor->popover, "closed",
                              G_CALLBACK (gtk_widget_grab_focus), editor);

    g_object_bind_property (editor, "query",
                            editor->popover, "query",
                            G_BINDING_DEFAULT);

    /* setup the filter menu button */
    editor->dropdown_button = gtk_menu_button_new ();
    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (editor->dropdown_button), "funnel-symbolic");
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (editor->dropdown_button), editor->popover);
    gtk_widget_set_parent (editor->dropdown_button, GTK_WIDGET (editor));
    gtk_widget_add_css_class (editor->dropdown_button, "circular");

    g_signal_connect (editor->text, "activate",
                      G_CALLBACK (entry_activate_cb), editor);
    g_signal_connect (editor->text, "changed",
                      G_CALLBACK (entry_changed_cb), editor);
    g_signal_connect (editor->popover, "date-range",
                      G_CALLBACK (search_popover_date_range_changed_cb), editor);
    g_signal_connect (editor->popover, "mime-type",
                      G_CALLBACK (search_popover_mime_type_changed_cb), editor);
    g_signal_connect (editor->popover, "time-type",
                      G_CALLBACK (search_popover_time_type_changed_cb), editor);
    g_signal_connect (editor->popover, "notify::fts-enabled",
                      G_CALLBACK (search_popover_fts_changed_cb), editor);
}

static void
nautilus_query_editor_changed (NautilusQueryEditor *editor)
{
    if (editor->change_frozen)
    {
        return;
    }

    g_signal_emit (editor, signals[CHANGED], 0, editor->query, TRUE);
}

NautilusQuery *
nautilus_query_editor_get_query (NautilusQueryEditor *editor)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY_EDITOR (editor), NULL);

    if (editor->text == NULL)
    {
        return NULL;
    }

    return editor->query;
}

GtkWidget *
nautilus_query_editor_new (void)
{
    return GTK_WIDGET (g_object_new (NAUTILUS_TYPE_QUERY_EDITOR, NULL));
}

void
nautilus_query_editor_set_location (NautilusQueryEditor *editor,
                                    GFile               *location)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    NautilusDirectory *base_model;
    gboolean should_notify;

    g_return_if_fail (NAUTILUS_IS_QUERY_EDITOR (editor));

    /* The client could set us a location that is actually a search directory,
     * like what happens with the slot when updating the query editor location.
     * However here we want the real location used as a model for the search,
     * not the search directory invented uri. */
    directory = nautilus_directory_get (location);
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        g_autoptr (GFile) real_location = NULL;

        base_model = nautilus_search_directory_get_base_model (NAUTILUS_SEARCH_DIRECTORY (directory));
        real_location = nautilus_directory_get_location (base_model);

        should_notify = g_set_object (&editor->location, real_location);
    }
    else
    {
        should_notify = g_set_object (&editor->location, location);
    }

    if (editor->query == NULL)
    {
        create_query (editor);
    }
    nautilus_query_set_location (editor->query, editor->location);

    update_fts_sensitivity (editor);

    if (should_notify)
    {
        g_object_notify (G_OBJECT (editor), "location");
    }
}

void
nautilus_query_editor_set_query (NautilusQueryEditor *self,
                                 NautilusQuery       *query)
{
    g_autofree char *text = NULL;
    g_autofree char *current_text = NULL;

    g_return_if_fail (NAUTILUS_IS_QUERY_EDITOR (self));

    if (query != NULL)
    {
        text = nautilus_query_get_text (query);
    }

    if (!text)
    {
        text = g_strdup ("");
    }

    self->change_frozen = TRUE;

    current_text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->text)));
    current_text = g_strstrip (current_text);
    if (!g_str_equal (current_text, text))
    {
        gtk_editable_set_text (GTK_EDITABLE (self->text), text);
    }

    if (g_set_object (&self->query, query))
    {
        g_object_notify (G_OBJECT (self), "query");
    }

    self->change_frozen = FALSE;
}

void
nautilus_query_editor_set_text (NautilusQueryEditor *self,
                                const gchar         *text)
{
    g_return_if_fail (NAUTILUS_IS_QUERY_EDITOR (self));
    g_return_if_fail (text != NULL);

    /* The handler of the entry will take care of everything */
    gtk_editable_set_text (GTK_EDITABLE (self->text), text);
}

static gboolean
nautilus_gtk_search_entry_is_keynav_event (guint           keyval,
                                           GdkModifierType state)
{
    if (keyval == GDK_KEY_Tab || keyval == GDK_KEY_KP_Tab ||
        keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up ||
        keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down ||
        keyval == GDK_KEY_Left || keyval == GDK_KEY_KP_Left ||
        keyval == GDK_KEY_Right || keyval == GDK_KEY_KP_Right ||
        keyval == GDK_KEY_Home || keyval == GDK_KEY_KP_Home ||
        keyval == GDK_KEY_End || keyval == GDK_KEY_KP_End ||
        keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_KP_Page_Up ||
        keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down ||
        ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) != 0))
    {
        return TRUE;
    }

    /* Other navigation events should get automatically
     * ignored as they will not change the content of the entry
     */
    return FALSE;
}

gboolean
nautilus_query_editor_handle_event (NautilusQueryEditor   *self,
                                    GtkEventControllerKey *controller,
                                    guint                  keyval,
                                    GdkModifierType        state)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY_EDITOR (self), GDK_EVENT_PROPAGATE);
    g_return_val_if_fail (controller != NULL, GDK_EVENT_PROPAGATE);

    /* Conditions are copied straight from GTK. */
    if (nautilus_gtk_search_entry_is_keynav_event (keyval, state) ||
        keyval == GDK_KEY_space ||
        keyval == GDK_KEY_Menu)
    {
        return GDK_EVENT_PROPAGATE;
    }

    return gtk_event_controller_key_forward (controller, self->text);
}
