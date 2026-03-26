/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#include "test-utilities.h"

#include <src/nautilus-file.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-navigation-state.h>

#include <gio/gio.h>


static void
basic_navigate (GFile    *location,
                gpointer  user_data)
{
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    NautilusNavigationState *state = user_data;

    nautilus_navigation_state_navigate_file (state, file);
}

static void
assert_state_list_lengths (NautilusNavigationState *state,
                           guint                    length_back,
                           guint                    length_forward)
{
    g_assert_true (nautilus_navigation_state_has_current (state));
    g_assert_true (nautilus_navigation_state_has_backward (state) == (length_back > 0));
    g_assert_true (nautilus_navigation_state_has_forward (state) == (length_forward > 0));

    for (int i = -1; ABS (i) <= length_back; i -= 1)
    {
        g_assert_nonnull (nautilus_navigation_state_get_nth (state, i, NULL));
    }

    for (int i = 1; ABS (i) <= length_forward; i += 1)
    {
        g_assert_nonnull (nautilus_navigation_state_get_nth (state, i, NULL));
    }
}

static void
assert_selection (NautilusNavigationState *state,
                  NautilusFileList        *expected_selection)
{
    NautilusFileList *l, *l2;
    g_autolist (NautilusFile) selection = NULL;
    NautilusFile *current = nautilus_navigation_state_get_nth (state, 0, &selection);

    g_assert_nonnull (current);

    for (l = selection, l2 = expected_selection;
         l != NULL;
         l = l->next, l2 = l2->next)
    {
        g_assert_nonnull (l2);
        g_assert_true (l->data == l2->data);
    }
}

static void
test_navigation_basic (void)
{
    const GStrv hierarchy = (char *[])
    {
        "first_dir/",
        "first_dir/first_child/",
        "first_dir/second_child/",
        NULL
    };

    g_autoptr (NautilusNavigationState) state = nautilus_navigation_state_new ();

    file_hierarchy_create (hierarchy, "");
    file_hierarchy_foreach (hierarchy, "", basic_navigate, state);

    assert_state_list_lengths (state, 2, 0);

    g_autoptr (NautilusFile) furthest =
        g_object_ref (nautilus_navigation_state_get_nth (state, 0, NULL));
    g_autoptr (NautilusFile) first =
        g_object_ref (nautilus_navigation_state_get_nth (state, -2, NULL));

    nautilus_navigation_state_navigate_history (state, -2);

    g_assert_false (furthest == first);
    assert_state_list_lengths (state, 0, 2);

    g_autoptr (NautilusNavigationState) copy = nautilus_navigation_state_copy (state);

    /* Current is always accessable, even when inactive */
    g_assert_true (first == nautilus_navigation_state_get_nth (copy, 0, NULL));

    nautilus_navigation_state_activate (copy);

    assert_state_list_lengths (copy, 0, 2);

    nautilus_navigation_state_navigate_file (state, furthest);

    assert_state_list_lengths (state, 1, 0);
    assert_state_list_lengths (copy, 0, 2);

    nautilus_navigation_state_navigate_history (copy, 2);

    assert_state_list_lengths (state, 1, 0);
    assert_state_list_lengths (copy, 2, 0);

    nautilus_navigation_state_navigate_file (copy, first);

    assert_state_list_lengths (state, 1, 0);
    assert_state_list_lengths (copy, 3, 0);

    g_autoptr (NautilusFile) copy_current =
        g_object_ref (nautilus_navigation_state_get_nth (copy, 0, NULL));

    g_assert_true (copy_current == first);

    test_clear_tmp_dir ();
}

static void
test_selection_storing (void)
{
    const GStrv hierarchy = (char *[])
    {
        "first_dir/",
        "first_dir/first_file",
        "first_dir/second_file",
        "second_dir/",
        NULL
    };

    file_hierarchy_create (hierarchy, "");
    g_autolist (GFile) files = file_hierarchy_get_files_list (hierarchy, "", FALSE);

    g_autoptr (NautilusNavigationState) state = nautilus_navigation_state_new ();
    g_autoptr (NautilusFile) first_dir = nautilus_file_get (g_list_nth_data (files, 0));
    g_autoptr (NautilusFile) first_file = nautilus_file_get (g_list_nth_data (files, 1));
    g_autoptr (NautilusFile) second_file = nautilus_file_get (g_list_nth_data (files, 2));
    g_autoptr (NautilusFile) second_dir = nautilus_file_get (g_list_nth_data (files, 3));
    g_autoptr (GStrvBuilder) selected_uris = g_strv_builder_new ();

    nautilus_navigation_state_navigate_file (state, first_dir);

    assert_state_list_lengths (state, 0, 0);

    g_strv_builder_take (selected_uris, nautilus_file_get_uri (first_file));
    g_strv_builder_take (selected_uris, nautilus_file_get_uri (second_file));
    nautilus_navigation_state_set_selection (state, g_strv_builder_end (selected_uris));

    g_autoptr (NautilusFileList) selection =
        g_list_prepend (g_list_prepend (NULL, second_file), first_file);

    assert_selection (state, selection);

    nautilus_navigation_state_navigate_file (state, second_dir);

    assert_state_list_lengths (state, 1, 0);
    assert_selection (state, NULL);

    nautilus_navigation_state_navigate_history (state, -12);

    assert_state_list_lengths (state, 0, 1);
    assert_selection (state, selection);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/navigation-state/basic",
                     test_navigation_basic);
    g_test_add_func ("/navigation-state/selection-storing",
                     test_selection_storing);

    return g_test_run ();
}
