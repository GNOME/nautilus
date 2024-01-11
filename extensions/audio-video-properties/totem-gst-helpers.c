/*
 * Copyright (C) 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2005-2008 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright © 2009 Christian Persch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
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
