/* Nautilus
 *
 *  Copyright (C) 2008 Red Hat, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Author: David Zeuthen <davidz@redhat.com>
 */


#include <config.h>

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>
#include <gio/gio.h>

#include <glib/gi18n.h>

typedef struct
{
    GtkWidget *dialog;
    GMount *mount;
} AutorunSoftwareDialogData;

static void autorun_software_dialog_mount_unmounted (GMount                    *mount,
                                                     AutorunSoftwareDialogData *data);

static void
autorun_software_dialog_destroy (AutorunSoftwareDialogData *data)
{
    g_signal_handlers_disconnect_by_func (G_OBJECT (data->mount),
                                          G_CALLBACK (autorun_software_dialog_mount_unmounted),
                                          data);

    gtk_window_destroy (GTK_WINDOW (data->dialog));
    g_object_unref (data->mount);
    g_free (data);
}

static void
autorun_software_dialog_mount_unmounted (GMount                    *mount,
                                         AutorunSoftwareDialogData *data)
{
    autorun_software_dialog_destroy (data);
}

static gboolean
_check_file (GFile      *mount_root,
             const char *file_path,
             gboolean   *executable)
{
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFileInfo) file_info = NULL;

    file = g_file_get_child (mount_root, file_path);
    file_info = g_file_query_info (file,
                                   G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL,
                                   NULL);

    if (file_info == NULL)
    {
        return FALSE;
    }

    if (executable != NULL)
    {
        *executable = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
    }

    return TRUE;
}

static void
autorun (GMount *mount)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) program_to_spawn = NULL;
    g_autoptr (GFile) program_parameter_file = NULL;
    g_autofree char *error_string = NULL;
    g_autofree char *path_to_spawn = NULL;
    g_autofree char *cwd_for_program = NULL;
    g_autofree char *program_parameter = NULL;
    gboolean executable = TRUE;

    root = g_mount_get_root (mount);

    /* Careful here, according to
     *
     *  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html
     *
     * the ordering does matter.
     */

    if (_check_file (root, ".autorun", &executable) && executable)
    {
        program_to_spawn = g_file_get_child (root, ".autorun");
    }
    else if (_check_file (root, "autorun", &executable) && executable)
    {
        program_to_spawn = g_file_get_child (root, "autorun");
    }
    else if (_check_file (root, "autorun.sh", NULL))
    {
        program_to_spawn = g_file_new_for_path ("/bin/sh");
        program_parameter_file = g_file_get_child (root, "autorun.sh");
    }

    if (program_to_spawn != NULL)
    {
        path_to_spawn = g_file_get_path (program_to_spawn);
    }
    if (program_parameter_file != NULL)
    {
        program_parameter = g_file_get_path (program_parameter_file);
    }

    cwd_for_program = g_file_get_path (root);

    if (path_to_spawn != NULL && cwd_for_program != NULL)
    {
        if (chdir (cwd_for_program) == 0)
        {
            execl (path_to_spawn, path_to_spawn, program_parameter, NULL);
            error_string = g_strdup_printf (_("Unable to start the program:\n%s"), strerror (errno));
            goto out;
        }
        error_string = g_strdup_printf (_("Unable to start the program:\n%s"), strerror (errno));
        goto out;
    }
    else if (!executable)
    {
        error_string = g_strdup (_("The program is not executable."));
        goto out;
    }
    error_string = g_strdup_printf (_("Unable to locate the program"));

out:
    if (error_string != NULL)
    {
        GtkWidget *dialog;
        dialog = adw_message_dialog_new (NULL,
                                         _("Oops! There was a problem running this software."),
                                         error_string);
        adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "ok", _("_OK"));
        adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "ok");

        gtk_window_present (GTK_WINDOW (dialog));
    }
}

static void
autorun_software_dialog_response (GtkDialog *dialog,
                                  gchar     *response,
                                  GMount    *mount)
{
    if (g_strcmp0 (response, "run") == 0)
    {
        autorun (mount);
    }
}

static void
present_autorun_for_software_dialog (GMount *mount)
{
    GIcon *icon;
    g_autofree char *mount_name = NULL;
    GtkWidget *dialog;
    AutorunSoftwareDialogData *data;

    mount_name = g_mount_get_name (mount);

    dialog = adw_message_dialog_new (NULL, NULL, _("If you don’t trust this location or aren’t sure, press Cancel."));
    adw_message_dialog_format_heading (ADW_MESSAGE_DIALOG (dialog),
                                       _("“%s” contains software intended to be automatically started. Would you like to run it?"),
                                       mount_name);

    /* TODO: in a star trek future add support for verifying
     * software on media (e.g. if it has a certificate, check it
     * etc.)
     */


    icon = g_mount_get_icon (mount);
    if (G_IS_THEMED_ICON (icon))
    {
        const gchar * const *names;

        names = g_themed_icon_get_names (G_THEMED_ICON (icon));

        if (names != NULL)
        {
            gtk_window_set_icon_name (GTK_WINDOW (dialog), names[0]);
        }
    }

    data = g_new0 (AutorunSoftwareDialogData, 1);
    data->dialog = dialog;
    data->mount = g_object_ref (mount);

    g_signal_connect (G_OBJECT (mount),
                      "unmounted",
                      G_CALLBACK (autorun_software_dialog_mount_unmounted),
                      data);

    adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                      "cancel", _("_Cancel"),
                                      "run", _("_Run"),
                                      NULL);
    adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "cancel");

    g_signal_connect (dialog,
                      "response",
                      G_CALLBACK (autorun_software_dialog_response),
                      mount);

    gtk_window_present (GTK_WINDOW (dialog));
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GVolumeMonitor) monitor = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GMount) mount = NULL;
    g_autoptr (GError) error = NULL;

    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    gtk_init ();
    adw_init ();

    if (argc != 2)
    {
        g_print ("Usage: %s mount-uri\n", argv[0]);
        goto out;
    }

    /* instantiate monitor so we get the "unmounted" signal properly */
    monitor = g_volume_monitor_get ();
    if (monitor == NULL)
    {
        g_warning ("Unable to connect to the volume monitor");
        goto out;
    }

    file = g_file_new_for_commandline_arg (argv[1]);
    if (file == NULL)
    {
        g_warning ("Unable to parse mount URI");
        goto out;
    }

    mount = g_file_find_enclosing_mount (file, NULL, &error);
    if (mount == NULL)
    {
        g_warning ("Unable to find device for URI: %s", error->message);
        goto out;
    }

    present_autorun_for_software_dialog (mount);

    while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    {
        g_main_context_iteration (NULL, TRUE);
    }

out:
    return 0;
}
