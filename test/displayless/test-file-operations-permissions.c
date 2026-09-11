/*
 * Copyright © 2026 Khalid Abu Shawarib
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests for nautilus_file_set_permissions_recursive ()
 */

#include "test-utilities.h"

#include <src/nautilus-file-operations.h>
#include <src/nautilus-file-undo-manager.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-tag-manager.h>

/* Only the standard POSIX permission bits are relevant for these tests. */
#define PERMISSIONS_MASK 0777

/* The passed directory itself is not considered an enclosed item, so it is
 * listed here only to be created; all assertions use permissions_children_hier. */
static const GStrv permissions_hierarchy = (char *[])
{
    "perm_dir/",
    "perm_dir/file_1",
    "perm_dir/child_dir/",
    "perm_dir/child_dir/file_2",
    "perm_dir/child_dir/grandchild_dir/",
    "perm_dir/child_dir/grandchild_dir/file_3",
    NULL
};

static const GStrv permissions_children_hier = &permissions_hierarchy[1];

typedef struct
{
    gboolean called;
    gboolean success;
} PermissionsCallbackData;

static void
permissions_done_callback (gboolean success,
                           gpointer callback_data)
{
    PermissionsCallbackData *data = callback_data;

    data->called = TRUE;
    data->success = success;
}

static guint32
get_unix_permissions (GFile *file)
{
    g_autoptr (GFileInfo) info = g_file_query_info (file,
                                                    G_FILE_ATTRIBUTE_UNIX_MODE,
                                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                    NULL, NULL);

    g_assert_nonnull (info);
    g_assert_true (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE));

    return g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE) & PERMISSIONS_MASK;
}

static gboolean
file_is_directory (GFile *file)
{
    return g_file_query_file_type (file,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   NULL) == G_FILE_TYPE_DIRECTORY;
}

static void
set_hierarchy_permissions (const GStrv hierarchy,
                           guint32     file_permissions,
                           guint32     dir_permissions)
{
    g_autolist (GFile) files = file_hierarchy_get_files_list (hierarchy, "", FALSE);

    for (GList *l = files; l != NULL; l = l->next)
    {
        GFile *file = l->data;
        guint32 permissions = file_is_directory (file) ? dir_permissions : file_permissions;

        g_assert_true (g_file_set_attribute_uint32 (file,
                                                    G_FILE_ATTRIBUTE_UNIX_MODE,
                                                    permissions,
                                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                    NULL, NULL));
    }
}

static void
assert_hierarchy_permissions (const GStrv hierarchy,
                              guint32     file_permissions,
                              guint32     dir_permissions)
{
    g_autolist (GFile) files = file_hierarchy_get_files_list (hierarchy, "", FALSE);

    for (GList *l = files; l != NULL; l = l->next)
    {
        GFile *file = l->data;
        guint32 expected = file_is_directory (file) ? dir_permissions : file_permissions;

        g_assert_cmphex (get_unix_permissions (file), ==, expected);
    }
}

static void
run_set_permissions_recursive (GFile   *directory,
                               guint32  file_permissions,
                               guint32  file_mask,
                               guint32  dir_permissions,
                               guint32  dir_mask)
{
    g_autofree gchar *uri = g_file_get_uri (directory);
    PermissionsCallbackData callback_data = { 0 };

    nautilus_file_set_permissions_recursive (uri,
                                             file_permissions, file_mask,
                                             dir_permissions, dir_mask,
                                             permissions_done_callback,
                                             &callback_data);

    ITER_CONTEXT_WHILE (!callback_data.called);

    g_assert_true (callback_data.success);
}

static void
test_set_permissions_recursive (void)
{
    const guint32 initial_file_permissions = 0644;
    const guint32 initial_dir_permissions = 0755;
    const guint32 new_file_permissions = 0600;
    const guint32 new_dir_permissions = 0700;
    g_autoptr (GFile) directory = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "perm_dir",
                                                             NULL);

    file_hierarchy_create (permissions_hierarchy, "");
    set_hierarchy_permissions (permissions_hierarchy,
                               initial_file_permissions,
                               initial_dir_permissions);
    assert_hierarchy_permissions (permissions_children_hier,
                                  initial_file_permissions,
                                  initial_dir_permissions);

    run_set_permissions_recursive (directory,
                                   new_file_permissions, PERMISSIONS_MASK,
                                   new_dir_permissions, PERMISSIONS_MASK);

    /* The directory itself is not enclosed, so it must keep its permissions. */
    g_assert_cmphex (get_unix_permissions (directory), ==, initial_dir_permissions);
    assert_hierarchy_permissions (permissions_children_hier,
                                  new_file_permissions,
                                  new_dir_permissions);

    test_operation_undo ();

    g_assert_cmphex (get_unix_permissions (directory), ==, initial_dir_permissions);
    assert_hierarchy_permissions (permissions_children_hier,
                                  initial_file_permissions,
                                  initial_dir_permissions);

    test_operation_redo ();

    g_assert_cmphex (get_unix_permissions (directory), ==, initial_dir_permissions);
    assert_hierarchy_permissions (permissions_children_hier,
                                  new_file_permissions,
                                  new_dir_permissions);

    test_clear_tmp_dir ();
}

/* Only the bits selected by the mask should be changed, while the remaining
 * permission bits are preserved. */
static void
test_set_permissions_recursive_partial_mask (void)
{
    const guint32 initial_file_permissions = 0644;
    const guint32 initial_dir_permissions = 0755;
    /* Clear the "other" permission bits only. */
    const guint32 other_mask = 0007;
    g_autoptr (GFile) directory = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "perm_dir",
                                                             NULL);

    file_hierarchy_create (permissions_hierarchy, "");
    set_hierarchy_permissions (permissions_hierarchy,
                               initial_file_permissions,
                               initial_dir_permissions);

    run_set_permissions_recursive (directory,
                                   0000, other_mask,
                                   0000, other_mask);

    assert_hierarchy_permissions (permissions_children_hier,
                                  initial_file_permissions & ~other_mask,
                                  initial_dir_permissions & ~other_mask);

    test_operation_undo ();

    assert_hierarchy_permissions (permissions_children_hier,
                                  initial_file_permissions,
                                  initial_dir_permissions);

    test_operation_redo ();

    assert_hierarchy_permissions (permissions_children_hier,
                                  initial_file_permissions & ~other_mask,
                                  initial_dir_permissions & ~other_mask);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;
    g_autoptr (NautilusTagManager) tag_manager = NULL;

    undo_manager = nautilus_file_undo_manager_new ();
    tag_manager = nautilus_tag_manager_new_dummy ();
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/permissions/recursive/basic",
                     test_set_permissions_recursive);
    g_test_add_func ("/permissions/recursive/partial-mask",
                     test_set_permissions_recursive_partial_mask);

    return g_test_run ();
}
