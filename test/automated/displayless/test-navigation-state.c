/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#include "test-utilities.h"

#include <src/nautilus-file-utilities.h>
#include <src/nautilus-navigation-state.h>

#include <gio/gio.h>


static void
basic_navigate (GFile    *file,
                gpointer  user_data)
{
    NautilusNavigationState *state = user_data;

    nautilus_navigation_state_navigate_location (state, file);
}

static void
assert_state_list_lengths (NautilusNavigationState *state,
                           guint                    length_back,
                           guint                    length_forward)
{
    g_assert_nonnull (state->current);
    g_assert_cmpint (g_list_length (state->back_list), ==, length_back);
    g_assert_cmpint (g_list_length (state->forward_list), ==, length_forward);
}

static void
test_navigation_basic (void)
{
    const GStrv hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_child/",
        "%s_first_dir/%s_second_child/",
        NULL
    };

    g_autoptr (NautilusNavigationState) state = nautilus_navigation_state_new ();

    file_hierarchy_create (hierarchy, "");
    file_hierarchy_foreach (hierarchy, "", basic_navigate, state);

    assert_state_list_lengths (state, 2, 0);

    g_autoptr (GFile) furthest = g_object_ref (state->current->location);

    nautilus_navigation_state_navigate_history (state, -2);

    assert_state_list_lengths (state, 0, 2);

    g_autoptr (NautilusNavigationState) copy = nautilus_navigation_state_copy (state);
    g_autoptr (GFile) first = g_object_ref (state->current->location);

    g_assert_false (g_file_equal (furthest, first));
    assert_state_list_lengths (copy, 0, 2);

    nautilus_navigation_state_navigate_location (state, furthest);

    assert_state_list_lengths (state, 1, 0);
    assert_state_list_lengths (copy, 0, 2);

    nautilus_navigation_state_navigate_history (copy, 2);

    assert_state_list_lengths (state, 1, 0);
    assert_state_list_lengths (copy, 2, 0);

    nautilus_navigation_state_navigate_location (copy, first);

    assert_state_list_lengths (state, 1, 0);
    assert_state_list_lengths (copy, 3, 0);
    g_assert_true (g_file_equal (copy->current->location, first));

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

    return g_test_run ();
}
