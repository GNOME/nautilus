#include "nautilus-task.h"

typedef struct
{
    GCancellable *cancellable;

    gpointer task_data;
    GDestroyNotify task_data_destroy;
} NautilusTaskPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusTask, nautilus_task,
                                     G_TYPE_OBJECT)

enum
{
    PROP_CANCELLABLE = 1,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    switch (property_id)
    {
        case PROP_CANCELLABLE:
        {
            NautilusTask *self;
            NautilusTaskPrivate *priv;

            self = NAUTILUS_TASK (object);
            priv = nautilus_task_get_instance_private (self);

            if (G_UNLIKELY (priv->cancellable) != NULL)
            {
                g_clear_object (&priv->cancellable);
            }

            priv->cancellable = g_value_dup_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
finalize (GObject *object)
{
    NautilusTask *self;
    NautilusTaskPrivate *priv;

    self = NAUTILUS_TASK (object);
    priv = nautilus_task_get_instance_private (self);

    g_clear_object (&priv->cancellable);

    if (priv->task_data != NULL && priv->task_data_destroy != NULL)
    {
        priv->task_data_destroy (priv->task_data);
    }

    G_OBJECT_CLASS (nautilus_task_parent_class)->finalize (object);
}

static void
nautilus_task_class_init (NautilusTaskClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = set_property;
    object_class->finalize = finalize;

    properties[PROP_CANCELLABLE] =
        g_param_spec_object ("cancellable", "Cancellable", "Cancellable",
                             G_TYPE_CANCELLABLE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_task_init (NautilusTask *self)
{
}

GCancellable *
nautilus_task_get_cancellable (NautilusTask *task)
{
    NautilusTaskPrivate *priv;

    g_return_val_if_fail (NAUTILUS_TASK (task), NULL);

    priv = nautilus_task_get_instance_private (task);

    return g_object_ref (priv->cancellable);
}

void
nautilus_task_execute (NautilusTask *task)
{
    NautilusTaskClass *klass;

    g_return_if_fail (NAUTILUS_IS_TASK (task));

    klass = NAUTILUS_TASK_GET_CLASS (task);

    g_return_if_fail (klass->execute != NULL);

    klass->execute (task);
}

gpointer
nautilus_task_get_task_data (NautilusTask *task)
{
    NautilusTaskPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_TASK (task), NULL);

    priv = nautilus_task_get_instance_private (task);

    return priv->task_data;
}

void
nautilus_task_set_task_data (NautilusTask   *task,
                             gpointer        task_data,
                             GDestroyNotify  task_data_destroy)
{
    NautilusTaskPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_TASK (task));

    priv = nautilus_task_get_instance_private (task);

    if (priv->task_data != NULL && priv->task_data_destroy != NULL)
    {
        priv->task_data_destroy (priv->task_data);
    }

    priv->task_data = task_data;
    priv->task_data_destroy = task_data_destroy;
}
