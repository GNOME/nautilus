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

void empty_directory (GFile *parent,
                      gchar *prefix);

void create_search_file_hierarchy (gchar *search_engine);
void delete_search_file_hierarchy (gchar *search_engine);

void quit_loop_callback (NautilusFileUndoManager *undo_manager,
                         GMainLoop               *loop);
void test_operation_undo_redo (void);
void test_operation_undo (void);

void create_one_file (gchar *prefix);
void create_one_empty_directory (gchar *prefix);

void create_multiple_files (gchar *prefix, gint number_of_files);
void create_multiple_directories (gchar *prefix, gint number_of_directories);
void create_multiple_full_directories (gchar *prefix, gint number_of_directories);

void create_first_hierarchy (gchar *prefix);
void create_second_hierarchy (gchar *prefix);
void create_third_hierarchy (gchar *prefix);
void create_fourth_hierarchy (gchar *prefix);