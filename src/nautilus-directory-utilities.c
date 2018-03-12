#include "nautilus-directory-utilities.h"
#include "nautilus-file-utilities.h"
#include "nautilus-directory.h"
#include <glib.h>

GList *
directory_is_empty_for_ui (NautilusDirectory *directory)
{
    g_autolist (NautilusFile) filtered_file_list = NULL;
    GList *file;

    for (file = directory->details->file_list; file != NULL; file = file->next)
    {
        if (!nautilus_file_is_hidden_file (file->data))
            filtered_file_list = g_list_prepend (filtered_file_list, file);
    }

    filtered_file_list = g_list_reverse (filtered_file_list);
    return filtered_file_list;
}