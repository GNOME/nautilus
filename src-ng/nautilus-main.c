#include <locale.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-task.h"
#include "nautilus-tasks.h"

static void
got_info (NautilusFile *file,
          GFileInfo    *info,
          GError       *error,
          gpointer      user_data)
{
    g_print ("Got info for %p\n"
             "\tDisplay name: %s\n"
             "\tFile is directory: %s\n\n",
             (gpointer) file,
             g_file_info_get_display_name (info),
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
    g_print ("Got children for %p\n", (gpointer) directory);

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

static void
perform_self_test_checks (const gchar *path)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (NautilusFile) duplicate_file = NULL;
    GMainLoop *loop;

    location = g_file_new_for_path (path);

    g_print ("Creating NautilusFile\n");
    file = nautilus_file_new (location);
    g_print ("\tGot %p\n\n", (gpointer) file);

    g_print ("Creating another NautilusFile for the same location\n");
    duplicate_file = nautilus_file_new (location);
    g_print ("\tGot %p, which is %s\n\n",
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
}

static void
on_children_changed (NautilusDirectory *directory,
                     gpointer           user_data)
{
    g_main_loop_quit ((GMainLoop *) user_data);
}

static void
_rename (const gchar *target,
         const gchar *name)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (NautilusFile) parent = NULL;
    g_autoptr (GHashTable) targets = NULL;
    g_autoptr (NautilusTask) task = NULL;
    GMainLoop *loop;

    location = g_file_new_for_path (target);
    g_message ("Constructed GFile %p for path %s",
               (gpointer) location, target);
    file = nautilus_file_new (location);
    parent = nautilus_file_get_parent (file);
    g_message ("Constructed NautilusFile %p for location %p",
               (gpointer) file, (gpointer) location);
    targets = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
    task = nautilus_task_new_with_func (nautilus_rename_task_func, targets, NULL);
    loop = g_main_loop_new (NULL, TRUE);

    (void) g_hash_table_insert (targets, location, name);

    g_signal_connect (parent, "children-changed",
                      G_CALLBACK (on_children_changed), loop);

    nautilus_task_run (task);

    g_main_loop_run (loop);
}

/*static void
on_thumbnail_finished (NautilusThumbnailTask *task,
                       GFile                 *location,
                       GdkPixbuf             *thumbnail,
                       gpointer               user_data)
{
    GtkWidget *image;

    image = gtk_image_new_from_pixbuf (thumbnail);

    gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
    gtk_widget_set_visible (image, TRUE);

    gtk_container_add (GTK_CONTAINER (user_data), image);

    g_object_unref (thumbnail);
}*/

static gboolean
on_window_deleted (GtkWidget *widget,
                   GdkEvent  *event,
                   gpointer   user_data)
{
    gtk_main_quit ();

    return GDK_EVENT_PROPAGATE;
}

static void
display_thumbnail (const gchar *path)
{
    /*GtkWidget *window;
    g_autoptr (GFile) location = NULL;
    g_autoptr (NautilusTask) task = NULL;
    g_autoptr (NautilusTaskManager) task_manager = NULL;

    gtk_init (NULL, NULL);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    location = g_file_new_for_path (path);
    task = nautilus_thumbnail_task_new (location, TRUE);
    task_manager = nautilus_task_manager_dup_singleton ();

    gtk_widget_show_all (window);

    g_signal_connect_after (window, "delete-event", G_CALLBACK (on_window_deleted), NULL);
    g_signal_connect (task, "finished", G_CALLBACK (on_thumbnail_finished), window);

    nautilus_task_manager_queue_task (task_manager, task);

    gtk_main ();*/
}

int
main (int    argc,
      char **argv)
{
    g_autoptr (GOptionContext) option_context = NULL;
    gchar **files = NULL;
    gboolean check = FALSE;
    gchar *new_name = NULL;
    gboolean thumbnail = FALSE;
    const GOptionEntry option_entries[] =
    {
        {
            G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE,
            G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, NULL
        },
        {
            "check", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &check,
            "Perform self-test checks with FILE as input", NULL
        },
        {
            "rename", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &new_name,
            "Rename FILE to NAME", "NAME"
        },
        {
            "thumbnail", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &thumbnail,
            "Display thumbnail for FILE", NULL
        },
        { NULL }
    };
    GError *error = NULL;

    setlocale (LC_ALL, "");

    option_context = g_option_context_new ("[FILE]");

    g_option_context_add_main_entries (option_context, option_entries, NULL);

    if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
        g_print ("%s\n", error->message);

        return EXIT_FAILURE;
    }

    if (files == NULL)
    {
        g_print ("No input file specified\n");

        return EXIT_FAILURE;
    }

    if (check)
    {
        perform_self_test_checks (files[0]);
    }

    if (new_name != NULL && new_name[0] != '\0')
    {
        _rename (files[0], new_name);
    }

    if (thumbnail)
    {
        display_thumbnail (files[0]);
    }

    return EXIT_SUCCESS;
}
