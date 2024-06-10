/*
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-network-address-bar.h"

#include <glib/gi18n.h>
#include <adwaita.h>

#include "nautilus-recent-servers.h"

struct _NautilusNetworkAddressBar
{
    GtkBox parent_instance;

    GtkWidget *address_entry;
    GtkWidget *connect_button;
    GtkWidget *available_protocols_grid;

    gboolean should_open_location;
    gboolean should_pulse_entry;
    gboolean connecting_to_server;
    guint entry_pulse_timeout_id;

    GCancellable *cancellable;
};

G_DEFINE_TYPE (NautilusNetworkAddressBar, nautilus_network_address_bar, GTK_TYPE_BOX)

const char *unsupported_protocols[] =
{
    "afc", "archive", "burn", "computer", "file", "http", "localtest", "obex", "recent", "trash", NULL
};


static void
show_error_message (NautilusNetworkAddressBar *self,
                    const gchar               *primary,
                    const gchar               *secondary)
{
    GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (self));
    GtkWidget *dialog = adw_message_dialog_new (GTK_WINDOW (window), primary, secondary);
    adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "close", _("_Close"));
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
server_mount_ready_cb (GObject      *source_file,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    g_autoptr (NautilusNetworkAddressBar) self = NAUTILUS_NETWORK_ADDRESS_BAR (user_data);
    gboolean should_show = TRUE;
    g_autoptr (GError) error = NULL;
    GFile *location = G_FILE (source_file);

    g_file_mount_enclosing_volume_finish (location, res, &error);
    if (error != NULL)
    {
        should_show = FALSE;

        if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
        {
            /*
             * Already mounted volume is not a critical error
             * and we can still continue with the operation.
             */
            should_show = TRUE;
        }
        else if (error->domain != G_IO_ERROR ||
                 (error->code != G_IO_ERROR_CANCELLED &&
                  error->code != G_IO_ERROR_FAILED_HANDLED))
        {
            /* if it wasn't cancelled show a dialog */
            show_error_message (self, _("Unable to access location"), error->message);
        }

        /* The operation got cancelled by the user or dispose() and or the error
         *  has been handled already. */
    }

    self->should_pulse_entry = FALSE;
    gtk_entry_set_progress_fraction (GTK_ENTRY (self->address_entry), 0);

    /* Restore from Cancel to Connect */
    gtk_button_set_label (GTK_BUTTON (self->connect_button), _("Con_nect"));
    gtk_widget_set_sensitive (self->address_entry, TRUE);
    self->connecting_to_server = FALSE;

    if (should_show)
    {
        nautilus_add_recent_server (location);

        /*
         * Only clear the entry if it successfully connects to the server.
         * Otherwise, the user would lost the typed address even if it fails
         * to connect.
         */
        gtk_editable_set_text (GTK_EDITABLE (self->address_entry), "");

        if (self->should_open_location)
        {
            g_autofree char *uri_to_open = NULL;
            g_autoptr (GMount) mount = g_file_find_enclosing_mount (location, self->cancellable, NULL);

            /*
             * If the mount is not found at this point, it is probably user-
             * invisible, which happens e.g for smb-browse, but the location
             * should be opened anyway...
             */
            if (mount != NULL)
            {
                g_autoptr (GFile) root = g_mount_get_default_location (mount);

                uri_to_open = g_file_get_uri (root);
            }
            else
            {
                uri_to_open = g_file_get_uri (location);
            }

            gtk_widget_activate_action (GTK_WIDGET (self),
                                        "slot.open-location", "s", uri_to_open);
        }
    }
}

static gboolean
pulse_entry_cb (gpointer user_data)
{
    NautilusNetworkAddressBar *self = NAUTILUS_NETWORK_ADDRESS_BAR (user_data);

    if (self->should_pulse_entry)
    {
        gtk_entry_progress_pulse (GTK_ENTRY (self->address_entry));

        return G_SOURCE_CONTINUE;
    }
    else
    {
        gtk_entry_set_progress_fraction (GTK_ENTRY (self->address_entry), 0);
        self->entry_pulse_timeout_id = 0;

        return G_SOURCE_REMOVE;
    }
}


static void
mount_server (NautilusNetworkAddressBar *self,
              GFile                     *location)
{
    GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));
    g_autoptr (GMountOperation) operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    /* User cliked when the operation was ongoing, so wanted to cancel it */
    if (self->connecting_to_server)
    {
        return;
    }

    self->cancellable = g_cancellable_new ();

    self->should_pulse_entry = TRUE;
    gtk_entry_set_progress_pulse_step (GTK_ENTRY (self->address_entry), 0.1);
    gtk_entry_set_progress_fraction (GTK_ENTRY (self->address_entry), 0.1);
    /* Allow to cancel the operation */
    gtk_button_set_label (GTK_BUTTON (self->connect_button), _("Cance_l"));
    gtk_widget_set_sensitive (self->address_entry, FALSE);
    self->connecting_to_server = TRUE;

    if (self->entry_pulse_timeout_id == 0)
    {
        self->entry_pulse_timeout_id = g_timeout_add (100, (GSourceFunc) pulse_entry_cb, self);
    }

    g_mount_operation_set_password_save (operation, G_PASSWORD_SAVE_FOR_SESSION);

    /* make sure we keep the view around for as long as we are running */
    g_file_mount_enclosing_volume (location,
                                   0,
                                   operation,
                                   self->cancellable,
                                   server_mount_ready_cb,
                                   g_object_ref (self));
}

static void
on_connect_button_clicked (NautilusNetworkAddressBar *self)
{
    /* Since the 'Connect' button is updated whenever the typed
     * address changes, it is sufficient to check if it's sensitive
     * or not, in order to determine if the given address is valid.
     */
    if (!gtk_widget_get_sensitive (self->connect_button))
    {
        return;
    }

    const char *uri = gtk_editable_get_text (GTK_EDITABLE (self->address_entry));

    if (uri != NULL && uri[0] != '\0')
    {
        g_autoptr (GFile) file = g_file_new_for_commandline_arg (uri);

        self->should_open_location = TRUE;

        mount_server (self, file);
    }
    else
    {
        show_error_message (self, _("Unable to get remote server location"), NULL);
    }
}

static void
on_entry_icon_press (GtkEntry             *entry,
                     GtkEntryIconPosition  icon_pos,
                     gpointer              user_data)
{
    g_return_if_fail (icon_pos == GTK_ENTRY_ICON_SECONDARY);

    gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
on_address_entry_text_changed (NautilusNetworkAddressBar *self)
{
    const char * const *supported_protocols = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
    const char *address = gtk_editable_get_text (GTK_EDITABLE (self->address_entry));
    g_autofree char *scheme = g_uri_parse_scheme (address);
    gboolean is_empty = (address == NULL || *address == '\0');
    gboolean supported = FALSE;

    if (supported_protocols != NULL && scheme != NULL)
    {
        supported = g_strv_contains (supported_protocols, scheme) &&
                    !g_strv_contains (unsupported_protocols, scheme);
    }

    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->address_entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       is_empty ? NULL : "edit-clear-symbolic");

    gtk_widget_set_sensitive (self->connect_button, supported);
    if (scheme != NULL && !supported)
    {
        gtk_widget_add_css_class (self->address_entry, "error");
    }
    else
    {
        gtk_widget_remove_css_class (self->address_entry, "error");
    }
}

static void
nautilus_network_address_bar_map (GtkWidget *widget)
{
    NautilusNetworkAddressBar *self = NAUTILUS_NETWORK_ADDRESS_BAR (widget);

    gtk_editable_set_text (GTK_EDITABLE (self->address_entry), "");

    GTK_WIDGET_CLASS (nautilus_network_address_bar_parent_class)->map (widget);
}

static void
attach_protocol_row_to_grid (GtkGrid    *grid,
                             const char *protocol_name,
                             const char *protocol_prefix)
{
    GtkWidget *name_label = gtk_label_new (protocol_name);
    GtkWidget *prefix_label = gtk_label_new (protocol_prefix);

    gtk_widget_set_halign (name_label, GTK_ALIGN_START);
    gtk_grid_attach_next_to (grid, name_label, NULL, GTK_POS_BOTTOM, 1, 1);

    gtk_widget_set_halign (prefix_label, GTK_ALIGN_START);
    gtk_grid_attach_next_to (grid, prefix_label, name_label, GTK_POS_RIGHT, 1, 1);
}

static void
populate_available_protocols_grid (GtkGrid *grid)
{
    const char * const *supported_protocols = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
    gboolean has_any = FALSE;

    if (g_strv_contains (supported_protocols, "afp"))
    {
        attach_protocol_row_to_grid (grid, _("AppleTalk"), "afp://");
        has_any = TRUE;
    }

    if (g_strv_contains (supported_protocols, "ftp"))
    {
        attach_protocol_row_to_grid (grid, _("File Transfer Protocol"),
                                     /* Translators: do not translate ftp:// and ftps:// */
                                     _("ftp:// or ftps://"));
        has_any = TRUE;
    }

    if (g_strv_contains (supported_protocols, "nfs"))
    {
        attach_protocol_row_to_grid (grid, _("Network File System"), "nfs://");
        has_any = TRUE;
    }

    if (g_strv_contains (supported_protocols, "smb"))
    {
        attach_protocol_row_to_grid (grid, _("Samba"), "smb://");
        has_any = TRUE;
    }

    if (g_strv_contains (supported_protocols, "ssh"))
    {
        attach_protocol_row_to_grid (grid, _("SSH File Transfer Protocol"),
                                     /* Translators: do not translate sftp:// and ssh:// */
                                     _("sftp:// or ssh://"));
        has_any = TRUE;
    }

    if (g_strv_contains (supported_protocols, "dav"))
    {
        attach_protocol_row_to_grid (grid, _("WebDAV"),
                                     /* Translators: do not translate dav:// and davs:// */
                                     _("dav:// or davs://"));
        has_any = TRUE;
    }

    if (!has_any)
    {
        gtk_widget_set_visible (GTK_WIDGET (grid), FALSE);
    }
}

static void
nautilus_network_address_bar_dispose (GObject *object)
{
    NautilusNetworkAddressBar *self = NAUTILUS_NETWORK_ADDRESS_BAR (object);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_handle_id (&self->entry_pulse_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_network_address_bar_parent_class)->dispose (object);
}

static void
nautilus_network_address_bar_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_network_address_bar_parent_class)->finalize (object);
}

static void
nautilus_network_address_bar_init (NautilusNetworkAddressBar *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self->address_entry, "icon-press", G_CALLBACK (on_entry_icon_press), NULL);

    populate_available_protocols_grid (GTK_GRID (self->available_protocols_grid));
}

static void
nautilus_network_address_bar_class_init (NautilusNetworkAddressBarClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_network_address_bar_finalize;
    object_class->dispose = nautilus_network_address_bar_dispose;

    widget_class->map = nautilus_network_address_bar_map;

    /* Bind class to template */
    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-network-address-bar.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNetworkAddressBar, address_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusNetworkAddressBar, connect_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusNetworkAddressBar, available_protocols_grid);

    gtk_widget_class_bind_template_callback (widget_class, on_address_entry_text_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_connect_button_clicked);
}

NautilusNetworkAddressBar *
nautilus_network_address_bar_new (void)
{
    return g_object_new (NAUTILUS_TYPE_NETWORK_ADDRESS_BAR, NULL);
}
