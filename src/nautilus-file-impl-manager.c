#include "config.h"

#include "nautilus-file-impl-manager.h"

#include "nautilus-file.h"
#include "nautilus-file-generated.h"

struct _NautilusFileImplManager {
    GObject parent;

    NautilusFileImpl *skeleton;
};

G_DEFINE_TYPE (NautilusFileImplManager, nautilus_file_impl_manager, G_TYPE_OBJECT);

static gboolean
handle_add_emblem (NautilusFileImpl      *object,
                   GDBusMethodInvocation *invocation,
                   const char            *uri,
                   const char            *emblem_name)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);

    nautilus_file_add_emblem (file, emblem_name);
    nautilus_file_impl_complete_add_emblem (object, invocation);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_add_string_attribute (NautilusFileImpl      *object,
                             GDBusMethodInvocation *invocation,
                             const char            *uri,
                             const char            *attribute_name,
                             const char            *value)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);

    nautilus_file_add_string_attribute (file, attribute_name, value);
    nautilus_file_impl_complete_add_string_attribute (object, invocation);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_can_write (NautilusFileImpl      *object,
                  GDBusMethodInvocation *invocation,
                  const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    gboolean result;

    result = nautilus_file_can_write (file);
    nautilus_file_impl_complete_can_write (object, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_activation_uri (NautilusFileImpl      *object,
                           GDBusMethodInvocation *invocation,
                           const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    char *activation_uri;

    activation_uri = nautilus_file_get_activation_uri (file);
    nautilus_file_impl_complete_get_activation_uri (object, invocation, activation_uri);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_file_type (NautilusFileImpl      *object,
                      GDBusMethodInvocation *invocation,
                      const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    GFileType file_type;

    file_type = nautilus_file_get_file_type (file);
    nautilus_file_impl_complete_get_file_type (object, invocation, file_type);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_mime_type (NautilusFileImpl      *object,
                      GDBusMethodInvocation *invocation,
                      const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    char *mime_type;

    mime_type = nautilus_file_get_mime_type (file);
    nautilus_file_impl_complete_get_mime_type (object, invocation, mime_type);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_name (NautilusFileImpl      *object,
                 GDBusMethodInvocation *invocation,
                 const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    char *name;

    name = nautilus_file_get_name (file);
    nautilus_file_impl_complete_get_name (object, invocation, name);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_parent_uri (NautilusFileImpl      *object,
                       GDBusMethodInvocation *invocation,
                       const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    char *parent_uri;

    parent_uri = nautilus_file_get_parent_uri (file);
    nautilus_file_impl_complete_get_parent_uri (object, invocation, parent_uri);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_string_attribute (NautilusFileImpl      *object,
                             GDBusMethodInvocation *invocation,
                             const char            *uri,
                             const char            *attribute_name)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    char *value;

    value = nautilus_file_get_string_attribute (file, attribute_name);
    nautilus_file_impl_complete_get_string_attribute (object, invocation, value);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_get_uri_scheme (NautilusFileImpl      *object,
                       GDBusMethodInvocation *invocation,
                       const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    char *uri_scheme;

    uri_scheme = nautilus_file_get_uri_scheme (file);
    nautilus_file_impl_complete_get_uri_scheme (object, invocation, uri_scheme);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_has_mount (NautilusFileImpl      *object,
                  GDBusMethodInvocation *invocation,
                  const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    g_autoptr (GMount) mount = nautilus_file_get_mount (file);
    gboolean result;

    result = (mount != NULL);
    nautilus_file_impl_complete_get_string_attribute (object, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_invalidate_extension_info (NautilusFileImpl      *object,
                                  GDBusMethodInvocation *invocation,
                                  const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);

    nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO);

    nautilus_file_impl_complete_invalidate_extension_info (object, invocation);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_is_directory (NautilusFileImpl      *object,
                     GDBusMethodInvocation *invocation,
                     const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    gboolean result;

    result = nautilus_file_is_directory (file);
    nautilus_file_impl_complete_is_directory (object, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_is_gone (NautilusFileImpl      *object,
                GDBusMethodInvocation *invocation,
                const char            *uri)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    gboolean result;

    result = nautilus_file_is_gone (file);
    nautilus_file_impl_complete_is_gone (object, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_is_mime_type (NautilusFileImpl      *object,
                     GDBusMethodInvocation *invocation,
                     const char            *uri,
                     const char            *mime_type)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);
    gboolean result;

    result = nautilus_file_is_mime_type (file, mime_type);
    nautilus_file_impl_complete_is_mime_type (object, invocation, result);

    return G_DBUS_METHOD_INVOCATION_HANDLED;
}

gboolean
nautilus_file_impl_manager_register (NautilusFileImplManager *self,
                                     GDBusConnection         *connection,
                                     GError                 **error)
{
    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                      connection,
                                      "/org/gnome/Nautilus" PROFILE "/FileImpl",
                                      error);

    return TRUE;
}

NautilusFileImplManager *
nautilus_file_impl_manager_new (void)
{
    return g_object_new (NAUTILUS_TYPE_FILE_IMPL_MANAGER, NULL);
}

static void
nautilus_file_impl_manager_init (NautilusFileImplManager *self)
{
    self->skeleton = nautilus_file_impl_skeleton_new ();

    g_signal_connect (self->skeleton, "handle-add-emblem", G_CALLBACK (handle_add_emblem), NULL);
    g_signal_connect (self->skeleton, "handle-add-string-attribute", G_CALLBACK (handle_add_string_attribute), NULL);
    g_signal_connect (self->skeleton, "handle-can-write", G_CALLBACK (handle_can_write), NULL);
    g_signal_connect (self->skeleton, "handle-get-activation-uri", G_CALLBACK (handle_get_activation_uri), NULL);
    g_signal_connect (self->skeleton, "handle-get-file-type", G_CALLBACK (handle_get_file_type), NULL);
    g_signal_connect (self->skeleton, "handle-get-mime-type", G_CALLBACK (handle_get_mime_type), NULL);
    g_signal_connect (self->skeleton, "handle-get-name", G_CALLBACK (handle_get_name), NULL);
    g_signal_connect (self->skeleton, "handle-get-parent-uri", G_CALLBACK (handle_get_parent_uri), NULL);
    g_signal_connect (self->skeleton, "handle-get-string-attribute", G_CALLBACK (handle_get_string_attribute), NULL);
    g_signal_connect (self->skeleton, "handle-get-uri-scheme", G_CALLBACK (handle_get_uri_scheme), NULL);
    g_signal_connect (self->skeleton, "handle-has-mount", G_CALLBACK (handle_has_mount), NULL);
    g_signal_connect (self->skeleton, "handle-invalidate-extension-info", G_CALLBACK (handle_invalidate_extension_info), NULL);
    g_signal_connect (self->skeleton, "handle-is-directory", G_CALLBACK (handle_is_directory), NULL);
    g_signal_connect (self->skeleton, "handle-is-gone", G_CALLBACK (handle_is_gone), NULL);
    g_signal_connect (self->skeleton, "handle-is-mime-type", G_CALLBACK (handle_is_mime_type), NULL);
}

static void
nautilus_file_impl_manager_class_init (NautilusFileImplManagerClass *klass)
{

}
