#include <glib.h>

static void
create_search_file_hierarchy (gchar *search_engine)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;
    GFileOutputStream *out;
    g_autoptr (GError) error = NULL;
    g_autofree gchar *file_name = NULL;

    location = g_file_new_for_path (g_get_tmp_dir ());

    file_name = g_strdup_printf ("engine_%s", search_engine);
    file = g_file_get_child (location, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }


    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_make_directory (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
}

static void
delete_search_file_hierarchy (gchar *search_engine)
{
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GError) error = NULL;
    g_autofree gchar *file_name = NULL;

    location = g_file_new_for_path (g_get_tmp_dir ()); 

    file_name = g_strdup_printf ("engine_%s", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    file_name = g_strdup_printf ("%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    g_file_delete (file, NULL, NULL);
    file_name = g_strdup_printf ("engine_%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);

    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    g_file_delete (file, NULL, NULL);
    file_name = g_strdup_printf ("engine_%s_second_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);

    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    file_name = g_strdup_printf ("engine_%s_child", search_engine);
    file = g_file_get_child (file, file_name);
    g_file_delete (file, NULL, NULL);
    file_name = g_strdup_printf ("%s_directory", search_engine);
    file = g_file_get_child (location, file_name);
    g_file_delete (file, NULL, NULL);
}