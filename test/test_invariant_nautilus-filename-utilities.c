#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

/* Import the actual production functions */
#include "src/nautilus-filename-utilities.c"

START_TEST(test_filename_utilities_buffer_overflow)
{
    /* Invariant: Buffer reads never exceed the declared length;
       oversized inputs must not cause out-of-bounds access */

    /* Generate oversized filenames */
    char *huge_2x = g_strnfill(512, 'A');   /* 2x typical NAME_MAX */
    char *huge_10x = g_strnfill(2560, 'B'); /* 10x typical NAME_MAX */
    char *boundary = g_strnfill(255, 'C');  /* exactly NAME_MAX */
    const char *valid = "document.txt";
    const char *with_ext = g_strconcat(huge_2x, ".txt", NULL);

    const char *payloads[] = {
        huge_10x,
        huge_2x,
        with_ext,
        boundary,
        valid
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* Test nautilus_filename_get_extension_offset - should not crash */
        const char *ext = nautilus_filename_get_extension_offset(payloads[i]);
        /* Result is either NULL or points within the original string */
        if (ext != NULL) {
            ck_assert(ext >= payloads[i]);
            ck_assert(ext <= payloads[i] + strlen(payloads[i]));
        }

        /* Test nautilus_filename_get_common_prefix - should not crash */
        GList *list = NULL;
        list = g_list_append(list, (gpointer)payloads[i]);
        list = g_list_append(list, (gpointer)valid);
        char *prefix = nautilus_filename_get_common_prefix(list, 4);
        /* prefix is either NULL or a valid allocated string */
        if (prefix != NULL) {
            ck_assert(strlen(prefix) <= strlen(payloads[i]));
            g_free(prefix);
        }
        g_list_free(list);
    }

    g_free(huge_2x);
    g_free(huge_10x);
    g_free(boundary);
    g_free((char *)with_ext);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_filename_utilities_buffer_overflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}