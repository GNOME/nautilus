#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#define BUTTON_ACTIVATION_DELAY_IN_SECONDS 2

typedef struct {
    int id;
    char *new_name;
    gboolean apply_to_all;
} FileConflictResponse;

void file_conflict_response_free (FileConflictResponse *data);

FileConflictResponse * copy_move_conflict_ask_user_action (GtkWindow *parent_window,
                                                           gboolean   should_start_inactive,
                                                           GFile     *src,
                                                           GFile     *dest,
                                                           GFile     *dest_dir,
                                                           gchar     *suggestion);

enum
{
    CONFLICT_RESPONSE_SKIP = 1,
    CONFLICT_RESPONSE_REPLACE = 2,
    CONFLICT_RESPONSE_RENAME = 3,
};

void handle_unsupported_compressed_file (GtkWindow *parent_window,
                                         GFile     *compressed_file);

gchar *extract_ask_passphrase (GtkWindow   *parent_window,
                               const gchar *archive_basename);
