/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-resources.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#include <locale.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main (int   argc,
      char *argv[])
{
    gint retval;
    NautilusApplication *application;
    /* Initialize gettext support */
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    g_set_prgname (APPLICATION_ID);

    if (getuid () == 0)
    {
        g_warning (_("Running as root is not supported. "
                     "Consider running `nautilus admin:///` instead."));

        exit (ENOTSUP);
    }

    const guint trim_threshold_mb = 128;
    mallopt (M_TRIM_THRESHOLD, trim_threshold_mb * 1024 * 1024);

    nautilus_register_resource ();
    /* Run the nautilus application. */
    application = nautilus_application_new ();

    /* hold indefinitely if we're asked to persist */
    if (g_getenv ("NAUTILUS_PERSIST") != NULL)
    {
        g_application_hold (G_APPLICATION (application));
    }

    retval = g_application_run (G_APPLICATION (application),
                                argc, argv);

    g_object_unref (application);

    return retval;
}
