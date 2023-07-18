#include <config.h>

#include <gio/gio.h>

#include "nautilus-ext-man-generated.h"
#include "nautilus-extension.h"
#include "nautilus-module.h"

struct _NautilusExtApp
{
    GApplication parent;
    NautilusExtensionManager *manager;

    guint export_id;
};

#define NAUTILUS_TYPE_EXT_APP nautilus_ext_app_get_type()
G_DECLARE_FINAL_TYPE (NautilusExtApp, nautilus_ext_app, NAUTILUS, EXT_APP, GApplication)

G_DEFINE_FINAL_TYPE (NautilusExtApp, nautilus_ext_app, G_TYPE_APPLICATION)


static void
get_columns (NautilusExtApp *self)
{
    GVariantBuilder builder;
    g_autolist (GObject) providers = NULL;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssss)"));
    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_COLUMN_PROVIDER);
    for (GList *l = providers; l != NULL; l = l->next)
    {
        g_autolist (GObject) columns = nautilus_column_provider_get_columns (l->data);
        for (GList *c = columns; c != NULL; c = c->next)
        {
            g_autofree char *name = NULL;
            g_autofree char *attribute = NULL;
            g_autofree char *label = NULL;
            g_autofree char *description = NULL;

            g_object_get (G_OBJECT (c->data),
                          "name", &name,
                          "attribute", &attribute,
                          "label", &label,
                          "description", &description, NULL);
            g_variant_builder_add (&builder, "(ssss)", name, attribute, label, description);
        }
    }
    nautilus_extension_manager_set_column_list (self->manager, g_variant_builder_end (&builder));
}

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
startup (GApplication *app)
{
    NautilusExtApp *self = NAUTILUS_EXT_APP (app);
    g_autofree char *extension_list = NULL;

    nautilus_module_setup ();
    extension_list = nautilus_module_get_installed_module_names ();
    nautilus_extension_manager_set_extension_list (self->manager, extension_list);

    get_columns (self);

    g_application_hold (app);

    G_APPLICATION_CLASS (nautilus_ext_app_parent_class)->startup (app);
}

static void
nautilus_ext_app_class_init (NautilusExtAppClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

    app_class->dbus_register = dbus_register;
    app_class->startup = startup;

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

    free_module_objects ();

    return ret;
}
