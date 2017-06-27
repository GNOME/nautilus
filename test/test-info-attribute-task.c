#include <stdlib.h>

#include <gtk/gtk.h>

#include <nautilus-file-utilities.h>
#include <nautilus-task-manager.h>
#include <tasks/nautilus-info-attribute-task.h>

#define ATTRIBUTE "owner"

static void
callback (NautilusTask *task,
          gpointer      user_data)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (user_data);

    g_message ("%s: %s",
               ATTRIBUTE,
               nautilus_file_get_string_attribute (file, ATTRIBUTE));

    nautilus_file_unref (file);

    gtk_main_quit ();
}

int
main (int    argc,
      char **argv)
{
    NautilusFile *file;
    g_autoptr (NautilusTaskManager) manager = NULL;
    g_autoptr (NautilusTask) task = NULL;

    gtk_init (&argc, &argv);

    nautilus_ensure_extension_points ();

    file = nautilus_file_get_by_uri ("file:///tmp");
    manager = nautilus_task_manager_dup_singleton ();
    task = nautilus_info_attribute_task_new_for_file (file);

    g_message ("%s: %s",
               ATTRIBUTE,
               nautilus_file_get_string_attribute (file, ATTRIBUTE));
    nautilus_task_manager_queue_task (manager, task, callback, file);

    file = NULL;

    gtk_main ();

    return EXIT_SUCCESS;
}
