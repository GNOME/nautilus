/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 */

#include "nautilus-query-editor.h"

#include <adwaita.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-query.h"
#include "nautilus-scheme.h"
#include "nautilus-search-directory.h"
#include "nautilus-search-popover.h"
#include "nautilus-localsearch-utilities.h"

struct _NautilusQueryEditor
{
    GtkWidget parent_instance;

    GtkWidget *prefix_icon;
    GtkWidget *text;
    GtkWidget *clear_icon;
    GtkWidget *popover;
    GtkWidget *dropdown_button;
    GtkWidget *search_info_button;
    GtkWidget *status_page;
    GtkWidget *search_settings_button;

    GCancellable *cancellable;

    guint search_changed_idle_id;
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

static GParamSpec *properties[LAST_PROP];

static guint signals[LAST_SIGNAL];

static void nautilus_query_editor_changed (NautilusQueryEditor *editor);

static void entry_activate_cb (NautilusQueryEditor *editor);
static void entry_changed_cb (NautilusQueryEditor *editor);
static void search_popover_date_range_changed_cb (NautilusQueryEditor *editor,
                                                  GPtrArray           *date_range);
static void search_popover_mime_type_changed_cb (NautilusQueryEditor *editor,
                                                 GPtrArray           *mimetypes);
static void search_popover_time_type_changed_cb (NautilusQueryEditor   *editor,
                                                 NautilusSearchTimeType data);
static void search_popover_fts_changed_cb (NautilusQueryEditor *editor,
                                           gboolean             fts_enabled);
static void on_clear_icon_pressed (GtkGestureClick     *gesture,
                                   int                  n_press,
                                   double               x,
                                   double               y,
                                   NautilusQueryEditor *self);
static void on_clear_icon_released (GtkGestureClick     *gesture,
                                    int                  n_press,
                                    double               x,
                                    double               y,
                                    NautilusQueryEditor *self);

G_DEFINE_FINAL_TYPE (NautilusQueryEditor, nautilus_query_editor, GTK_TYPE_WIDGET);

static void
update_filter_button (NautilusQueryEditor *self)
{
    if (nautilus_query_has_active_filter (self->query))
    {
        gtk_widget_add_css_class (self->dropdown_button, "accent");
    }
    else
    {
        gtk_widget_remove_css_class (self->dropdown_button, "accent");
    }
}

static void
update_fts_sensitivity (NautilusQueryEditor *editor)
{
    nautilus_search_popover_set_fts_available (NAUTILUS_SEARCH_POPOVER (editor->popover),
                                               nautilus_query_can_search_content (editor->query));
}

typedef struct
{
    gboolean is_remote_ready;
    gboolean is_remote;
    gboolean is_external_ready;
    gboolean is_external;
    NautilusQueryEditor *self;
} SearchInfoData;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SearchInfoData, g_rc_box_release)

static void
real_update_search_information (SearchInfoData *search_info_data)
{
    g_autoptr (SearchInfoData) search_info = search_info_data;

    if (!search_info_data->is_external_ready ||
        !search_info_data->is_remote_ready)
    {
        return;
    }

    NautilusQueryEditor *editor = search_info->self;
    gboolean is_remote = search_info->is_remote;
    gboolean is_external = search_info->is_external;

    /* Subfolders are disabled */
    if (!nautilus_query_recursive (editor->query))
    {
        adw_status_page_set_description (ADW_STATUS_PAGE (editor->status_page),
                                         _("Search may be slow and will not include "
                                           "subfolders or file contents")
                                         );
    }
    else
    {
        adw_status_page_set_description (ADW_STATUS_PAGE (editor->status_page),
                                         _("Search may be slow and will not include "
                                           "file contents")
                                         );
    }

    if (is_remote)
    {
        adw_status_page_set_title (ADW_STATUS_PAGE (editor->status_page),
                                   _("Remote Location"));
        gtk_widget_set_visible (editor->search_info_button, TRUE);
    }
    else if (is_external)
    {
        adw_status_page_set_title (ADW_STATUS_PAGE (editor->status_page),
                                   _("External Drive"));
        gtk_widget_set_visible (editor->search_info_button, TRUE);
    }
    else if (!nautilus_localsearch_directory_is_tracked (editor->location))
    {
        adw_status_page_set_title (ADW_STATUS_PAGE (editor->status_page),
                                   _("Folder Not in Search Locations"));
        gtk_widget_set_visible (editor->search_info_button, TRUE);
        gtk_widget_set_visible (editor->search_settings_button, TRUE);
    }
    else if (nautilus_localsearch_directory_is_single (editor->location))
    {
        adw_status_page_set_title (ADW_STATUS_PAGE (editor->status_page),
                                   _("Limited Search in this Folder"));
        gtk_widget_set_visible (editor->search_info_button, TRUE);
        gtk_widget_set_visible (editor->search_settings_button, TRUE);


        /* Subfolders are disabled */
        if (!nautilus_query_recursive (editor->query))
        {
            adw_status_page_set_description (ADW_STATUS_PAGE (editor->status_page),
                                             _("Some subfolders will not be included "
                                               "in search results")
                                             );
        }
        else
        {
            adw_status_page_set_description (ADW_STATUS_PAGE (editor->status_page),
                                             _("Search will be slower and will not include "
                                               "file contents for some folders")
                                             );
        }
    }
}

static void
query_filesystem_info_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
    g_autoptr (SearchInfoData) search_info_data = user_data;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInfo) info = g_file_query_filesystem_info_finish (G_FILE (source_object),
                                                                      res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The operation was cancelled and the editor was already freed, bailout. */
        return;
    }

    if (info != NULL &&
        g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE))
    {
        search_info_data->is_remote =
            g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE);
    }

    search_info_data->is_remote_ready = TRUE;
    real_update_search_information (g_steal_pointer (&search_info_data));
}

static void
find_enclosing_mount_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
    g_autoptr (SearchInfoData) search_info_data = user_data;
    g_autoptr (GMount) mount = NULL;
    g_autoptr (GError) error = NULL;

    mount = g_file_find_enclosing_mount_finish (G_FILE (source_object), res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The operation was cancelled and the editor was already freed, bailout. */
        return;
    }

    if (mount != NULL)
    {
        g_autoptr (GVolume) volume = g_mount_get_volume (mount);

        search_info_data->is_external = volume != NULL && is_external_volume (volume);
    }

    search_info_data->is_external_ready = TRUE;
    real_update_search_information (g_steal_pointer (&search_info_data));
}

static void
update_search_information (NautilusQueryEditor *editor)
{
    gtk_widget_set_visible (editor->search_settings_button, FALSE);
    gtk_widget_set_visible (editor->search_info_button, FALSE);

    g_cancellable_cancel (editor->cancellable);
    g_clear_object (&editor->cancellable);
    editor->cancellable = g_cancellable_new ();

    if (editor->location == NULL)
    {
        return;
    }

    g_autofree gchar *uri_scheme = g_file_get_uri_scheme (editor->location);

    if (nautilus_scheme_is_internal (uri_scheme))
    {
        return;
    }

    g_autoptr (SearchInfoData) search_info = g_rc_box_new0 (SearchInfoData);

    search_info->self = editor;

    g_file_query_filesystem_info_async (editor->location,
                                        G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE,
                                        G_PRIORITY_DEFAULT,
                                        editor->cancellable,
                                        query_filesystem_info_cb,
                                        g_rc_box_acquire (search_info));
    g_file_find_enclosing_mount_async (editor->location,
                                       G_PRIORITY_DEFAULT,
                                       editor->cancellable,
                                       find_enclosing_mount_cb,
                                       g_steal_pointer (&search_info));
}

static void
recursive_search_preferences_changed (GSettings           *settings,
                                      gchar               *key,
                                      NautilusQueryEditor *editor)
{
    if (nautilus_query_update_recursive_setting (editor->query))
    {
        update_filter_button (editor);
        nautilus_query_editor_changed (editor);
        update_fts_sensitivity (editor);
        update_search_information (editor);
    }
}


static void
nautilus_query_editor_dispose (GObject *object)
{
    NautilusQueryEditor *editor;

    editor = NAUTILUS_QUERY_EDITOR (object);

    g_clear_handle_id (&editor->search_changed_idle_id, g_source_remove);

    g_clear_object (&editor->location);
    g_clear_object (&editor->query);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          recursive_search_preferences_changed,
                                          object);

    g_cancellable_cancel (editor->cancellable);
    g_clear_object (&editor->cancellable);

    gtk_widget_dispose_template (GTK_WIDGET (editor), NAUTILUS_TYPE_QUERY_EDITOR);

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

void
nautilus_query_editor_select_all_text (NautilusQueryEditor *editor)
{
    if (gtk_widget_get_visible (GTK_WIDGET (editor)))
    {
        gtk_widget_grab_focus (editor->text);
    }
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
                                         GDK_KEY_Escape, GDK_NO_MODIFIER_MASK, "cancel",
                                         NULL);

    /**
     * NautilusQueryEditor::location:
     *
     * Binding target for the slot's location. To be applied to the existing
     * query, or when creating a new one.
     */
    properties[PROP_LOCATION] = g_param_spec_object ("location", NULL, NULL,
                                                     G_TYPE_FILE,
                                                     G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

    /**
     * NautilusQueryEditor::query:
     *
     * The current query of the query editor. It it always synchronized
     * with the filter popover's query.
     */
    properties[PROP_QUERY] = g_param_spec_object ("query",
                                                  "Query of the search",
                                                  "The query that the editor is handling",
                                                  NAUTILUS_TYPE_QUERY,
                                                  G_PARAM_READWRITE);

    g_object_class_install_properties (gobject_class, G_N_ELEMENTS (properties), properties);

    g_type_ensure (NAUTILUS_TYPE_SEARCH_POPOVER);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-query-editor.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, prefix_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, text);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, clear_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, popover);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, dropdown_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, search_info_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, status_page);
    gtk_widget_class_bind_template_child (widget_class, NautilusQueryEditor, search_settings_button);

    gtk_widget_class_bind_template_callback (widget_class, entry_activate_cb);
    gtk_widget_class_bind_template_callback (widget_class, entry_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, search_popover_date_range_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, search_popover_mime_type_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, search_popover_time_type_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, search_popover_fts_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_clear_icon_pressed);
    gtk_widget_class_bind_template_callback (widget_class, on_clear_icon_released);

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
    gtk_widget_class_set_css_name (widget_class, "entry");
    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEARCH_BOX);
}

static void
entry_activate_cb (NautilusQueryEditor *editor)
{
    g_signal_emit (editor, signals[ACTIVATED], 0);
}

static void
entry_changed_internal (NautilusQueryEditor *editor)
{
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE (editor->text));

    editor->search_changed_idle_id = 0;

    if (!nautilus_query_set_text (editor->query, text))
    {
        return;
    }

    nautilus_query_editor_changed (editor);
}

static void
entry_changed_cb (NautilusQueryEditor *editor)
{
    const gchar *text = gtk_editable_get_text (GTK_EDITABLE (editor->text));

    gboolean is_empty = (text == NULL || *text == '\0');
    gtk_widget_set_child_visible (editor->clear_icon, !is_empty);

    g_clear_handle_id (&editor->search_changed_idle_id, g_source_remove);

    if (editor->change_frozen)
    {
        return;
    }

    editor->search_changed_idle_id = g_idle_add_once ((GSourceOnceFunc) entry_changed_internal,
                                                      editor);
}

static void
search_popover_date_range_changed_cb (NautilusQueryEditor *editor,
                                      GPtrArray           *date_range)
{
    if (!nautilus_query_set_date_range (editor->query, date_range))
    {
        return;
    }

    update_filter_button (editor);
    nautilus_query_editor_changed (editor);
}

static void
search_popover_mime_type_changed_cb (NautilusQueryEditor *editor,
                                     GPtrArray           *mimetypes)
{
    if (!nautilus_query_set_mime_types (editor->query, mimetypes))
    {
        return;
    }

    update_filter_button (editor);
    nautilus_query_editor_changed (editor);
}

static void
search_popover_time_type_changed_cb (NautilusQueryEditor    *editor,
                                     NautilusSearchTimeType  data)
{
    if (!nautilus_query_set_search_type (editor->query, data))
    {
        return;
    }

    update_filter_button (editor);
    nautilus_query_editor_changed (editor);
}

static void
search_popover_fts_changed_cb (NautilusQueryEditor *editor,
                               gboolean             fts_enabled)
{
    if (nautilus_query_update_search_content (editor->query))
    {
        update_filter_button (editor);
        nautilus_query_editor_changed (editor);
    }
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
    g_signal_connect (nautilus_preferences,
                      "changed::recursive-search",
                      G_CALLBACK (recursive_search_preferences_changed),
                      editor);

    editor->query = nautilus_query_new ();

    editor->cancellable = g_cancellable_new ();

    gtk_widget_init_template (GTK_WIDGET (editor));

    gtk_widget_set_child_visible (editor->clear_icon, FALSE);
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
    gboolean should_notify;

    g_return_if_fail (NAUTILUS_IS_QUERY_EDITOR (editor));

    /* The client could set us a location that is actually a search directory,
     * like what happens with the slot when updating the query editor location.
     * However here we want the real location used as a model for the search,
     * not the search directory invented uri. */
    directory = nautilus_directory_get (location);
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        NautilusSearchDirectory *search = NAUTILUS_SEARCH_DIRECTORY (directory);
        g_autoptr (GFile) real_location = NULL;

        real_location = nautilus_search_directory_get_search_location (search);

        should_notify = g_set_object (&editor->location, real_location);
    }
    else
    {
        should_notify = g_set_object (&editor->location, location);
    }

    nautilus_query_set_location (editor->query, editor->location);

    update_fts_sensitivity (editor);
    update_search_information (editor);

    gtk_image_set_from_icon_name (GTK_IMAGE (editor->prefix_icon),
                                  (editor->location != NULL ?
                                   "nautilus-folder-search-symbolic" :
                                   "nautilus-edit-find"));
    gtk_text_set_placeholder_text (GTK_TEXT (editor->text),
                                   (editor->location != NULL ?
                                    _("Search current folder") :
                                    _("Search everywhere")));

    if (should_notify)
    {
        g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_LOCATION]);
    }
}

/* Note: This function does not forward mime types to the popover. */
void
nautilus_query_editor_set_query (NautilusQueryEditor *self,
                                 NautilusQuery       *query)
{
    g_autoptr (NautilusQuery) empty_query = NULL;
    g_autofree char *text = NULL;
    g_autofree char *current_text = NULL;

    g_return_if_fail (NAUTILUS_IS_QUERY_EDITOR (self));

    if (query == NULL)
    {
        empty_query = nautilus_query_new ();
        nautilus_query_set_location (empty_query, self->location);
        query = empty_query;

        nautilus_search_popover_reset_mime_types (NAUTILUS_SEARCH_POPOVER (self->popover));
    }

    text = nautilus_query_get_text (query);
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
        gtk_editable_set_position (GTK_EDITABLE (self->text), -1);
    }

    if (g_set_object (&self->query, query))
    {
        g_autoptr (GPtrArray) date_range = nautilus_query_get_date_range (self->query);

        nautilus_search_popover_set_date_range (NAUTILUS_SEARCH_POPOVER (self->popover),
                                                date_range);

        update_fts_sensitivity (self);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_QUERY]);
    }

    self->change_frozen = FALSE;
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
