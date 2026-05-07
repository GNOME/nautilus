/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#define G_LOG_DOMAIN "test-icon-info"

#include <nautilus-icon-info.h>

#include <gtk/gtk.h>

static void
test_themed_cache (void)
{
    struct
    {
        const char *icon_name;
        int size;
        int scale;
        int equality_group;
    } test_cases[] =
    {
        { "text-x-generic", 16, 1, 0 },
        { "text-x-generic", 16, 1, 0 },
        { "image-x-generic", 16, 1, 1 },
        { "text-x-generic", 32, 1, 2 },
        { "text-x-generic", 16, 2, 3 },
        { "application-x-should-not-exist", 16, 1, 4 },
        { "application-x-should-also-not-exist", 16, 1, 4 },
        { "application-x-should-not-exist", 32, 1, 5 },
        { "application-x-should-not-exist", 16, 2, 6 },
    };

    GdkPaintable *paintables[G_N_ELEMENTS (test_cases)] = { NULL };

    for (size_t i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        g_autoptr (GIcon) icon = g_themed_icon_new (test_cases[i].icon_name);

        paintables[i] = nautilus_icon_info_lookup (icon, test_cases[i].size,
                                                   test_cases[i].scale);
        g_assert_nonnull (paintables[i]);
        g_assert_cmpint (gdk_paintable_get_intrinsic_width (paintables[i]),
                         ==,
                         test_cases[i].size);
        g_assert_cmpint (gdk_paintable_get_intrinsic_height (paintables[i]),
                         ==,
                         test_cases[i].size);
    }

    for (size_t i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        for (size_t j = i; j < G_N_ELEMENTS (test_cases); j++)
        {
            if (test_cases[i].equality_group == test_cases[j].equality_group)
            {
                g_assert_true (paintables[i] == paintables[j]);
            }
            else
            {
                g_assert_false (paintables[i] == paintables[j]);
            }
        }
    }

    for (size_t i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
        g_object_unref (paintables[i]);
    }
}

int
main (int   argc,
      char *argv[])
{
    gtk_test_init (&argc, &argv, NULL);

    g_test_add_func ("/icon-info/themed/cache",
                     test_themed_cache);

    return g_test_run ();
}
