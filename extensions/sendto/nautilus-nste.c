/*
 *  Nautilus-sendto
 *
 *  Copyright (C) 2004 Free Software Foundation, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Roberto Majadas <roberto.majadas@openshine.com>
 *
 */

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <nautilus-extension.h>
#include "nautilus-nste.h"

struct _NautilusNste
{
    GObject parent_instance;

    gboolean nst_present;
};

static void menu_provider_iface_init (NautilusMenuProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (NautilusNste, nautilus_nste, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_MENU_PROVIDER,
                                                               menu_provider_iface_init))

static void
sendto_callback (NautilusMenuItem *item,
                 gpointer          user_data)
{
    GList *files;
    g_autoptr (GString) command = NULL;

    files = g_object_get_data (G_OBJECT (item), "files");
    command = g_string_new ("nautilus-sendto");

    for (GList *l = files; l != NULL; l = l->next)
    {
        g_autofree char *uri = NULL;

        uri = nautilus_file_info_get_uri (l->data);

        g_string_append_printf (command, " \"%s\"", uri);
    }

    g_spawn_command_line_async (command->str, NULL);
}

static gboolean
check_available_mailer (void)
{
    g_autoptr (GAppInfo) app_info = NULL;

    app_info = g_app_info_get_default_for_uri_scheme ("mailto");

    return app_info != NULL;
}

static GList *
get_file_items (NautilusMenuProvider *provider,
                GtkWidget            *window,
                GList                *files)
{
    GList *items = NULL;
    gboolean one_item;
    NautilusMenuItem *item;
    NautilusNste *nste;

    nste = NAUTILUS_NSTE (provider);
    if (!nste->nst_present)
    {
        return NULL;
    }

    if (files == NULL)
    {
        return NULL;
    }

    if (!check_available_mailer ())
    {
        return NULL;
    }

    one_item = (files != NULL) && (files->next == NULL);
    if (one_item &&
        !nautilus_file_info_is_directory ((NautilusFileInfo *) files->data))
    {
        item = nautilus_menu_item_new ("NautilusNste::sendto",
                                       _("Send to…"),
                                       _("Send file by mail…"),
                                       "document-send");
    }
    else
    {
        item = nautilus_menu_item_new ("NautilusNste::sendto",
                                       _("Send to…"),
                                       _("Send files by mail…"),
                                       "document-send");
    }
    g_signal_connect (item,
                      "activate",
                      G_CALLBACK (sendto_callback),
                      provider);
    g_object_set_data_full (G_OBJECT (item),
                            "files",
                            nautilus_file_info_list_copy (files),
                            (GDestroyNotify) nautilus_file_info_list_free);

    items = g_list_append (items, item);

    return items;
}

static void
menu_provider_iface_init (NautilusMenuProviderInterface *iface)
{
    iface->get_file_items = get_file_items;
}

static void
nautilus_nste_init (NautilusNste *nste)
{
    g_autofree char *path = NULL;

    path = g_find_program_in_path ("nautilus-sendto");
    nste->nst_present = (path != NULL);
}

static void
nautilus_nste_class_init (NautilusNsteClass *klass)
{
}

static void
nautilus_nste_class_finalize (NautilusNsteClass *klass)
{
}

void
nautilus_nste_load (GTypeModule *module)
{
    nautilus_nste_register_type (module);
}
