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
    g_message ("\tDisplay name: %s\n",
               g_file_info_get_display_name (info));

    g_object_unref (info);

    g_main_loop_quit ((GMainLoop *) user_data);
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
    g_message ("\tGot %p", (gpointer) file);
    g_message ("\tFile is directory: %s\n",
               NAUTILUS_IS_DIRECTORY (file)? "yes" : "no");

    g_message ("Creating another NautilusFile for the same location");
    duplicate_file = nautilus_file_new (location);
    g_message ("\tGot %p, which is %s\n",
               (gpointer) duplicate_file,
               file == duplicate_file? "the same" : "not the same");

    loop = g_main_loop_new (NULL, TRUE);

    nautilus_file_query_info (file, NULL, got_info, loop);

    g_main_loop_run (loop);

    return EXIT_SUCCESS;
}
