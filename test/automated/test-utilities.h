#include <gio/gio.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-global-preferences.h>
#include <src/nautilus-search-engine.h>
#include <tracker-sparql.h>
#include <locale.h>
#include <src/nautilus-file-undo-manager.h>
#include <src/nautilus-file-operations.h>
#include <gio/gio.h>

#pragma once

#define MAX_CONTEXT_ITERATIONS 100

#define ITER_CONTEXT_WHILE(CONDITION) \
    for (guint context_iter = 0; \
         context_iter < MAX_CONTEXT_ITERATIONS && \
         CONDITION; \
         context_iter++) \
    { \
        g_main_context_iteration (NULL, TRUE); \
    }

const gchar *test_get_tmp_dir (void);
void test_clear_tmp_dir (void);

void test_init_config_dir (void);

void empty_directory_by_prefix (GFile *parent,
                                gchar *prefix);

void create_hierarchy_from_template (const GStrv  hier,
                                     const gchar *substitution);

void create_search_file_hierarchy (gchar *search_engine);
void delete_search_file_hierarchy (gchar *search_engine);

void quit_loop_callback (NautilusFileUndoManager *undo_manager,
                         GMainLoop               *loop);
void test_operation_undo_redo (void);
void test_operation_undo (void);

void create_one_file (gchar *prefix);
void create_one_empty_directory (gchar *prefix);

void create_multiple_files (gchar *prefix, guint number_of_files);
void create_multiple_directories (gchar *prefix, guint number_of_directories);
void create_multiple_full_directories (gchar *prefix, guint number_of_directories);

void create_first_hierarchy (gchar *prefix);
void create_second_hierarchy (gchar *prefix);
void create_third_hierarchy (gchar *prefix);
void create_fourth_hierarchy (gchar *prefix);
