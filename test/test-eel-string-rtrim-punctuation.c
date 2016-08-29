#include <glib.h>

#include "eel/eel-string.h"


static void
test_single_punctuation_character_removed ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Yossarian-");
    g_assert_cmpstr ("Yossarian", ==, actual);
    g_free (actual);
}

static void
test_tailing_space_is_removed ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Yossarian ");
    g_assert_cmpstr ("Yossarian", ==, actual);
    g_free (actual);
}

static void
test_multiple_punctuation_characters_removed ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Yossarian-$$!£");
    g_assert_cmpstr ("Yossarian", ==, actual);
    g_free (actual);
}

static void
test_multiple_punctuation_characters_removed_try_all_punctuation_characters ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Yossarian-`¬!\"£$%^&*()_+-= {}[]:@~;'#<>?,./\\");
    g_assert_cmpstr ("Yossarian", ==, actual);
    g_free (actual);
}

static void
test_punctuation_characters_removed_when_punctuation_in_middle_of_string ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Yoss,,arian-$$!£");
    g_assert_cmpstr ("Yoss,,arian", ==, actual);
    g_free (actual);
}

static void
test_punctuation_characters_removed_when_prefix_is_single_character ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Y-$$  !£");
    g_assert_cmpstr ("Y", ==, actual);
    g_free (actual);
}

static void
test_punctuation_characters_removed_when_unicode_characters_are_used ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Y✺ǨǨǨ-$$  !£");
    g_assert_cmpstr ("Y✺ǨǨǨ", ==, actual);
    g_free (actual);
}

static void
test_when_no_trailing_punctuation ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("Yoss,,arian");
    g_assert_cmpstr ("Yoss,,arian", ==, actual);
    g_free (actual);
}

static void
test_when_single_character_and_no_trailing_punctuation ()
{
    char *actual;
    actual = eel_str_rtrim_punctuation ("t");
    g_assert_cmpstr ("t", ==, actual);
    g_free (actual);
}

static void
setup_test_suite ()
{
    g_test_add_func ("/rtrim-punctuation/1.0",
                     test_single_punctuation_character_removed);
    g_test_add_func ("/rtrim-punctuation/1.1",
                     test_tailing_space_is_removed);
    g_test_add_func ("/rtrim-punctuation/1.2",
                     test_multiple_punctuation_characters_removed);
    g_test_add_func ("/rtrim-punctuation/1.3",
                     test_multiple_punctuation_characters_removed_try_all_punctuation_characters);
    g_test_add_func ("/rtrim-punctuation/1.4",
                     test_punctuation_characters_removed_when_punctuation_in_middle_of_string);
    g_test_add_func ("/rtrim-punctuation/1.5",
                     test_punctuation_characters_removed_when_prefix_is_single_character);
    g_test_add_func ("/rtrim-punctuation/1.6",
                     test_punctuation_characters_removed_when_unicode_characters_are_used);

    g_test_add_func ("/rtrim-punctuation/2.0",
                     test_when_no_trailing_punctuation);
    g_test_add_func ("/rtrim-punctuation/2.1",
                     test_when_single_character_and_no_trailing_punctuation);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=747907");
    g_test_set_nonfatal_assertions ();

    setup_test_suite ();

    return g_test_run ();
}
