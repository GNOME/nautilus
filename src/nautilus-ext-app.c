#include <config.h>

#include <gio/gio.h>

#include "nautilus-ext-man-generated.h"
#include "nautilus-extension.h"
#include "nautilus-module.h"

struct _NautilusExtApp
{
    GApplication parent;

    GHashTable *extensions;

    NautilusExtensionManager *manager;

    guint export_id;
};

#define NAUTILUS_TYPE_EXT_APP nautilus_ext_app_get_type()
G_DECLARE_FINAL_TYPE (NautilusExtApp, nautilus_ext_app, NAUTILUS, EXT_APP, GApplication)

G_DEFINE_FINAL_TYPE (NautilusExtApp, nautilus_ext_app, G_TYPE_APPLICATION)

typedef struct
{
    NautilusExtensionManager *manager;
    GDBusMethodInvocation    *invocation;
    char *str;
} DBusData;

static DBusData *
dbus_data_new (NautilusExtensionManager *manager,
               GDBusMethodInvocation    *invocation)
{
    DBusData *data = g_new0 (DBusData, 1);

    data->manager = g_object_ref (manager);
    data->invocation = g_object_ref (invocation);

    return data;
}

static void
dbus_data_free (DBusData *data)
{
    g_clear_object (&data->manager);
    g_clear_object (&data->invocation);
    g_free (data);
}


static char *
get_extension_id (NautilusExtApp   *self,
                  NautilusMenuItem *item,
                  const char       *extension_prefix,
                  guint             idx)
{
    g_autofree char *name = NULL;
    g_autofree char *escaped_name = NULL;
    char *extension_id = NULL;

    g_object_get (G_OBJECT (item), "name", &name, NULL);
    escaped_name = g_uri_escape_string (name, NULL, TRUE);

    extension_id = g_strdup_printf ("extension_%s_%d_%s",
                                    extension_prefix, idx, escaped_name);
    g_hash_table_replace (self->extensions, g_strdup (extension_id), g_object_ref (item));

    return extension_id;
}

static GVariant *
menu_items_to_variant (NautilusExtApp *self,
                       GList          *list,
                       const char     *extension_prefix)
{
    guint idx = 0;
    GVariantBuilder builder;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    for (GList *l = list; l != NULL; l = l->next)
    {
        char *label;
        gboolean sensitive;
        NautilusMenuItem *item = l->data;
        NautilusMenu *menu;
        g_autofree char * extension_id = NULL;

        g_object_get (item,
                      "label", &label,
                      "sensitive", &sensitive,
                      "menu", &menu, NULL);

        extension_id = get_extension_id (self, item, extension_prefix, idx);

        if (menu == NULL)
        {
            g_variant_builder_add_parsed (&builder,
                                          "{'label': <%s>, 'sensitive': <%b>, 'id': <%s>}",
                                          label, sensitive, extension_id);
        }
        else
        {
            GVariant *variant_menu;
            g_autolist (GObject) menu_items = nautilus_menu_get_items (menu);

            variant_menu = menu_items_to_variant (self, menu_items, extension_prefix);
            g_variant_builder_add_parsed (&builder,
                                          "{'label': <%s>, 'sensitive': <%b>, 'id': <%s>, 'menu': %v}",
                                          label, sensitive, extension_id, variant_menu);
        }

        idx++;
    }

    return g_variant_builder_end (&builder);
}

static gboolean
background_menu (NautilusExtensionManager *manager,
                 GDBusMethodInvocation    *invocation,
                 const char               *directory_uri,
                 NautilusExtApp           *self)
{
    g_autolist (GObject) providers = NULL;
    g_autoptr (NautilusFileInfo) file = nautilus_file_info_new (directory_uri);
    g_autolist (GObject) all_items = NULL;
    GVariant *result;

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);

    for (GList *l = providers; l != NULL; l = l->next)
    {
        GList *items = nautilus_menu_provider_get_background_items (l->data, file);

        g_signal_connect_object (l->data, "items-updated",
                                 G_CALLBACK (nautilus_extension_manager_emit_menu_items_updated),
                                 self->manager, G_CONNECT_SWAPPED);
        all_items = g_list_concat (all_items, items);
    }

    result = menu_items_to_variant (self, all_items, "background");

    nautilus_extension_manager_complete_background_menu (manager, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
selection_menu (NautilusExtensionManager *manager,
                GDBusMethodInvocation    *invocation,
                const char              **file_uris,
                NautilusExtApp           *self)
{
    g_autolist (GObject) providers = NULL;
    g_autolist (NautilusFileInfo) files = NULL;
    g_autolist (GObject) all_items = NULL;
    GVariant *result;

    for (guint i = 0; file_uris[i] != NULL; i++)
    {
        files = g_list_prepend (files, nautilus_file_info_new (file_uris[i]));
    }

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);

    for (GList *l = providers; l != NULL; l = l->next)
    {
        GList *items = nautilus_menu_provider_get_file_items (l->data, files);

        g_signal_connect_object (l->data, "items-updated",
                                 G_CALLBACK (nautilus_extension_manager_emit_menu_items_updated),
                                 self->manager, G_CONNECT_SWAPPED);
        all_items = g_list_concat (all_items, items);
    }

    result = menu_items_to_variant (self, all_items, "selection");

    nautilus_extension_manager_complete_selection_menu (manager, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
menu_item_activate (NautilusExtensionManager *manager,
                    GDBusMethodInvocation    *invocation,
                    const char               *action_name,
                    NautilusExtApp           *self)
{
    NautilusMenuItem *item;

    item = g_hash_table_lookup (self->extensions, action_name);
    nautilus_menu_item_activate (item);

    nautilus_extension_manager_complete_menu_item_activate (manager, invocation);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static GVariant *
get_properties_items_variant (GListModel *items)
{
    GVariantBuilder builder;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

    for (guint i = 0; i < g_list_model_get_n_items (items); i++)
    {
        g_autoptr (NautilusPropertiesItem) item = g_list_model_get_item (items, i);
        const char *name = nautilus_properties_item_get_name (item);
        const char *value = nautilus_properties_item_get_value (item);

        g_variant_builder_add (&builder, "(ss)", g_strdup (name), g_strdup (value));
    }

    return g_variant_builder_end (&builder);
}

static void
properties_items_changed (GListModel *self,
                          guint       position,
                          guint       removed,
                          guint       added,
                          gpointer    user_data)
{
    NautilusExtensionManager *manager = user_data;
    GVariant *item_variant = get_properties_items_variant (self);
    const char *timestamp = g_object_get_data (G_OBJECT (self), "timestamp");

    nautilus_extension_manager_emit_properties_changed (manager, timestamp, item_variant);
}

static gboolean
properties (NautilusExtensionManager *manager,
            GDBusMethodInvocation    *invocation,
            const char * const       *uris,
            NautilusExtApp           *self)
{
    GVariantBuilder builder;
    g_autolist (GObject) providers = NULL;
    g_autolist (NautilusPropertiesModel) all_models = NULL;
    g_autolist (GObject) files = NULL;

    for (guint i = 0; uris[i] != NULL; i++)
    {
        files = g_list_prepend (files, nautilus_file_info_new (uris[i]));
    }

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_PROPERTIES_MODEL_PROVIDER);
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    for (GList *l = providers; l != NULL; l = l->next)
    {
        GList *models = nautilus_properties_model_provider_get_models (l->data, files);

        all_models = g_list_concat (all_models, models);
    }

    for (GList *l = all_models; l != NULL; l = l->next)
    {
        const char *title = nautilus_properties_model_get_title (l->data);
        GListModel *item_model = nautilus_properties_model_get_model (l->data);
        GVariant *item_variant = get_properties_items_variant (item_model);
        char *timestamp = g_strdup_printf ("%ld", g_get_monotonic_time ());

        g_hash_table_insert (self->extensions, g_strdup (timestamp), g_object_ref (l->data));
        g_signal_connect (item_model, "items-changed", G_CALLBACK (properties_items_changed), manager);
        g_object_set_data_full (G_OBJECT (item_model), "timestamp", timestamp, g_free);

        g_variant_builder_add_parsed (&builder, "{'title': <%s>, 'items': %v, 'timestamp': <%s>}",
                                      title, item_variant, timestamp);
    }

    nautilus_extension_manager_complete_properties (manager, invocation, g_variant_builder_end (&builder));

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
properties_close (NautilusExtensionManager *manager,
                  GDBusMethodInvocation    *invocation,
                  const char               *timestamp,
                  NautilusExtApp           *self)
{
    g_hash_table_remove (self->extensions, timestamp);

    nautilus_extension_manager_complete_properties_close (manager, invocation);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
info_provider_callback (NautilusInfoProvider    *provider,
                        NautilusOperationHandle *handle,
                        NautilusOperationResult  result,
                        gpointer                 user_data)
{
    DBusData *data = user_data;

    nautilus_extension_manager_complete_info_update (data->manager, data->invocation);
}

static gboolean
info_update (NautilusExtensionManager *manager,
             GDBusMethodInvocation    *invocation,
             const char               *uri,
             NautilusExtApp           *self)
{

    g_autolist (GObject) providers = NULL;
    g_autoptr (NautilusFileInfo) file = nautilus_file_info_new (uri);
    gboolean all_results_complete = TRUE;

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_INFO_PROVIDER);

    for (GList *l = providers; l != NULL; l = l->next)
    {
        g_autoptr (GClosure) update_complete = NULL;
        NautilusOperationHandle *handle;
        NautilusOperationResult result;
        DBusData *data = dbus_data_new (manager, invocation);

        update_complete = g_cclosure_new (G_CALLBACK (info_provider_callback),
                                          data,
                                          (GClosureNotify) dbus_data_free);
        g_closure_set_marshal (update_complete,
                               g_cclosure_marshal_generic);

        result = nautilus_info_provider_update_file_info
                     (l->data,
                     file,
                     update_complete,
                     &handle);

        all_results_complete &= (result == NAUTILUS_OPERATION_COMPLETE);
    }

    if (all_results_complete)
    {
        nautilus_extension_manager_complete_info_update (manager, invocation);
    }

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

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
    g_signal_connect (app->manager, "handle-background-menu", G_CALLBACK (background_menu), app);
    g_signal_connect (app->manager, "handle-selection-menu", G_CALLBACK (selection_menu), app);
    g_signal_connect (app->manager, "handle-menu-item-activate", G_CALLBACK (menu_item_activate), app);
    g_signal_connect (app->manager, "handle-properties", G_CALLBACK (properties), app);
    g_signal_connect (app->manager, "handle-properties-close", G_CALLBACK (properties_close), app);
    g_signal_connect (app->manager, "handle-info-update", G_CALLBACK (info_update), app);

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
nautilus_ext_app_dispose (GObject *object)
{
    NautilusExtApp *self = NAUTILUS_EXT_APP (object);

    g_clear_pointer (&self->extensions, g_hash_table_destroy);
}

static void
nautilus_ext_app_class_init (NautilusExtAppClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    app_class->dbus_register = dbus_register;
    app_class->startup = startup;

    object_class->dispose = nautilus_ext_app_dispose;
}

static void
nautilus_ext_app_init (NautilusExtApp *self)
{
    self->extensions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
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
