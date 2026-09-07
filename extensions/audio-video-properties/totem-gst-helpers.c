/*
 * SPDX-FileCopyrightText: 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * SPDX-FileCopyrightText: 2005-2008 Tim-Philipp Müller <tim centricular net>
 * SPDX-FileCopyrightText: 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * SPDX-FileCopyrightText: 2009 Christian Persch
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "totem-gst-helpers.h"
#include <gst/gstprotection.h>

/* Disable decoders that require a display environment to work,
 * and that might cause crashes */
void
totem_gst_disable_display_decoders (void)
{
    GstRegistry *registry;
    const char *blacklisted_plugins[] =
    {
        "bmcdec",
        "vaapi",
        "video4linux2"
    };
    guint i;

    /* Disable the vaapi plugin as it will not work with the
     * fakesink we use:
     * See: https://bugzilla.gnome.org/show_bug.cgi?id=700186 and
     * https://bugzilla.gnome.org/show_bug.cgi?id=749605 */
    registry = gst_registry_get ();

    for (i = 0; i < G_N_ELEMENTS (blacklisted_plugins); i++)
    {
        GstPlugin *plugin =
            gst_registry_find_plugin (registry,
                                      blacklisted_plugins[i]);
        if (plugin)
        {
            gst_registry_remove_plugin (registry, plugin);
        }
    }
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
