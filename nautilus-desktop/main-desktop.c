#include <config.h>

#include "nautilus-desktop-application.h"
#include <src/nautilus-resources.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

int
main (int   argc,
      char *argv[])
{
    NautilusDesktopApplication *application;
    int retval;

    /* Initialize gettext support */
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    g_set_prgname ("nautilus-desktop");

#ifdef HAVE_EXEMPI
    xmp_init ();
#endif

    gdk_set_allowed_backends ("x11");

    nautilus_register_resource ();
    application = nautilus_desktop_application_new ();

    retval = g_application_run (G_APPLICATION (application),
                                argc, argv);

    g_object_unref (application);

    return retval;
}
