/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include "nautilus-image-properties-model-provider.h"

#include <glib/gi18n-lib.h>

#include <nautilus-extension.h>

void
nautilus_module_initialize (GTypeModule *module)
{
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    nautilus_image_properties_model_provider_load (module);
}

void
nautilus_module_shutdown (void)
{
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
    static GType type_list[1] = { 0 };

    g_assert (types != NULL);
    g_assert (num_types != NULL);

    type_list[0] = NAUTILUS_TYPE_IMAGE_PROPERTIES_MODEL_PROVIDER;

    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
