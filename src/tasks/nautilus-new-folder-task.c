#include "nautilus-new-folder-task.h"

struct _NautilusNewFolderTask
{
    NautilusCreateTask parent_instance;
};

G_DEFINE_TYPE (NautilusNewFolderTask, nautilus_new_folder_task,
               NAUTILUS_TYPE_CREATE_TASK)

static void
nautilus_new_folder_task_class_init (NautilusNewFolderTaskClass *klass)
{
}

static void
nautilus_new_folder_task_init (NautilusNewFolderTask *self)
{
}

NautilusTask *
nautilus_new_folder_task_new (GtkWidget  *parent_view,
                              GdkPoint   *target_point,
                              const char *parent_dir,
                              const char *folder_name)
{
    GObject *instance;

    instance = g_object_new (NAUTILUS_TYPE_NEW_FOLDER_TASK, NULL);

    return NAUTILUS_TASK (instance);
}
