/*
 * Copyright (C) 2000, 2001 Eazel Inc.
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 * Copyright (C) 2005  Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#define GST_USE_UNSTABLE_API 1
#include <gst/gst.h>

#include "totem-properties-view.h"
#include "totem-gst-helpers.h"
#include <nautilus-extension.h>

#define WANT_MIME_TYPES 1
#include "totem-mime-types.h"

static GType tpp_type = 0;
static void properties_model_provider_iface_init (NautilusPropertiesModelProviderInterface *iface);
static GList *totem_properties_get_models (NautilusPropertiesModelProvider *provider,
                                           GList                           *files);

static void
totem_properties_plugin_register_type (GTypeModule *module)
{
    const GTypeInfo info =
    {
        .class_size = sizeof (GObjectClass),
        .base_init = (GBaseInitFunc) NULL,
        .base_finalize = (GBaseFinalizeFunc) NULL,
        .class_init = (GClassInitFunc) NULL,
        .class_finalize = NULL,
        .class_data = NULL,
        .instance_size = sizeof (GObject),
        .n_preallocs = 0,
        .instance_init = (GInstanceInitFunc) NULL,
        .value_table = NULL
    };
    const GInterfaceInfo properties_model_provider_iface_info =
    {
        (GInterfaceInitFunc) properties_model_provider_iface_init,
        NULL,
        NULL
    };

    tpp_type = g_type_module_register_type (module, G_TYPE_OBJECT,
                                            "TotemPropertiesPlugin",
                                            &info, 0);
    g_type_module_add_interface (module,
                                 tpp_type,
                                 NAUTILUS_TYPE_PROPERTIES_MODEL_PROVIDER,
                                 &properties_model_provider_iface_info);
}

static void
properties_model_provider_iface_init (NautilusPropertiesModelProviderInterface *iface)
{
    iface->get_models = totem_properties_get_models;
}

static gpointer
init_backend (gpointer data)
{
    gst_init (NULL, NULL);
    totem_gst_disable_display_decoders ();
    return NULL;
}

static GList *
totem_properties_get_models (NautilusPropertiesModelProvider *provider,
                             GList                           *files)
{
    static GOnce backend_inited = G_ONCE_INIT;
    NautilusFileInfo *file;
    char *uri;
    NautilusPropertiesModel *model;
    guint i;
    gboolean found;

    /* only add properties model if a single file is selected */
    if (files == NULL || files->next != NULL)
    {
        return NULL;
    }
    file = files->data;

    /* only add the properties model to these mime types */
    found = FALSE;
    for (i = 0; mime_types[i] != NULL; i++)
    {
        if (nautilus_file_info_is_mime_type (file, mime_types[i]))
        {
            found = TRUE;
            break;
        }
    }
    if (found == FALSE)
    {
        return NULL;
    }

    /* okay, make the model, init'ing the backend first if necessary */
    g_once (&backend_inited, init_backend, NULL);

    uri = nautilus_file_info_get_uri (file);
    model = totem_properties_view_new (uri);
    g_free (uri);

    return g_list_prepend (NULL, model);
}

/* --- extension interface --- */
void
nautilus_module_initialize (GTypeModule *module)
{
    /* set up translation catalog */
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    totem_properties_plugin_register_type (module);
}

void
nautilus_module_shutdown (void)
{
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
    static GType type_list[1];

    type_list[0] = tpp_type;
    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
