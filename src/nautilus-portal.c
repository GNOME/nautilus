/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#define G_LOG_DOMAIN "nautilus-dbus"

#include "nautilus-portal.h"

#include <config.h>
#include <glib/gi18n.h>
#include <xdp-gnome/externalwindow.h>
#include <xdp-gnome/request.h>
#include <xdp-gnome/xdg-desktop-portal-dbus.h>

#include "nautilus-file-chooser.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"

#define DESKTOP_PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"

/* https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html#org-freedesktop-portal-request-response
 */
#define RESPONSE_SUCCESS 0
#define RESPONSE_USER_CANCELLED 1
#define RESPONSE_OTHER 2

struct _NautilusPortal
{
    GObject parent;

    XdpImplFileChooser *impl_file_chooser_skeleton;
};

G_DEFINE_TYPE (NautilusPortal, nautilus_portal, G_TYPE_OBJECT);

typedef struct
{
    NautilusPortal *self;

    GDBusMethodInvocation *invocation;
    Request *request;

    ExternalWindow *external_parent;
    GtkWindow *window;

    GActionGroup *choices_action_group;
    char **filenames_to_save;
} FileChooserData;

static void
file_chooser_data_free (gpointer data)
{
    FileChooserData *fc_data = data;

    if (fc_data->request != NULL)
    {
        g_signal_handlers_disconnect_by_data (fc_data->request, fc_data);
        g_clear_object (&fc_data->request);
    }
    g_clear_object (&fc_data->external_parent);

    if (fc_data->window != NULL)
    {
        g_signal_handlers_disconnect_by_data (fc_data->window, fc_data);
        g_clear_object (&fc_data->window);
    }

    g_clear_pointer (&fc_data->filenames_to_save, g_strfreev);

    g_clear_object (&fc_data->choices_action_group);

    g_free (fc_data);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (FileChooserData, file_chooser_data_free)

static void
save_window_size (GtkWindow *window)
{
    int width, height;

    gtk_window_get_default_size (window, &width, &height);
    g_settings_set (nautilus_window_state, NAUTILUS_WINDOW_STATE_INITIAL_SIZE_FILE_CHOOSER,
                    "(ii)", width, height);
}

static void
complete_file_chooser (FileChooserData *data,
                       int              response,
                       GVariantBuilder *results)
{
    const char *method_name = g_dbus_method_invocation_get_method_name (data->invocation);

    if (strcmp (method_name, "OpenFile") == 0)
    {
        xdp_impl_file_chooser_complete_open_file (data->self->impl_file_chooser_skeleton,
                                                  data->invocation,
                                                  response,
                                                  g_variant_builder_end (results));
    }
    else if (strcmp (method_name, "SaveFile") == 0)
    {
        xdp_impl_file_chooser_complete_save_file (data->self->impl_file_chooser_skeleton,
                                                  data->invocation,
                                                  response,
                                                  g_variant_builder_end (results));
    }
    else if (strcmp (method_name, "SaveFiles") == 0)
    {
        xdp_impl_file_chooser_complete_save_files (data->self->impl_file_chooser_skeleton,
                                                   data->invocation,
                                                   response,
                                                   g_variant_builder_end (results));
    }
    else
    {
        g_assert_not_reached ();
    }

    request_unexport (data->request);

    save_window_size (data->window);
    gtk_window_destroy (data->window);

    g_application_release (g_application_get_default ());
}

static void
build_uris_variant (GVariantBuilder *uris,
                    GList           *locations)
{
    for (GList *l = locations; l != NULL; l = l->next)
    {
        GFile *location = G_FILE (l->data);
        const char *path = g_file_peek_path (location);
        g_autofree char *file_uri = NULL;
        /* Backends must normalize URIs of locations selected by the user into
         * “file://” URIs. URIs that cannot be normalized should be discarded.
         */
        if (g_file_has_uri_scheme (location, "file"))
        {
            file_uri = g_file_get_uri (location);
        }
        else if (path != NULL)
        {
            file_uri = g_filename_to_uri (path, NULL, NULL);
        }

        if (file_uri != NULL)
        {
            g_variant_builder_add_value (uris, g_variant_new_take_string (g_steal_pointer (&file_uri)));
        }
    }
}

static GVariant *
get_choices (GActionGroup *action_group,
             gboolean     *writable)
{
    g_auto (GStrv) actions = g_action_group_list_actions (action_group);
    g_auto (GVariantBuilder) builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(ss)"));

    for (guint i = 0; actions[i] != NULL; i++)
    {
        GAction *action = g_action_map_lookup_action (G_ACTION_MAP (action_group), actions[i]);
        const char *action_name = g_action_get_name (action);
        const GVariantType *state_type = g_action_get_state_type (action);
        g_autoptr (GVariant) state = g_action_get_state (action);

        if (g_strcmp0 (actions[i], "x-nautilus-is-read-only") == 0)
        {
            *writable = !g_variant_get_boolean (state);
        }
        else if (g_strcmp0 (g_variant_type_peek_string (state_type), "b") == 0)
        {
            char *value = g_variant_get_boolean (state) ? "true" : "false";
            g_variant_builder_add (&builder, "(ss)", action_name, value);
        }
        else if (g_strcmp0 (g_variant_type_peek_string (state_type), "s") == 0)
        {
            g_variant_builder_add (&builder, "(ss)", action_name, g_variant_get_string (state, NULL));
        }
        else
        {
            g_assert_not_reached ();
        }
    }

    return g_variant_builder_end (&builder);
}

static gboolean
on_file_chooser_accepted (gpointer       user_data,
                          GList         *locations,
                          GtkFileFilter *current_filter)
{
    g_autoptr (FileChooserData) data = (FileChooserData *) user_data;
    g_auto (GVariantBuilder) results = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);
    g_auto (GVariantBuilder) uris = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_STRING_ARRAY);
    const char *method_name = g_dbus_method_invocation_get_method_name (data->invocation);
    gboolean writable = FALSE;
    GVariant *choices = get_choices (data->choices_action_group, &writable);

    if (strcmp (method_name, "SaveFiles") == 0 && data->filenames_to_save != NULL)
    {
        GFile *directory = G_FILE (locations->data);
        g_autolist (GFile) unique_locations = NULL;

        for (gsize i = 0; data->filenames_to_save[i] != NULL; i++)
        {
            g_autoptr (GFile) location = NULL;

            location = nautilus_generate_unique_file_in_directory (directory,
                                                                   data->filenames_to_save[i],
                                                                   FALSE);
            unique_locations = g_list_prepend (unique_locations, g_steal_pointer (&location));
        }

        build_uris_variant (&uris, unique_locations);
    }
    else
    {
        build_uris_variant (&uris, locations);
    }

    /* TODO: add to recents */

    if (current_filter != NULL)
    {
        g_variant_builder_add (&results, "{sv}", "current_filter", gtk_file_filter_to_gvariant (current_filter));
    }

    g_variant_builder_add (&results, "{sv}", "uris", g_variant_builder_end (&uris));
    g_variant_builder_add (&results, "{sv}", "choices", choices);
    g_variant_builder_add (&results, "{sv}", "writable", g_variant_new_boolean (writable));

    complete_file_chooser (data, RESPONSE_SUCCESS, &results);

    return TRUE;
}

static gboolean
on_window_close_request (gpointer user_data)
{
    g_autoptr (FileChooserData) data = (FileChooserData *) user_data;
    g_auto (GVariantBuilder) results = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);

    complete_file_chooser (data, RESPONSE_USER_CANCELLED, &results);

    return TRUE;
}

static gboolean
handle_close (XdpImplRequest        *request,
              GDBusMethodInvocation *invocation,
              gpointer               user_data)
{
    g_autoptr (FileChooserData) data = (FileChooserData *) user_data;
    g_auto (GVariantBuilder) results = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);

    complete_file_chooser (data, RESPONSE_OTHER, &results);

    xdp_impl_request_complete_close (request, invocation);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static GActionGroup *
create_choices_action_group (void)
{
    GSimpleActionGroup *action_group = g_simple_action_group_new ();
    g_autoptr (GSimpleAction) action = NULL;

    action = g_simple_action_new_stateful ("x-nautilus-is-read-only",
                                           NULL,
                                           g_variant_new_boolean (FALSE));

    g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
    g_simple_action_set_enabled (action, TRUE);

    return G_ACTION_GROUP (action_group);
}

static GMenu *
get_menu_from_choices (GVariant     *arg_options,
                       GActionGroup *action_group)
{
    GMenu *menu = g_menu_new ();
    g_autoptr (GVariantIter) iter = NULL;
    GVariant *choices_list;
    const char *choice_id;
    const char *label;
    const char *selected;
    char *option_id;
    char *option_label;

    if (!g_variant_lookup (arg_options, "choices", "a(ssa(ss)s)", &iter))
    {
        return menu;
    }

    while (g_variant_iter_next (iter, "(&s&s@a(ss)&s)", &choice_id, &label, &choices_list, &selected))
    {
        g_autoptr (GSimpleAction) action = NULL;

        if (g_variant_n_children (choices_list) > 0)
        {
            g_autoptr (GMenu) submenu = g_menu_new ();
            g_autoptr (GMenuItem) item = g_menu_item_new_submenu (label, G_MENU_MODEL (submenu));
            g_menu_append_item (menu, item);

            action = g_simple_action_new_stateful (choice_id,
                                                   G_VARIANT_TYPE_STRING,
                                                   g_variant_new_string (""));

            g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
            g_simple_action_set_enabled (action, TRUE);

            GVariantIter list_iter;
            g_variant_iter_init (&list_iter, choices_list);

            while (g_variant_iter_next (&list_iter, "(ss)", &option_id, &option_label))
            {
                g_autofree char *detailed_action_name = g_strdup_printf ("choices.%s::%s",
                                                                         choice_id,
                                                                         option_id);
                g_autoptr (GMenuItem) option_item = g_menu_item_new (option_label, detailed_action_name);

                g_menu_append_item (submenu, option_item);

                if (g_strcmp0 (option_id, selected) == 0)
                {
                    g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (option_id));
                }

                g_free (option_id);
                g_free (option_label);
            }
        }
        else
        {
            g_autofree char *action_name = g_strconcat ("choices.", choice_id, NULL);
            GVariant *state = g_variant_new_boolean (FALSE);

            if (g_strcmp0 (selected, "true") == 0)
            {
                state = g_variant_new_boolean (TRUE);
            }

            action = g_simple_action_new_stateful (choice_id, NULL, state);
            g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
            g_simple_action_set_enabled (action, TRUE);

            g_menu_append (menu, label, action_name);
        }

        g_variant_unref (choices_list);
    }

    return menu;
}

static gboolean
handle_file_chooser_methods (XdpImplFileChooser    *object,
                             GDBusMethodInvocation *invocation,
                             const char            *arg_handle,
                             const char            *arg_app_id,
                             const char            *arg_parent_window,
                             const char            *arg_title,
                             GVariant              *arg_options,
                             gpointer               user_data)
{
    NautilusPortal *self = NAUTILUS_PORTAL (user_data);
    const char *method_name = g_dbus_method_invocation_get_method_name (invocation);
    FileChooserData *data = g_new0 (FileChooserData, 1);
    data->self = self;
    data->invocation = invocation;

    g_application_hold (g_application_get_default ());

    /* Decide mode */
    NautilusMode mode = NAUTILUS_MODE_BROWSE;
    gboolean open_multiple = FALSE;
    gboolean open_directory = FALSE;

    if (strcmp (method_name, "OpenFile") == 0)
    {
        (void) g_variant_lookup (arg_options, "multiple", "b", &open_multiple);
        (void) g_variant_lookup (arg_options, "directory", "b", &open_directory);

        mode = (open_directory ? (open_multiple ? NAUTILUS_MODE_OPEN_FOLDERS
                                                : NAUTILUS_MODE_OPEN_FOLDER)
                               : (open_multiple ? NAUTILUS_MODE_OPEN_FILES
                                                : NAUTILUS_MODE_OPEN_FILE));
    }
    else if (strcmp (method_name, "SaveFile") == 0)
    {
        mode = NAUTILUS_MODE_SAVE_FILE;
    }
    else if (strcmp (method_name, "SaveFiles") == 0)
    {
        mode = NAUTILUS_MODE_SAVE_FILES;
    }
    else
    {
        g_return_val_if_reached (G_DBUS_METHOD_INVOCATION_UNHANDLED);
    }
    g_assert (mode != NAUTILUS_MODE_BROWSE);

    /* Define label */
    const char *accept_label;

    if (!g_variant_lookup (arg_options, "accept_label", "&s", &accept_label))
    {
        if (strcmp (method_name, "OpenFile") == 0)
        {
            accept_label = open_multiple ? _("_Open") : _("_Select");
        }
        else
        {
            accept_label = _("_Save");
        }
    }

    /* Define starting location (and name, for SAVE_FILE mode)*/
    const char *path;
    g_autofree char *suggested_filename = NULL;
    g_autoptr (GFile) starting_location = NULL;

    if (mode == NAUTILUS_MODE_SAVE_FILE)
    {
        if (g_variant_lookup (arg_options, "current_file", "^&ay", &path))
        {
            g_autoptr (GFile) file = g_file_new_for_path (path);

            suggested_filename = g_file_get_basename (file);
            starting_location = g_file_get_parent (file);
        }
        else
        {
            (void) g_variant_lookup (arg_options, "current_name", "s", &suggested_filename);
        }
    }
    else if (mode == NAUTILUS_MODE_SAVE_FILES)
    {
        (void) g_variant_lookup (arg_options, "files", "^aay", &data->filenames_to_save);
    }

    if (starting_location == NULL)
    {
        if (g_variant_lookup (arg_options, "current_folder", "^&ay", &path) &&
            path[0] != '\0')
        {
            starting_location = g_file_new_for_path (path);
        }
    }

    /* Define filters */
    g_autoptr (GVariant) current_filter = NULL;
    g_autoptr (GVariantIter) filters_iter = NULL;
    g_autoptr (GListStore) filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    guint current_filter_position = GTK_INVALID_LIST_POSITION;

    (void) g_variant_lookup (arg_options, "current_filter", "@(sa(us))", &current_filter);
    if (g_variant_lookup (arg_options, "filters", "a(sa(us))", &filters_iter))
    {
        GVariant *variant;
        guint position = 0;

        while (g_variant_iter_next (filters_iter, "@(sa(us))", &variant))
        {
            g_autoptr (GtkFileFilter) filter = gtk_file_filter_new_from_gvariant (variant);

            g_list_store_append (filters, filter);

            if (current_filter != NULL && g_variant_equal (variant, current_filter))
            {
                current_filter_position = position;
            }

            g_variant_unref (variant);
            position++;
        }
    }

    if (current_filter != NULL &&
        g_list_model_get_n_items (G_LIST_MODEL (filters)) == 0)
    {
        g_autoptr (GtkFileFilter) filter = gtk_file_filter_new_from_gvariant (current_filter);

        /* We are setting a single, unchangeable filter. */
        g_list_store_append (filters, filter);
        current_filter_position = 0;
    }

    /* Get choices */
    data->choices_action_group = create_choices_action_group ();
    g_autoptr (GMenu) menu = get_menu_from_choices (arg_options, data->choices_action_group);

    if (g_strcmp0 (method_name, "OpenFile") == 0)
    {
        g_menu_prepend (menu, _("Open Read-Only"), "choices.x-nautilus-is-read-only");
    }

    /* Prepare window */
    NautilusFileChooser *window = nautilus_file_chooser_new (mode);

    g_set_object (&data->window, GTK_WINDOW (window));

    nautilus_file_chooser_set_accept_label (window, accept_label);
    nautilus_file_chooser_set_filters (window, G_LIST_MODEL (filters));
    nautilus_file_chooser_set_current_filter (window, current_filter_position);
    nautilus_file_chooser_set_starting_location (window, starting_location);
    nautilus_file_chooser_set_suggested_name (window, suggested_filename);
    nautilus_file_chooser_add_choices (window, data->choices_action_group, G_MENU_MODEL (menu));

    const char *title;
    title = (arg_title[0] == '\0') ? _("Files") : arg_title;
    gtk_window_set_title (GTK_WINDOW (window), title);

    g_signal_connect_swapped (window, "close-request",
                              G_CALLBACK (on_window_close_request), data);
    g_signal_connect_swapped (window, "accepted",
                              G_CALLBACK (on_file_chooser_accepted), data);

    /* Show window */
    if (arg_parent_window != NULL)
    {
        data->external_parent = create_external_window_from_handle (arg_parent_window);
        if (data->external_parent == NULL)
        {
            g_warning ("Failed to associate portal window with parent window %s",
                       arg_parent_window);
        }
        else
        {
            gtk_widget_realize (GTK_WIDGET (window));

            GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
            gboolean modal = TRUE;

            (void) g_variant_lookup (arg_options, "modal", "b", &modal);

            external_window_set_parent_of (data->external_parent, surface);
            gtk_window_set_modal (data->window, modal);
        }
    }

    gtk_window_present (data->window);

    /* Setup request. */
    data->request = request_new (g_dbus_method_invocation_get_sender (invocation),
                                 arg_app_id,
                                 arg_handle);
    g_signal_connect (data->request, "handle-close", G_CALLBACK (handle_close), data);
    request_export (data->request, g_dbus_method_invocation_get_connection (invocation));

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
nautilus_portal_dispose (GObject *object)
{
    NautilusPortal *self = (NautilusPortal *) object;

    if (self->impl_file_chooser_skeleton != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->impl_file_chooser_skeleton, self);
        g_clear_object (&self->impl_file_chooser_skeleton);
    }

    G_OBJECT_CLASS (nautilus_portal_parent_class)->dispose (object);
}

static void
nautilus_portal_class_init (NautilusPortalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_portal_dispose;
}

static void
nautilus_portal_init (NautilusPortal *self)
{
    self->impl_file_chooser_skeleton = xdp_impl_file_chooser_skeleton_new ();
}

NautilusPortal *
nautilus_portal_new (void)
{
    return g_object_new (NAUTILUS_TYPE_PORTAL, NULL);
}

gboolean
nautilus_portal_register (NautilusPortal   *self,
                          GDBusConnection  *connection,
                          GError          **error)
{
    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->impl_file_chooser_skeleton),
                                           connection,
                                           DESKTOP_PORTAL_OBJECT_PATH,
                                           error))
    {
        return FALSE;
    }

    g_signal_connect (self->impl_file_chooser_skeleton, "handle-open-file",
                      G_CALLBACK (handle_file_chooser_methods), self);
    g_signal_connect (self->impl_file_chooser_skeleton, "handle-save-file",
                      G_CALLBACK (handle_file_chooser_methods), self);
    g_signal_connect (self->impl_file_chooser_skeleton, "handle-save-files",
                      G_CALLBACK (handle_file_chooser_methods), self);

    return TRUE;
}

void
nautilus_portal_unregister (NautilusPortal *self)
{
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->impl_file_chooser_skeleton));
}
