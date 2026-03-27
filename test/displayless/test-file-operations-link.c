/*
 * Copyright © 2025 Khalid Abu Shawarib
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests for nautilus_file_operations_link ()
 */

#include <nautilus-file-undo-manager.h>
#include <nautilus-file-operations.h>

#include "test-utilities.h"

typedef struct
{
    GStrv file_hierarchy; /* files to create */
    /* If NULL, reuse file_hierarchy as the files-to-link list */
    GStrv files_to_link;

    /* Assume the test temporary directory as a location to create the links */

    /* Destination relative names expected after linking (symlink names) */
    GStrv links_hierarchy;
} LinkTestCase;

static void
link_done_callback (GHashTable *debuting_uris,
                    gboolean    success,
                    gpointer    callback_data)
{
    gboolean *success_data = callback_data;

    g_assert_true (success);

    *success_data = success;
}

static void
assert_is_symlink_to (GFile       *link_location,
                      const gchar *link_target)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInfo) info = g_file_query_info (link_location,
                                                    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                    G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                                    G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                    NULL, &error);
    g_assert_nonnull (info);
    g_assert_no_error (error);
    g_assert_cmpint (g_file_info_get_file_type (info), ==, G_FILE_TYPE_SYMBOLIC_LINK);
    g_assert_true (g_file_info_get_is_symlink (info));

    g_assert_cmpstr (g_file_info_get_symlink_target (info), ==, link_target);
}

static void
assert_links (const GStrv links_hierarchy,
              const GStrv link_targets_hierarchy)
{
    g_autolist (GFile) links = file_hierarchy_get_files_list (links_hierarchy, "", FALSE);
    g_autolist (GFile) link_targets = file_hierarchy_get_files_list (link_targets_hierarchy, "", FALSE);

    for (GList *l = links, *m = link_targets; l != NULL && m != NULL; l = l->next, m = m->next)
    {
        GFile *link = l->data;
        GFile *link_target = m->data;

        assert_is_symlink_to (link, g_file_peek_path (link_target));
    }
}

static void
run_link_testcases (LinkTestCase test_cases[],
                    gsize        n)
{
    for (gsize i = 0; i < n; i++)
    {
        const GStrv file_hierarchy = test_cases[i].file_hierarchy;
        const GStrv files_to_link = test_cases[i].files_to_link != NULL
                                    ? test_cases[i].files_to_link
                                    : test_cases[i].file_hierarchy;
        const GStrv links_hierarchy = test_cases[i].links_hierarchy;
        g_autolist (GFile) file_list = file_hierarchy_get_files_list (files_to_link, "", FALSE);
        gboolean success = FALSE;
        g_autoptr (GFile) dest_dir = g_file_new_for_path (test_get_tmp_dir ());

        file_hierarchy_create (file_hierarchy, "");

        nautilus_file_operations_link (file_list,
                                       dest_dir,
                                       NULL,
                                       NULL,
                                       link_done_callback,
                                       &success);

        ITER_CONTEXT_WHILE (!success);

        file_hierarchy_assert_exists (file_hierarchy, "", TRUE);
        assert_links (links_hierarchy, files_to_link);

        test_operation_undo ();

        file_hierarchy_assert_exists (file_hierarchy, "", TRUE);
        file_hierarchy_assert_exists (links_hierarchy, "", FALSE);

        test_operation_redo ();

        file_hierarchy_assert_exists (file_hierarchy, "", TRUE);
        assert_links (links_hierarchy, files_to_link);

        test_clear_tmp_dir ();
    }
}

static void
test_link_files (void)
{
    LinkTestCase test_cases[] =
    {
        /* Single file */
        {
            (char *[]){
                "file_1",
                NULL
            },
            NULL,
            (char *[]){
                "Link to file_1",
                NULL
            }
        },
        /* Already linked file */
        {
            /* They are not actual links, but they are treated as such from the
             * looking at the names.
             */
            (char *[]){
                "file_1",
                "Link to file_1",
                "Link to file_1 (2)",
                "Link to file_1 (3)",
                "Link to file_1 (4)",
                NULL
            },
            (char *[]){
                "file_1",
                NULL
            },
            (char *[]){
                "Link to file_1 (5)",
                NULL
            }
        },
        /* Multiple files */
        {
            (char *[]){
                "file_1",
                "file_2",
                NULL
            },
            NULL,
            (char *[]){
                "Link to file_1",
                "Link to file_2",
                NULL
            }
        },
        /* Already linked files */
        {
            (char *[]){
                "file_1",
                "file_2",
                "Link to file_1",
                "Link to file_2",
                NULL
            },
            (char *[]){
                "file_1",
                "file_2",
                NULL
            },
            (char *[]){
                "Link to file_1 (2)",
                "Link to file_2 (2)",
                NULL
            }
        },
    };

    run_link_testcases (test_cases, G_N_ELEMENTS (test_cases));
}

static void
test_link_directories (void)
{
#define FULL_DIR(name) \
        name "/", \
        name "/dir_child/", \
        name "/dir_child/dir_grandchild"

    LinkTestCase test_cases[] =
    {
        /* Single directory */
        {
            (char *[]){
                FULL_DIR ("dir_1"),
                NULL
            },
            (char *[]){
                "dir_1",
                NULL
            },
            (char *[]){
                "Link to dir_1",
                NULL
            }
        },
        /* Already linked full directory */
        {
            /* They are not actual links, but they are treated as such from the
             * looking at the names.
             */
            (char *[]){
                FULL_DIR ("dir_1"),
                FULL_DIR ("Link to dir_1"),
                FULL_DIR ("Link to dir_1 (2)"),
                FULL_DIR ("Link to dir_1 (3)"),
                NULL
            },
            (char *[]){
                "dir_1",
                NULL
            },
            (char *[]){
                "Link to dir_1 (4)",
                NULL
            },
        },
        /* Multiple directories */
        {
            (char *[]){
                FULL_DIR ("dir_1"),
                FULL_DIR ("dir_2"),
                NULL
            },
            (char *[]){
                "dir_1/",
                "dir_2/",
                NULL
            },
            (char *[]){
                "Link to dir_1",
                "Link to dir_2",
                NULL
            },
        },
        /* Already duplicated directories */
        {
            (char *[]){
                FULL_DIR ("dir_1"),
                FULL_DIR ("dir_2"),
                FULL_DIR ("Link to dir_1"),
                FULL_DIR ("Link to dir_2"),
                NULL
            },
            (char *[]){
                "dir_1/",
                "dir_2/",
                NULL
            },
            (char *[]){
                "Link to dir_1 (2)",
                "Link to dir_2 (2)",
                NULL
            },
        },
    };

    run_link_testcases (test_cases, G_N_ELEMENTS (test_cases));
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    g_test_init (&argc, &argv, NULL);

    undo_manager = nautilus_file_undo_manager_new ();

    g_test_add_func ("/link/files", test_link_files);
    g_test_add_func ("/link/directories", test_link_directories);

    return g_test_run ();
}
