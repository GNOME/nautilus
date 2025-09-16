#include "nautilus-types.h"

#include <gio/gio.h>

#pragma once

#define MAX_CONTEXT_ITERATIONS 100

#define ITER_CONTEXT_WHILE(CONDITION) \
    for (guint context_iter = 0; \
         context_iter < MAX_CONTEXT_ITERATIONS && \
         (CONDITION); \
         context_iter++) \
    { \
        g_main_context_iteration (NULL, TRUE); \
    }

const gchar *test_get_tmp_dir (void);
void test_clear_tmp_dir (void);

void test_init_config_dir (void);

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

void quit_loop_callback (NautilusFileUndoManager *undo_manager,
                         GMainLoop               *loop);
void test_operation_undo_redo (void);
void test_operation_undo (void);
void test_operation_redo (void);

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

gboolean can_run_bwrap (void);
