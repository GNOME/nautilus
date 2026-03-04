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
#include <gst/gstprotection.h>

#include "audio-video-properties-model.h"
#include <nautilus-extension.h>

static GType tpp_type = 0;
static void properties_model_provider_iface_init (NautilusPropertiesModelProviderInterface *iface);

static void
audio_video_properties_plugin_register_type (GTypeModule *module)
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
                                            "AVPropertiesPlugin",
                                            &info, 0);
    g_type_module_add_interface (module,
                                 tpp_type,
                                 NAUTILUS_TYPE_PROPERTIES_MODEL_PROVIDER,
                                 &properties_model_provider_iface_info);
}

/* Disable decoders that require a display environment to work,
 * and that might cause crashes */
static void
disable_gst_display_decoders (void)
{
    const char *disabled_plugins[] =
    {
        "bmcdec",
        "vaapi",
        "video4linux2"
    };

    /* Disable the vaapi plugin as it will not work with the
     * fakesink we use:
     * See: https://bugzilla.gnome.org/show_bug.cgi?id=700186 and
     * https://bugzilla.gnome.org/show_bug.cgi?id=749605 */
    GstRegistry *registry = gst_registry_get ();

    for (guint i = 0; i < G_N_ELEMENTS (disabled_plugins); i++)
    {
        GstPlugin *plugin = gst_registry_find_plugin (registry,
                                                      disabled_plugins[i]);

        if (plugin != NULL)
        {
            gst_registry_remove_plugin (registry, plugin);
        }
    }
}

static gpointer
init_backend (gpointer data)
{
    gst_init (NULL, NULL);
    disable_gst_display_decoders ();
    return NULL;
}

static GList *
audio_video_properties_get_models (NautilusPropertiesModelProvider *provider,
                                   GList                           *files)
{
    /* only add properties model if a single file is selected */
    if (files == NULL || files->next != NULL)
    {
        return NULL;
    }

    NautilusFileInfo *file_info = files->data;
    g_autofree char *content_type = nautilus_file_info_get_mime_type (file_info);
    g_autofree char *generic_icon_name = g_content_type_get_generic_icon_name (content_type);

    /* This model is for Audio/Video files */
    if (!g_str_equal (generic_icon_name, "audio-x-generic") &&
        !g_str_equal (generic_icon_name, "video-x-generic"))
    {
        return NULL;
    }

    /* okay, make the model, init'ing the backend first if necessary */
    static GOnce backend_inited = G_ONCE_INIT;

    g_once (&backend_inited, init_backend, NULL);

    g_autofree char *uri = nautilus_file_info_get_uri (file_info);

    return g_list_prepend (NULL, audio_video_properties_model_new (uri));
}

static void
properties_model_provider_iface_init (NautilusPropertiesModelProviderInterface *iface)
{
    iface->get_models = audio_video_properties_get_models;
}

/* --- extension interface --- */
void
nautilus_module_initialize (GTypeModule *module)
{
    /* set up translation catalog */
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    audio_video_properties_plugin_register_type (module);
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
