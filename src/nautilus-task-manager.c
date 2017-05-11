#include "nautilus-task-manager.h"

#include "nautilus-global-preferences.h"

struct _NautilusTaskManager
{
    GObject parent_instance;

    gint task_limit;
    GThreadPool *thread_pool;
};

G_DEFINE_TYPE (NautilusTaskManager, nautilus_task_manager, G_TYPE_OBJECT)

enum
{
    QUEUED,
    LAST_SIGNAL
};

static NautilusTaskManager *instance = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

static GObject *
constructor (GType                  type,
             guint                  n_construct_properties,
             GObjectConstructParam *construct_properties)
{
    static GMutex mutex;
    GObjectClass *parent_class;

    g_mutex_lock (&mutex);

    if (instance != NULL)
    {
        g_mutex_unlock (&mutex);
        return g_object_ref (instance);
    }

    parent_class = G_OBJECT_CLASS (nautilus_task_manager_parent_class);
    instance = NAUTILUS_TASK_MANAGER (parent_class->constructor (type,
                                                                 n_construct_properties,
                                                                 construct_properties));

    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);

    g_mutex_unlock (&mutex);

    return G_OBJECT (instance);
}

static void
finalize (GObject *object)
{
    NautilusTaskManager *self;

    self = NAUTILUS_TASK_MANAGER (object);

    g_thread_pool_free (self->thread_pool, TRUE, TRUE);
}

static void
nautilus_task_manager_class_init (NautilusTaskManagerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->constructor = constructor;
    object_class->finalize = finalize;

    signals[QUEUED] = g_signal_new ("queued",
                                    NAUTILUS_TYPE_TASK_MANAGER,
                                    0, 0, NULL, NULL,
                                    g_cclosure_marshal_VOID__OBJECT,
                                    G_TYPE_NONE,
                                    1,
                                    G_TYPE_OBJECT);
}

static void
on_task_limit_changed (GSettings *settings,
                       gchar     *key,
                       gpointer   user_data)
{
    NautilusTaskManager *self;
    gint task_limit;

    self = NAUTILUS_TASK_MANAGER (user_data);
    task_limit = g_settings_get_int (nautilus_preferences,
                                     NAUTILUS_PREFERENCES_TASK_LIMIT);

    if (task_limit == self->task_limit)
    {
        return;
    }

    self->task_limit = task_limit;

    g_thread_pool_set_max_threads (self->thread_pool, self->task_limit, NULL);
}

static void
execute_task (gpointer data,
              gpointer user_data)
{
    g_autoptr (NautilusTask) task = NULL;

    task = NAUTILUS_TASK (data);

    nautilus_task_execute (task);
}

static void
nautilus_task_manager_init (NautilusTaskManager *self)
{
    nautilus_global_preferences_init ();

    self->task_limit = g_settings_get_int (nautilus_preferences,
                                           NAUTILUS_PREFERENCES_TASK_LIMIT);
    self->thread_pool = g_thread_pool_new (execute_task, self,
                                           self->task_limit, FALSE,
                                           NULL);

    g_signal_connect (nautilus_preferences,
                      "changed::" NAUTILUS_PREFERENCES_TASK_LIMIT,
                      G_CALLBACK (on_task_limit_changed), self);
}

void
nautilus_task_manager_queue_task (NautilusTaskManager  *self,
                                  NautilusTask         *task)
{
    g_return_if_fail (NAUTILUS_IS_TASK_MANAGER (self));
    g_return_if_fail (NAUTILUS_IS_TASK (task));

    g_signal_emit (self, signals[QUEUED], 0, task);

    g_thread_pool_push (self->thread_pool, g_object_ref (task), NULL);
}

NautilusTaskManager *
nautilus_task_manager_dup_singleton (void)
{
    return g_object_new (NAUTILUS_TYPE_TASK_MANAGER, NULL);
}

