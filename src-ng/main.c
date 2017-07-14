#include <stdlib.h>

#include <glib.h>

#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-task-manager.h"

static void
got_info (NautilusFile *file,
          GFileInfo    *info,
          GError       *error,
          gpointer      user_data)
{
    g_message ("Got info for %p",
               (gpointer) file);
    g_message ("\tDisplay name: %s",
               g_file_info_get_display_name (info));
    g_message ("\tFile is directory: %s\n",
               NAUTILUS_IS_DIRECTORY (file)? "yes" : "no");

    g_object_unref (info);

    if (user_data != NULL)
    {
        g_main_loop_quit ((GMainLoop *) user_data);
    }
}

static void
got_children (NautilusDirectory *directory,
              GList             *children,
              GError            *error,
              gpointer           user_data)
{
    g_message ("Got children for %p", (gpointer) directory);

    if (children == NULL)
    {
        g_list_free (children);

        g_main_loop_quit ((GMainLoop *) user_data);

        return;
    }

    for (GList *i = children; i != NULL; i = i->next)
    {
        if (G_UNLIKELY (i->next == NULL))
        {
            nautilus_file_query_info (NAUTILUS_FILE (i->data),
                                      NULL, got_info, user_data);
        }
        else
        {
            nautilus_file_query_info (NAUTILUS_FILE (i->data),
                                      NULL, got_info, NULL);
        }
    }

    g_list_free (children);
}

int
main (int    argc,
      char **argv)
{
    g_autoptr (NautilusTaskManager) manager = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (NautilusFile) duplicate_file = NULL;
    GMainLoop *loop;

    if (!(argc > 1))
    {
        g_message ("No file provided, exiting");
        return EXIT_SUCCESS;
    }

    manager = nautilus_task_manager_dup_singleton ();
    location = g_file_new_for_commandline_arg (argv[1]);

    g_message ("Creating NautilusFile");
    file = nautilus_file_new (location);
    g_message ("\tGot %p\n", (gpointer) file);

    g_message ("Creating another NautilusFile for the same location");
    duplicate_file = nautilus_file_new (location);
    g_message ("\tGot %p, which is %s\n",
               (gpointer) duplicate_file,
               file == duplicate_file? "the same" : "not the same");

    loop = g_main_loop_new (NULL, TRUE);

    if (NAUTILUS_IS_DIRECTORY (file))
    {
        nautilus_file_query_info (file, NULL, got_info, NULL);
    }
    else
    {
        nautilus_file_query_info (file, NULL, got_info, loop);
    }

    if (NAUTILUS_IS_DIRECTORY (file))
    {
        nautilus_directory_enumerate_children (NAUTILUS_DIRECTORY (file),
                                               NULL,
                                               got_children,
                                               loop);
    }

    g_main_loop_run (loop);

    return EXIT_SUCCESS;
}
