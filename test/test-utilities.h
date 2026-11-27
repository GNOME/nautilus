#include "nautilus-types.h"

#include <gio/gio.h>

#pragma once

#define DEFAULT_IMAGE_SIZE 256
#define MAX_CONTEXT_ITERATIONS 100

#define ITER_CONTEXT_WHILE(CONDITION) \
    for (guint context_iter = 0; CONDITION ; context_iter++) \
    { \
        if (context_iter >= MAX_CONTEXT_ITERATIONS) { g_assert_true (FALSE); break; } \
        g_main_context_iteration (NULL, TRUE); \
    }

typedef enum
{
    NAUTILUS_TEST_INIT_NONE = 0,
    /* Isolate the XDG user dirs and initialize the config dir. */
    NAUTILUS_TEST_INIT_ISOLATE = 1 << 0,
    NAUTILUS_TEST_INIT_REGISTER_RESOURCE = 1 << 1,
    NAUTILUS_TEST_INIT_EXTENSIONS = 1 << 2,
    NAUTILUS_TEST_INIT_PREFS = 1 << 3,
} NautilusTestInitFlags;

void nautilus_test_init (int                     *argc,
                         char                  ***argv,
                         NautilusTestInitFlags    flags);

void nautilus_test_gtk_init (int                     *argc,
                             char                  ***argv,
                             NautilusTestInitFlags    flags);

const gchar *test_get_tmp_dir (void);
void test_clear_tmp_dir (void);

void test_isolate_user_dirs (void);
void test_init_config_dir (void);

/* Returns the root of a new tmpfs mount, or NULL after skipping the test if
 * mounting is not permitted. Pair with test_mount_point_destroy (). */
GFile *test_mount_point_create (void);
void test_mount_point_destroy (void);

void empty_directory_by_prefix (GFile *parent,
                                gchar *prefix);

typedef void (*HierarchyCallback) (GFile    *file,
                                   gpointer  user_data);
void file_hierarchy_create (const GStrv  hier,
                     const gchar *substitution);
void file_hierarchy_foreach (const GStrv        hier,
                             const gchar       *substitution,
                             HierarchyCallback  func,
                             gpointer           user_data);
GList * file_hierarchy_get_files_list (const GStrv  hier,
                                       const gchar *substitution,
                                       gboolean     shallow);
void file_hierarchy_assert_exists (const GStrv  hier,
                                   const gchar *substitution,
                                   gboolean     exists);
void file_hierarchy_delete (const GStrv  hier,
                            const gchar *substitution);

void create_search_file_hierarchy (gchar *search_engine);
void delete_search_file_hierarchy (gchar *search_engine);

void test_operation_undo_redo (void);
void test_operation_undo (void);
void test_operation_redo (void);
void test_operation_cancel (void);

void create_one_file (gchar *prefix);
void create_one_empty_directory (gchar *prefix);

void create_multiple_files (gchar *prefix, guint number_of_files);
void create_multiple_directories (gchar *prefix, guint number_of_directories);
void create_multiple_full_directories (gchar *prefix, guint number_of_directories);

void create_first_hierarchy (gchar *prefix);
void create_second_hierarchy (gchar *prefix);
void create_third_hierarchy (gchar *prefix);
void create_fourth_hierarchy (gchar *prefix);

void create_random_file (GFile *file,
                         gsize size);
void file_load_attributes (NautilusFile       *file,
                           NautilusAttributes  attributes);
void directory_load_attributes (NautilusDirectory  *directory,
                                NautilusAttributes  attributes);
void make_image_file_full (GFile    *file,
                           guint8    color[4],
                           guint     width,
                           guint     height,
                           guint64   mtime,
                           gboolean  thumbnailed);
void make_image_file_with_mtime (GFile    *file,
                                 guint64   mtime,
                                 gboolean  thumbnailed);
void make_image_file (GFile    *file,
                      gboolean  thumbnailed);
