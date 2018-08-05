#include "src/nautilus-file-undo-manager.h"
#include <gio/gio.h>

#pragma once

void create_search_file_hierarchy  (gchar *search_engine);

void delete_search_file_hierarchy  (gchar *search_engine);

void quit_loop_callback            (NautilusFileUndoManager *undo_manager,
                                    GMainLoop               *loop);
void test_operation_undo_redo      (void);
void test_operation_undo           (void);

void create_one_file               (gchar *prefix);
void create_one_directory          (gchar *prefix);