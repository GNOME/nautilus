#include <config.h>

#include <gio/gio.h>

#include "nautilus-ext-man-generated.h"
#include "nautilus-extension.h"

struct _NautilusExtApp
{
    GApplication parent;
    NautilusExtensionManager *manager;

    guint export_id;
};

#define NAUTILUS_TYPE_EXT_APP nautilus_ext_app_get_type()
G_DECLARE_FINAL_TYPE (NautilusExtApp, nautilus_ext_app, NAUTILUS, EXT_APP, GApplication)

G_DEFINE_FINAL_TYPE (NautilusExtApp, nautilus_ext_app, G_TYPE_APPLICATION)


static gboolean
dbus_register (GApplication    *application,
               GDBusConnection *connection,
               const gchar     *object_path,
               GError         **error)
{
    NautilusExtApp *app = NAUTILUS_EXT_APP (application);

    app->manager = nautilus_extension_manager_skeleton_new ();
    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (app->manager), connection, object_path, NULL);

    return TRUE;
}

static void
nautilus_ext_app_class_init (NautilusExtAppClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

    app_class->dbus_register = dbus_register;

}

static void
nautilus_ext_app_init (NautilusExtApp *self)
{
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusExtApp) app = NULL;
    int ret;

    app = NAUTILUS_EXT_APP (g_object_new (NAUTILUS_TYPE_EXT_APP,
                                          "application-id", "org.gnome.Nautilus.ExtensionManager",
                                          NULL));

    ret = g_application_run (G_APPLICATION (app), argc, argv);

    return ret;
}
