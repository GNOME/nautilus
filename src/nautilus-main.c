/*
 * SPDX-FileCopyrightText: 1999, 2000 Red Hat, Inc.
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 */

/* nautilus-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-resources.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#include <locale.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
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
