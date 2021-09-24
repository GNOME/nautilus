/* nautilus-file-operations.c - Nautilus file operations.
 *
 *  Copyright (C) 1999, 2000 Free Software Foundation
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2007 Red Hat, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Ettore Perazzoli <ettore@gnu.org>
 *           Pavel Cisler <pavel@eazel.com>
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

#include "nautilus-file-operations.h"

#include "nautilus-file-changes-queue.h"
#include "nautilus-lib-self-check-functions.h"

#include "nautilus-progress-info.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>

#include "nautilus-error-reporting.h"
#include "nautilus-operations-ui-manager.h"
#include "nautilus-file-changes-queue.h"
#include "nautilus-file-private.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file-undo-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-ui-utilities.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

/* TODO: TESTING!!! */

typedef struct
{
    GTimer *time;
    GtkWindow *parent_window;
    NautilusFileOperationsDBusData *dbus_data;
    guint inhibit_cookie;
    NautilusProgressInfo *progress;
    GCancellable *cancellable;
    GHashTable *skip_files;
    GHashTable *skip_readdir_error;
    NautilusFileUndoInfo *undo_info;
    gboolean skip_all_error;
    gboolean skip_all_conflict;
    gboolean merge_all;
    gboolean replace_all;
    gboolean delete_all;
} CommonJob;

typedef struct
{
    CommonJob common;
    gboolean is_move;
    GList *files;
    GFile *destination;
    GFile *fake_display_source;
    GHashTable *debuting_files;
    gchar *target_name;
    NautilusCopyCallback done_callback;
    gpointer done_callback_data;
} CopyMoveJob;

typedef struct
{
    CommonJob common;
    GList *files;
    gboolean try_trash;
    gboolean user_cancel;
    NautilusDeleteCallback done_callback;
    gpointer done_callback_data;
} DeleteJob;

typedef struct
{
    CommonJob common;
    GFile *dest_dir;
    char *filename;
    gboolean make_dir;
    GFile *src;
    char *src_data;
    int length;
    GFile *created_file;
    NautilusCreateCallback done_callback;
    gpointer done_callback_data;
} CreateJob;


typedef struct
{
    CommonJob common;
    GList *trash_dirs;
    gboolean should_confirm;
    NautilusOpCallback done_callback;
    gpointer done_callback_data;
} EmptyTrashJob;

typedef struct
{
    CommonJob common;
    GFile *file;
    gboolean interactive;
    NautilusOpCallback done_callback;
    gpointer done_callback_data;
} MarkTrustedJob;

typedef struct
{
    CommonJob common;
    GFile *file;
    NautilusOpCallback done_callback;
    gpointer done_callback_data;
    guint32 file_permissions;
    guint32 file_mask;
    guint32 dir_permissions;
    guint32 dir_mask;
} SetPermissionsJob;

typedef enum
{
    OP_KIND_COPY,
    OP_KIND_MOVE,
    OP_KIND_DELETE,
    OP_KIND_TRASH,
    OP_KIND_COMPRESS
} OpKind;

typedef struct
{
    int num_files_children;
    goffset num_bytes_children;
} SourceDirInfo;

typedef struct
{
    int num_files;
    goffset num_bytes;
    int num_files_since_progress;
    OpKind op;
    GHashTable *scanned_dirs_info;
} SourceInfo;

typedef struct
{
    int num_files;
    goffset num_bytes;
    OpKind op;
    guint64 last_report_time;
    int last_reported_files_left;

    /*
     * This is used when reporting progress for copy/move operations to not show
     * the remaining time. This is needed because some GVfs backends doesn't
     * report progress from those operations. Consequently it looks like that it
     * is hanged when the remaining time is not updated regularly. See:
     * https://gitlab.gnome.org/GNOME/nautilus/-/merge_requests/605
     */
    gboolean partial_progress;
} TransferInfo;

typedef struct
{
    CommonJob common;
    GList *source_files;
    GFile *destination_directory;
    GList *output_files;
    gboolean destination_decided;

    gdouble base_progress;

    guint64 archive_compressed_size;
    guint64 total_compressed_size;

    NautilusExtractCallback done_callback;
    gpointer done_callback_data;
} ExtractJob;

typedef struct
{
    CommonJob common;
    GList *source_files;
    GFile *output_file;

    AutoarFormat format;
    AutoarFilter filter;
    gchar *passphrase;

    guint64 total_size;
    guint total_files;

    gboolean success;

    NautilusCreateCallback done_callback;
    gpointer done_callback_data;
} CompressJob;

static void
source_info_clear (SourceInfo *source_info)
{
    if (source_info->scanned_dirs_info != NULL)
    {
        g_hash_table_unref (source_info->scanned_dirs_info);
    }
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (SourceInfo, source_info_clear)

#define SOURCE_INFO_INIT { 0 }
#define SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE 8
#define NSEC_PER_MICROSEC 1000
#define PROGRESS_NOTIFY_INTERVAL 100 * NSEC_PER_MICROSEC

#define MAXIMUM_DISPLAYED_FILE_NAME_LENGTH 50

#define IS_IO_ERROR(__error, KIND) (((__error)->domain == G_IO_ERROR && (__error)->code == G_IO_ERROR_ ## KIND))

#define CANCEL _("_Cancel")
#define SKIP _("_Skip")
#define SKIP_ALL _("S_kip All")
#define RETRY _("_Retry")
#define DELETE _("_Delete")
#define DELETE_ALL _("Delete _All")
#define REPLACE _("_Replace")
#define REPLACE_ALL _("Replace _All")
#define MERGE _("_Merge")
#define MERGE_ALL _("Merge _All")
#define COPY_FORCE _("Copy _Anyway")
#define EMPTY_TRASH _("Empty _Trash")

static gboolean
is_all_button_text (const char *button_text)
{
    g_assert (button_text != NULL);

    return !strcmp (button_text, SKIP_ALL) ||
           !strcmp (button_text, REPLACE_ALL) ||
           !strcmp (button_text, DELETE_ALL) ||
           !strcmp (button_text, MERGE_ALL);
}

static void scan_sources (GList      *files,
                          SourceInfo *source_info,
                          CommonJob  *job,
                          OpKind      kind);


static void empty_trash_thread_func (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable);

static void empty_trash_task_done (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data);

static char *query_fs_type (GFile        *file,
                            GCancellable *cancellable);

static void nautilus_file_operations_copy (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable);

static void nautilus_file_operations_move (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable);

/* keep in time with get_formatted_time ()
 *
 * This counts and outputs the number of “time units”
 * formatted and displayed by get_formatted_time ().
 * For instance, if get_formatted_time outputs “3 hours, 4 minutes”
 * it yields 7.
 */
static int
seconds_count_format_time_units (int seconds)
{
    int minutes;
    int hours;

    if (seconds < 0)
    {
        /* Just to make sure... */
        seconds = 0;
    }

    if (seconds < 60)
    {
        /* seconds */
        return seconds;
    }

    if (seconds < 60 * 60)
    {
        /* minutes */
        minutes = seconds / 60;
        return minutes;
    }

    hours = seconds / (60 * 60);

    if (seconds < 60 * 60 * 4)
    {
        /* minutes + hours */
        minutes = (seconds - hours * 60 * 60) / 60;
        return minutes + hours;
    }

    return hours;
}

static gchar *
get_formatted_time (int seconds)
{
    int minutes;
    int hours;
    gchar *res;

    if (seconds < 0)
    {
        /* Just to make sure... */
        seconds = 0;
    }

    if (seconds < 60)
    {
        return g_strdup_printf (ngettext ("%'d second", "%'d seconds", (int) seconds), (int) seconds);
    }

    if (seconds < 60 * 60)
    {
        minutes = seconds / 60;
        return g_strdup_printf (ngettext ("%'d minute", "%'d minutes", minutes), minutes);
    }

    hours = seconds / (60 * 60);

    if (seconds < 60 * 60 * 4)
    {
        gchar *h, *m;

        minutes = (seconds - hours * 60 * 60) / 60;

        h = g_strdup_printf (ngettext ("%'d hour", "%'d hours", hours), hours);
        m = g_strdup_printf (ngettext ("%'d minute", "%'d minutes", minutes), minutes);
        res = g_strconcat (h, ", ", m, NULL);
        g_free (h);
        g_free (m);
        return res;
    }

    return g_strdup_printf (ngettext ("%'d hour",
                                      "%'d hours",
                                      hours), hours);
}

static char *
shorten_utf8_string (const char *base,
                     int         reduce_by_num_bytes)
{
    int len;
    char *ret;
    const char *p;

    len = strlen (base);
    len -= reduce_by_num_bytes;

    if (len <= 0)
    {
        return NULL;
    }

    ret = g_new (char, len + 1);

    p = base;
    while (len)
    {
        char *next;
        next = g_utf8_next_char (p);
        if (next - p > len || *next == '\0')
        {
            break;
        }

        len -= next - p;
        p = next;
    }

    if (p - base == 0)
    {
        g_free (ret);
        return NULL;
    }
    else
    {
        memcpy (ret, base, p - base);
        ret[p - base] = '\0';
        return ret;
    }
}

/* Note that we have these two separate functions with separate format
 * strings for ease of localization.
 */

static char *
get_link_name (const char *name,
               int         count,
               int         max_length)
{
    const char *format;
    char *result;
    int unshortened_length;
    gboolean use_count;

    g_assert (name != NULL);

    if (count < 0)
    {
        g_warning ("bad count in get_link_name");
        count = 0;
    }

    if (count <= 2)
    {
        /* Handle special cases for low numbers.
         * Perhaps for some locales we will need to add more.
         */
        switch (count)
        {
            default:
            {
                g_assert_not_reached ();
                /* fall through */
            }

            case 0:
            {
                /* duplicate original file name */
                format = "%s";
            }
            break;

            case 1:
            {
                /* appended to new link file */
                format = _("Link to %s");
            }
            break;

            case 2:
            {
                /* appended to new link file */
                format = _("Another link to %s");
            }
            break;
        }

        use_count = FALSE;
    }
    else
    {
        /* Handle special cases for the first few numbers of each ten.
         * For locales where getting this exactly right is difficult,
         * these can just be made all the same as the general case below.
         */
        switch (count % 10)
        {
            case 1:
            {
                /* Localizers: Feel free to leave out the "st" suffix
                 * if there's no way to do that nicely for a
                 * particular language.
                 */
                format = _("%'dst link to %s");
            }
            break;

            case 2:
            {
                /* appended to new link file */
                format = _("%'dnd link to %s");
            }
            break;

            case 3:
            {
                /* appended to new link file */
                format = _("%'drd link to %s");
            }
            break;

            default:
            {
                /* appended to new link file */
                format = _("%'dth link to %s");
            }
            break;
        }

        use_count = TRUE;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    if (use_count)
    {
        result = g_strdup_printf (format, count, name);
    }
    else
    {
        result = g_strdup_printf (format, name);
    }

    if (max_length > 0 && (unshortened_length = strlen (result)) > max_length)
    {
        char *new_name;

        new_name = shorten_utf8_string (name, unshortened_length - max_length);
        if (new_name)
        {
            g_free (result);

            if (use_count)
            {
                result = g_strdup_printf (format, count, new_name);
            }
            else
            {
                result = g_strdup_printf (format, new_name);
            }

            g_assert (strlen (result) <= max_length);
            g_free (new_name);
        }
    }
#pragma GCC diagnostic pop
    return result;
}


/* Localizers:
 * Feel free to leave out the st, nd, rd and th suffix or
 * make some or all of them match.
 */

/* localizers: tag used to detect the first copy of a file */
static const char untranslated_copy_duplicate_tag[] = N_(" (copy)");
/* localizers: tag used to detect the second copy of a file */
static const char untranslated_another_copy_duplicate_tag[] = N_(" (another copy)");

/* localizers: tag used to detect the x11th copy of a file */
static const char untranslated_x11th_copy_duplicate_tag[] = N_("th copy)");
/* localizers: tag used to detect the x12th copy of a file */
static const char untranslated_x12th_copy_duplicate_tag[] = N_("th copy)");
/* localizers: tag used to detect the x13th copy of a file */
static const char untranslated_x13th_copy_duplicate_tag[] = N_("th copy)");

/* localizers: tag used to detect the x1st copy of a file */
static const char untranslated_st_copy_duplicate_tag[] = N_("st copy)");
/* localizers: tag used to detect the x2nd copy of a file */
static const char untranslated_nd_copy_duplicate_tag[] = N_("nd copy)");
/* localizers: tag used to detect the x3rd copy of a file */
static const char untranslated_rd_copy_duplicate_tag[] = N_("rd copy)");

/* localizers: tag used to detect the xxth copy of a file */
static const char untranslated_th_copy_duplicate_tag[] = N_("th copy)");

#define COPY_DUPLICATE_TAG _(untranslated_copy_duplicate_tag)
#define ANOTHER_COPY_DUPLICATE_TAG _(untranslated_another_copy_duplicate_tag)
#define X11TH_COPY_DUPLICATE_TAG _(untranslated_x11th_copy_duplicate_tag)
#define X12TH_COPY_DUPLICATE_TAG _(untranslated_x12th_copy_duplicate_tag)
#define X13TH_COPY_DUPLICATE_TAG _(untranslated_x13th_copy_duplicate_tag)

#define ST_COPY_DUPLICATE_TAG _(untranslated_st_copy_duplicate_tag)
#define ND_COPY_DUPLICATE_TAG _(untranslated_nd_copy_duplicate_tag)
#define RD_COPY_DUPLICATE_TAG _(untranslated_rd_copy_duplicate_tag)
#define TH_COPY_DUPLICATE_TAG _(untranslated_th_copy_duplicate_tag)

/* localizers: appended to first file copy */
static const char untranslated_first_copy_duplicate_format[] = N_("%s (copy)%s");
/* localizers: appended to second file copy */
static const char untranslated_second_copy_duplicate_format[] = N_("%s (another copy)%s");

/* localizers: appended to x11th file copy */
static const char untranslated_x11th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");
/* localizers: appended to x12th file copy */
static const char untranslated_x12th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");
/* localizers: appended to x13th file copy */
static const char untranslated_x13th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");

/* localizers: if in your language there's no difference between 1st, 2nd, 3rd and nth
 * plurals, you can leave the st, nd, rd suffixes out and just make all the translated
 * strings look like "%s (copy %'d)%s".
 */

/* localizers: appended to x1st file copy */
static const char untranslated_st_copy_duplicate_format[] = N_("%s (%'dst copy)%s");
/* localizers: appended to x2nd file copy */
static const char untranslated_nd_copy_duplicate_format[] = N_("%s (%'dnd copy)%s");
/* localizers: appended to x3rd file copy */
static const char untranslated_rd_copy_duplicate_format[] = N_("%s (%'drd copy)%s");
/* localizers: appended to xxth file copy */
static const char untranslated_th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");

#define FIRST_COPY_DUPLICATE_FORMAT _(untranslated_first_copy_duplicate_format)
#define SECOND_COPY_DUPLICATE_FORMAT _(untranslated_second_copy_duplicate_format)
#define X11TH_COPY_DUPLICATE_FORMAT _(untranslated_x11th_copy_duplicate_format)
#define X12TH_COPY_DUPLICATE_FORMAT _(untranslated_x12th_copy_duplicate_format)
#define X13TH_COPY_DUPLICATE_FORMAT _(untranslated_x13th_copy_duplicate_format)

#define ST_COPY_DUPLICATE_FORMAT _(untranslated_st_copy_duplicate_format)
#define ND_COPY_DUPLICATE_FORMAT _(untranslated_nd_copy_duplicate_format)
#define RD_COPY_DUPLICATE_FORMAT _(untranslated_rd_copy_duplicate_format)
#define TH_COPY_DUPLICATE_FORMAT _(untranslated_th_copy_duplicate_format)

static char *
extract_string_until (const char *original,
                      const char *until_substring)
{
    char *result;

    g_assert ((int) strlen (original) >= until_substring - original);
    g_assert (until_substring - original >= 0);

    result = g_malloc (until_substring - original + 1);
    strncpy (result, original, until_substring - original);
    result[until_substring - original] = '\0';

    return result;
}

/* Dismantle a file name, separating the base name, the file suffix and removing any
 * (xxxcopy), etc. string. Figure out the count that corresponds to the given
 * (xxxcopy) substring.
 */
static void
parse_previous_duplicate_name (const char  *name,
                               char       **name_base,
                               const char **suffix,
                               int         *count,
                               gboolean     ignore_extension)
{
    const char *tag;

    g_assert (name[0] != '\0');

    *suffix = eel_filename_get_extension_offset (name);

    if (*suffix == NULL || (*suffix)[1] == '\0')
    {
        /* no suffix */
        *suffix = "";
    }

    tag = strstr (name, COPY_DUPLICATE_TAG);
    if (tag != NULL)
    {
        if (tag > *suffix)
        {
            /* handle case "foo. (copy)" */
            *suffix = "";
        }
        *name_base = extract_string_until (name, tag);
        *count = 1;
        return;
    }


    tag = strstr (name, ANOTHER_COPY_DUPLICATE_TAG);
    if (tag != NULL)
    {
        if (tag > *suffix)
        {
            /* handle case "foo. (another copy)" */
            *suffix = "";
        }
        *name_base = extract_string_until (name, tag);
        *count = 2;
        return;
    }


    /* Check to see if we got one of st, nd, rd, th. */
    tag = strstr (name, X11TH_COPY_DUPLICATE_TAG);

    if (tag == NULL)
    {
        tag = strstr (name, X12TH_COPY_DUPLICATE_TAG);
    }
    if (tag == NULL)
    {
        tag = strstr (name, X13TH_COPY_DUPLICATE_TAG);
    }

    if (tag == NULL)
    {
        tag = strstr (name, ST_COPY_DUPLICATE_TAG);
    }
    if (tag == NULL)
    {
        tag = strstr (name, ND_COPY_DUPLICATE_TAG);
    }
    if (tag == NULL)
    {
        tag = strstr (name, RD_COPY_DUPLICATE_TAG);
    }
    if (tag == NULL)
    {
        tag = strstr (name, TH_COPY_DUPLICATE_TAG);
    }

    /* If we got one of st, nd, rd, th, fish out the duplicate number. */
    if (tag != NULL)
    {
        /* localizers: opening parentheses to match the "th copy)" string */
        tag = strstr (name, _(" ("));
        if (tag != NULL)
        {
            if (tag > *suffix)
            {
                /* handle case "foo. (22nd copy)" */
                *suffix = "";
            }
            *name_base = extract_string_until (name, tag);
            /* localizers: opening parentheses of the "th copy)" string */
            if (sscanf (tag, _(" (%'d"), count) == 1)
            {
                if (*count < 1 || *count > 1000000)
                {
                    /* keep the count within a reasonable range */
                    *count = 0;
                }
                return;
            }
            *count = 0;
            return;
        }
    }


    *count = 0;
    /* ignore_extension was not used before to let above code handle case "dir (copy).dir" for directories */
    if (**suffix != '\0' && !ignore_extension)
    {
        *name_base = extract_string_until (name, *suffix);
    }
    else
    {
        /* making sure extension is ignored in directories */
        *suffix = "";
        *name_base = g_strdup (name);
    }
}

static char *
make_next_duplicate_name (const char *base,
                          const char *suffix,
                          int         count,
                          int         max_length)
{
    const char *format;
    char *result;
    int unshortened_length;
    gboolean use_count;

    if (count < 1)
    {
        g_warning ("bad count %d in get_duplicate_name", count);
        count = 1;
    }

    if (count <= 2)
    {
        /* Handle special cases for low numbers.
         * Perhaps for some locales we will need to add more.
         */
        switch (count)
        {
            default:
            {
                g_assert_not_reached ();
                /* fall through */
            }

            case 1:
            {
                format = FIRST_COPY_DUPLICATE_FORMAT;
            }
            break;

            case 2:
            {
                format = SECOND_COPY_DUPLICATE_FORMAT;
            }
            break;
        }

        use_count = FALSE;
    }
    else
    {
        /* Handle special cases for the first few numbers of each ten.
         * For locales where getting this exactly right is difficult,
         * these can just be made all the same as the general case below.
         */

        /* Handle special cases for x11th - x20th.
         */
        switch (count % 100)
        {
            case 11:
            {
                format = X11TH_COPY_DUPLICATE_FORMAT;
            }
            break;

            case 12:
            {
                format = X12TH_COPY_DUPLICATE_FORMAT;
            }
            break;

            case 13:
            {
                format = X13TH_COPY_DUPLICATE_FORMAT;
            }
            break;

            default:
            {
                format = NULL;
            }
            break;
        }

        if (format == NULL)
        {
            switch (count % 10)
            {
                case 1:
                {
                    format = ST_COPY_DUPLICATE_FORMAT;
                }
                break;

                case 2:
                {
                    format = ND_COPY_DUPLICATE_FORMAT;
                }
                break;

                case 3:
                {
                    format = RD_COPY_DUPLICATE_FORMAT;
                }
                break;

                default:
                {
                    /* The general case. */
                    format = TH_COPY_DUPLICATE_FORMAT;
                }
                break;
            }
        }

        use_count = TRUE;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    if (use_count)
    {
        result = g_strdup_printf (format, base, count, suffix);
    }
    else
    {
        result = g_strdup_printf (format, base, suffix);
    }

    if (max_length > 0 && (unshortened_length = strlen (result)) > max_length)
    {
        char *new_base;

        new_base = shorten_utf8_string (base, unshortened_length - max_length);
        if (new_base)
        {
            g_free (result);

            if (use_count)
            {
                result = g_strdup_printf (format, new_base, count, suffix);
            }
            else
            {
                result = g_strdup_printf (format, new_base, suffix);
            }

            g_assert (strlen (result) <= max_length);
            g_free (new_base);
        }
    }
#pragma GCC diagnostic pop

    return result;
}

static char *
get_duplicate_name (const char *name,
                    int         count_increment,
                    int         max_length,
                    gboolean    ignore_extension)
{
    char *result;
    char *name_base;
    const char *suffix;
    int count;

    parse_previous_duplicate_name (name, &name_base, &suffix, &count, ignore_extension);
    result = make_next_duplicate_name (name_base, suffix, count + count_increment, max_length);

    g_free (name_base);

    return result;
}

static gboolean
has_invalid_xml_char (char *str)
{
    gunichar c;

    while (*str != 0)
    {
        c = g_utf8_get_char (str);
        /* characters XML permits */
        if (!(c == 0x9 ||
              c == 0xA ||
              c == 0xD ||
              (c >= 0x20 && c <= 0xD7FF) ||
              (c >= 0xE000 && c <= 0xFFFD) ||
              (c >= 0x10000 && c <= 0x10FFFF)))
        {
            return TRUE;
        }
        str = g_utf8_next_char (str);
    }
    return FALSE;
}

static gchar *
get_basename (GFile *file)
{
    GFileInfo *info;
    gchar *name, *basename, *tmp;
    GMount *mount;

    if ((mount = nautilus_get_mounted_mount_for_root (file)) != NULL)
    {
        name = g_mount_get_name (mount);
        g_object_unref (mount);
    }
    else
    {
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  0,
                                  g_cancellable_get_current (),
                                  NULL);
        name = NULL;
        if (info)
        {
            name = g_strdup (g_file_info_get_display_name (info));
            g_object_unref (info);
        }
    }

    if (name == NULL)
    {
        basename = g_file_get_basename (file);
        if (g_utf8_validate (basename, -1, NULL))
        {
            name = basename;
        }
        else
        {
            name = g_uri_escape_string (basename, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
            g_free (basename);
        }
    }

    /* Some chars can't be put in the markup we use for the dialogs... */
    if (has_invalid_xml_char (name))
    {
        tmp = name;
        name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
        g_free (tmp);
    }

    /* Finally, if the string is too long, truncate it. */
    if (name != NULL)
    {
        tmp = name;
        name = eel_str_middle_truncate (tmp, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
        g_free (tmp);
    }

    return name;
}

static gchar *
get_truncated_parse_name (GFile *file)
{
    g_autofree gchar *parse_name = NULL;

    g_assert (G_IS_FILE (file));

    parse_name = g_file_get_parse_name (file);

    return eel_str_middle_truncate (parse_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
}

#define op_job_new(__type, parent_window, dbus_data) ((__type *) (init_common (sizeof (__type), parent_window, dbus_data)))

static gpointer
init_common (gsize                           job_size,
             GtkWindow                      *parent_window,
             NautilusFileOperationsDBusData *dbus_data)
{
    CommonJob *common;

    common = g_malloc0 (job_size);

    if (parent_window)
    {
        common->parent_window = parent_window;
        g_object_add_weak_pointer (G_OBJECT (common->parent_window),
                                   (gpointer *) &common->parent_window);
    }

    if (dbus_data)
    {
        common->dbus_data = nautilus_file_operations_dbus_data_ref (dbus_data);
    }

    common->progress = nautilus_progress_info_new ();
    common->cancellable = nautilus_progress_info_get_cancellable (common->progress);
    common->time = g_timer_new ();
    common->inhibit_cookie = 0;

    return common;
}

static void
finalize_common (CommonJob *common)
{
    nautilus_progress_info_finish (common->progress);

    if (common->inhibit_cookie != 0)
    {
        gtk_application_uninhibit (GTK_APPLICATION (g_application_get_default ()),
                                   common->inhibit_cookie);
    }

    common->inhibit_cookie = 0;
    g_timer_destroy (common->time);

    if (common->parent_window)
    {
        g_object_remove_weak_pointer (G_OBJECT (common->parent_window),
                                      (gpointer *) &common->parent_window);
    }

    if (common->dbus_data)
    {
        nautilus_file_operations_dbus_data_unref (common->dbus_data);
    }

    if (common->skip_files)
    {
        g_hash_table_destroy (common->skip_files);
    }
    if (common->skip_readdir_error)
    {
        g_hash_table_destroy (common->skip_readdir_error);
    }

    if (common->undo_info != NULL)
    {
        nautilus_file_undo_manager_set_action (common->undo_info);
        g_object_unref (common->undo_info);
    }

    g_object_unref (common->progress);
    g_object_unref (common->cancellable);
    g_free (common);
}

static void
skip_file (CommonJob *common,
           GFile     *file)
{
    if (common->skip_files == NULL)
    {
        common->skip_files =
            g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
    }

    g_hash_table_insert (common->skip_files, g_object_ref (file), file);
}

static void
skip_readdir_error (CommonJob *common,
                    GFile     *dir)
{
    if (common->skip_readdir_error == NULL)
    {
        common->skip_readdir_error =
            g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
    }

    g_hash_table_insert (common->skip_readdir_error, g_object_ref (dir), dir);
}

static gboolean
should_skip_file (CommonJob *common,
                  GFile     *file)
{
    if (common->skip_files != NULL)
    {
        return g_hash_table_lookup (common->skip_files, file) != NULL;
    }
    return FALSE;
}

static gboolean
should_skip_readdir_error (CommonJob *common,
                           GFile     *dir)
{
    if (common->skip_readdir_error != NULL)
    {
        return g_hash_table_lookup (common->skip_readdir_error, dir) != NULL;
    }
    return FALSE;
}

static gboolean
can_delete_without_confirm (GFile *file)
{
    /* In the case of testing, we want to be able to delete
     * without asking for confirmation from the user.
     */
    if (g_file_has_uri_scheme (file, "burn") ||
        g_file_has_uri_scheme (file, "recent") ||
        !g_strcmp0 (g_getenv ("RUNNING_TESTS"), "TRUE"))
    {
        return TRUE;
    }

    return FALSE;
}

static gboolean
can_delete_files_without_confirm (GList *files)
{
    g_assert (files != NULL);

    while (files != NULL)
    {
        if (!can_delete_without_confirm (files->data))
        {
            return FALSE;
        }

        files = files->next;
    }

    return TRUE;
}

typedef struct
{
    GtkWindow **parent_window;
    NautilusFileOperationsDBusData *dbus_data;
    gboolean ignore_close_box;
    GtkMessageType message_type;
    const char *primary_text;
    const char *secondary_text;
    const char *details_text;
    const char **button_titles;
    gboolean show_all;
    int result;
    /* Dialogs are ran from operation threads, which need to be blocked until
     * the user gives a valid response
     */
    gboolean completed;
    GMutex mutex;
    GCond cond;
} RunSimpleDialogData;

static void
set_transient_for (GdkWindow  *child_window,
                   const char *parent_handle)
{
    GdkDisplay *display;
    const char *prefix;

    display = gdk_window_get_display (child_window);

#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (display))
    {
        prefix = "x11:";

        if (g_str_has_prefix (parent_handle, prefix))
        {
            const char *handle;
            GdkWindow *window;

            handle = parent_handle + strlen (prefix);
            window = gdk_x11_window_foreign_new_for_display (display, strtol (handle, NULL, 16));

            if (window != NULL)
            {
                gdk_window_set_transient_for (child_window, window);
                g_object_unref (window);
            }
        }
    }
#endif

#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (display))
    {
        prefix = "wayland:";

        if (g_str_has_prefix (parent_handle, prefix))
        {
            const char *handle;

            handle = parent_handle + strlen (prefix);

            gdk_wayland_window_set_transient_for_exported (child_window, (char *) handle);
        }
    }
#endif
}

static void
dialog_realize_cb (GtkWidget *widget,
                   gpointer   user_data)
{
    NautilusFileOperationsDBusData *dbus_data = user_data;
    const char *parent_handle;

    parent_handle = nautilus_file_operations_dbus_data_get_parent_handle (dbus_data);
    set_transient_for (gtk_widget_get_window (widget), parent_handle);
}

static gboolean
do_run_simple_dialog (gpointer _data)
{
    RunSimpleDialogData *data = _data;
    const char *button_title;
    GtkWidget *dialog;
    GtkWidget *button;
    int result;
    int response_id;

    g_mutex_lock (&data->mutex);

    /* Create the dialog. */
    dialog = gtk_message_dialog_new (*data->parent_window,
                                     0,
                                     data->message_type,
                                     GTK_BUTTONS_NONE,
                                     NULL);

    g_object_set (dialog,
                  "text", data->primary_text,
                  "secondary-text", data->secondary_text,
                  NULL);

    for (response_id = 0;
         data->button_titles[response_id] != NULL;
         response_id++)
    {
        button_title = data->button_titles[response_id];
        if (!data->show_all && is_all_button_text (button_title))
        {
            continue;
        }

        button = gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, response_id);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), response_id);

        if (g_strcmp0 (button_title, DELETE) == 0 ||
            g_strcmp0 (button_title, EMPTY_TRASH) == 0 ||
            g_strcmp0 (button_title, DELETE_ALL) == 0)
        {
            gtk_style_context_add_class (gtk_widget_get_style_context (button),
                                         "destructive-action");
        }
    }

    if (data->details_text)
    {
        GtkWidget *content_area, *label;
        content_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));

        label = gtk_label_new (data->details_text);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_selectable (GTK_LABEL (label), TRUE);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        /* Ideally, we shouldn’t do this.
         *
         * Refer to https://gitlab.gnome.org/GNOME/nautilus/merge_requests/94
         * and https://gitlab.gnome.org/GNOME/nautilus/issues/270.
         */
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars (GTK_LABEL (label),
                                       MAXIMUM_DISPLAYED_ERROR_MESSAGE_LENGTH);

        gtk_container_add (GTK_CONTAINER (content_area), label);

        gtk_widget_show (label);
    }

    if (data->dbus_data != NULL)
    {
        guint32 timestamp;

        timestamp = nautilus_file_operations_dbus_data_get_timestamp (data->dbus_data);

        /* Assuming this is used for desktop implementations, we want the
         * dialog to be centered on the screen rather than the parent window,
         * which could extend to all monitors. This is the case for
         * gnome-flashback.
         */
        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

        if (nautilus_file_operations_dbus_data_get_parent_handle (data->dbus_data) != NULL)
        {
            g_signal_connect (dialog, "realize", G_CALLBACK (dialog_realize_cb), data->dbus_data);
        }

        if (timestamp != 0)
        {
            gtk_window_present_with_time (GTK_WINDOW (dialog), timestamp);
        }
    }

    /* Run it. */
    result = gtk_dialog_run (GTK_DIALOG (dialog));

    while ((result == GTK_RESPONSE_NONE || result == GTK_RESPONSE_DELETE_EVENT) && data->ignore_close_box)
    {
        result = gtk_dialog_run (GTK_DIALOG (dialog));
    }

    gtk_widget_destroy (dialog);

    data->result = result;
    data->completed = TRUE;

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);

    return FALSE;
}

/* NOTE: This frees the primary / secondary strings, in order to
 *  avoid doing that everywhere. So, make sure they are strduped */

static int
run_simple_dialog_va (CommonJob      *job,
                      gboolean        ignore_close_box,
                      GtkMessageType  message_type,
                      char           *primary_text,
                      char           *secondary_text,
                      const char     *details_text,
                      gboolean        show_all,
                      va_list         varargs)
{
    RunSimpleDialogData *data;
    int res;
    const char *button_title;
    GPtrArray *ptr_array;

    g_timer_stop (job->time);

    data = g_new0 (RunSimpleDialogData, 1);
    data->parent_window = &job->parent_window;
    data->dbus_data = job->dbus_data;
    data->ignore_close_box = ignore_close_box;
    data->message_type = message_type;
    data->primary_text = primary_text;
    data->secondary_text = secondary_text;
    data->details_text = details_text;
    data->show_all = show_all;
    data->completed = FALSE;
    g_mutex_init (&data->mutex);
    g_cond_init (&data->cond);

    ptr_array = g_ptr_array_new ();
    while ((button_title = va_arg (varargs, const char *)) != NULL)
    {
        g_ptr_array_add (ptr_array, (char *) button_title);
    }
    g_ptr_array_add (ptr_array, NULL);
    data->button_titles = (const char **) g_ptr_array_free (ptr_array, FALSE);

    nautilus_progress_info_pause (job->progress);

    g_mutex_lock (&data->mutex);

    g_main_context_invoke (NULL,
                           do_run_simple_dialog,
                           data);

    while (!data->completed)
    {
        g_cond_wait (&data->cond, &data->mutex);
    }

    nautilus_progress_info_resume (job->progress);
    res = data->result;

    g_mutex_unlock (&data->mutex);
    g_mutex_clear (&data->mutex);
    g_cond_clear (&data->cond);

    g_free (data->button_titles);
    g_free (data);

    g_timer_continue (job->time);

    g_free (primary_text);
    g_free (secondary_text);

    return res;
}

#if 0 /* Not used at the moment */
static int
run_simple_dialog (CommonJob     *job,
                   gboolean       ignore_close_box,
                   GtkMessageType message_type,
                   char          *primary_text,
                   char          *secondary_text,
                   const char    *details_text,
                   ...)
{
    va_list varargs;
    int res;

    va_start (varargs, details_text);
    res = run_simple_dialog_va (job,
                                ignore_close_box,
                                message_type,
                                primary_text,
                                secondary_text,
                                details_text,
                                varargs);
    va_end (varargs);
    return res;
}
#endif

static int
run_error (CommonJob  *job,
           char       *primary_text,
           char       *secondary_text,
           const char *details_text,
           gboolean    show_all,
           ...)
{
    va_list varargs;
    int res;

    va_start (varargs, show_all);
    res = run_simple_dialog_va (job,
                                FALSE,
                                GTK_MESSAGE_ERROR,
                                primary_text,
                                secondary_text,
                                details_text,
                                show_all,
                                varargs);
    va_end (varargs);
    return res;
}

static int
run_warning (CommonJob  *job,
             char       *primary_text,
             char       *secondary_text,
             const char *details_text,
             gboolean    show_all,
             ...)
{
    va_list varargs;
    int res;

    va_start (varargs, show_all);
    res = run_simple_dialog_va (job,
                                FALSE,
                                GTK_MESSAGE_WARNING,
                                primary_text,
                                secondary_text,
                                details_text,
                                show_all,
                                varargs);
    va_end (varargs);
    return res;
}

static int
run_question (CommonJob  *job,
              char       *primary_text,
              char       *secondary_text,
              const char *details_text,
              gboolean    show_all,
              ...)
{
    va_list varargs;
    int res;

    va_start (varargs, show_all);
    res = run_simple_dialog_va (job,
                                FALSE,
                                GTK_MESSAGE_QUESTION,
                                primary_text,
                                secondary_text,
                                details_text,
                                show_all,
                                varargs);
    va_end (varargs);
    return res;
}

static int
run_cancel_or_skip_warning (CommonJob  *job,
                            char       *primary_text,
                            char       *secondary_text,
                            const char *details_text,
                            int         total_operations,
                            int         operations_remaining)
{
    int response;

    if (total_operations == 1)
    {
        response = run_warning (job,
                                primary_text,
                                secondary_text,
                                details_text,
                                FALSE,
                                CANCEL,
                                NULL);
    }
    else
    {
        response = run_warning (job,
                                primary_text,
                                secondary_text,
                                details_text,
                                operations_remaining > 1,
                                CANCEL, SKIP_ALL, SKIP,
                                NULL);
    }

    return response;
}

static void
inhibit_power_manager (CommonJob  *job,
                       const char *message)
{
    job->inhibit_cookie = gtk_application_inhibit (GTK_APPLICATION (g_application_get_default ()),
                                                   GTK_WINDOW (job->parent_window),
                                                   GTK_APPLICATION_INHIBIT_LOGOUT |
                                                   GTK_APPLICATION_INHIBIT_SUSPEND,
                                                   message);
}

static void
abort_job (CommonJob *job)
{
    /* destroy the undo action data too */
    g_clear_object (&job->undo_info);

    g_cancellable_cancel (job->cancellable);
}

static gboolean
job_aborted (CommonJob *job)
{
    return g_cancellable_is_cancelled (job->cancellable);
}

static gboolean
confirm_delete_from_trash (CommonJob *job,
                           GList     *files)
{
    char *prompt;
    int file_count;
    int response;

    file_count = g_list_length (files);
    g_assert (file_count > 0);

    if (file_count == 1)
    {
        g_autofree gchar *basename = NULL;

        basename = get_basename (files->data);
        prompt = g_strdup_printf (_("Are you sure you want to permanently delete “%s” "
                                    "from the trash?"), basename);
    }
    else
    {
        prompt = g_strdup_printf (ngettext ("Are you sure you want to permanently delete "
                                            "the %'d selected item from the trash?",
                                            "Are you sure you want to permanently delete "
                                            "the %'d selected items from the trash?",
                                            file_count),
                                  file_count);
    }

    response = run_warning (job,
                            prompt,
                            g_strdup (_("If you delete an item, it will be permanently lost.")),
                            NULL,
                            FALSE,
                            CANCEL, DELETE,
                            NULL);

    return (response == 1);
}

static gboolean
confirm_empty_trash (CommonJob *job)
{
    char *prompt;
    int response;

    prompt = g_strdup (_("Empty all items from Trash?"));

    response = run_warning (job,
                            prompt,
                            g_strdup (_("All items in the Trash will be permanently deleted.")),
                            NULL,
                            FALSE,
                            CANCEL, EMPTY_TRASH,
                            NULL);

    return (response == 1);
}

static gboolean
confirm_delete_directly (CommonJob *job,
                         GList     *files)
{
    char *prompt;
    int file_count;
    int response;

    file_count = g_list_length (files);
    g_assert (file_count > 0);

    if (can_delete_files_without_confirm (files))
    {
        return TRUE;
    }

    if (file_count == 1)
    {
        g_autofree gchar *basename = NULL;

        basename = get_basename (files->data);
        prompt = g_strdup_printf (_("Are you sure you want to permanently delete “%s”?"),
                                  basename);
    }
    else
    {
        prompt = g_strdup_printf (ngettext ("Are you sure you want to permanently delete "
                                            "the %'d selected item?",
                                            "Are you sure you want to permanently delete "
                                            "the %'d selected items?", file_count),
                                  file_count);
    }

    response = run_warning (job,
                            prompt,
                            g_strdup (_("If you delete an item, it will be permanently lost.")),
                            NULL,
                            FALSE,
                            CANCEL, DELETE,
                            NULL);

    return response == 1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
report_delete_progress (CommonJob    *job,
                        SourceInfo   *source_info,
                        TransferInfo *transfer_info)
{
    int files_left;
    double elapsed, transfer_rate;
    int remaining_time;
    gint64 now;
    char *details;
    char *status;
    DeleteJob *delete_job;

    delete_job = (DeleteJob *) job;
    now = g_get_monotonic_time ();
    files_left = source_info->num_files - transfer_info->num_files;

    /* Races and whatnot could cause this to be negative... */
    if (files_left < 0)
    {
        files_left = 0;
    }

    /* If the number of files left is 0, we want to update the status without
     * considering this time, since we want to change the status to completed
     * and probably we won't get more calls to this function */
    if (transfer_info->last_report_time != 0 &&
        ABS ((gint64) (transfer_info->last_report_time - now)) < 100 * NSEC_PER_MICROSEC &&
        files_left > 0)
    {
        return;
    }

    transfer_info->last_report_time = now;

    if (source_info->num_files == 1)
    {
        g_autofree gchar *basename = NULL;

        if (files_left == 0)
        {
            status = _("Deleted “%s”");
        }
        else
        {
            status = _("Deleting “%s”");
        }

        basename = get_basename (G_FILE (delete_job->files->data));
        nautilus_progress_info_take_status (job->progress,
                                            g_strdup_printf (status, basename));
    }
    else
    {
        if (files_left == 0)
        {
            status = ngettext ("Deleted %'d file",
                               "Deleted %'d files",
                               source_info->num_files);
        }
        else
        {
            status = ngettext ("Deleting %'d file",
                               "Deleting %'d files",
                               source_info->num_files);
        }
        nautilus_progress_info_take_status (job->progress,
                                            g_strdup_printf (status,
                                                             source_info->num_files));
    }

    elapsed = g_timer_elapsed (job->time, NULL);
    transfer_rate = 0;
    remaining_time = INT_MAX;
    if (elapsed > 0)
    {
        transfer_rate = transfer_info->num_files / elapsed;
        if (transfer_rate > 0)
        {
            remaining_time = (source_info->num_files - transfer_info->num_files) / transfer_rate;
        }
    }

    if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE ||
        transfer_rate == 0)
    {
        if (files_left > 0)
        {
            /* To translators: %'d is the number of files completed for the operation,
             * so it will be something like 2/14. */
            details = g_strdup_printf (_("%'d / %'d"),
                                       transfer_info->num_files + 1,
                                       source_info->num_files);
        }
        else
        {
            /* To translators: %'d is the number of files completed for the operation,
             * so it will be something like 2/14. */
            details = g_strdup_printf (_("%'d / %'d"),
                                       transfer_info->num_files,
                                       source_info->num_files);
        }
    }
    else
    {
        if (files_left > 0)
        {
            gchar *time_left_message;
            gchar *files_per_second_message;
            gchar *concat_detail;
            g_autofree gchar *formatted_time = NULL;

            /* To translators: %s will expand to a time duration like "2 minutes".
             * So the whole thing will be something like "1 / 5 -- 2 hours left (4 files/sec)"
             *
             * The singular/plural form will be used depending on the remaining time (i.e. the %s argument).
             */
            time_left_message = ngettext ("%'d / %'d \xE2\x80\x94 %s left",
                                          "%'d / %'d \xE2\x80\x94 %s left",
                                          seconds_count_format_time_units (remaining_time));
            transfer_rate += 0.5;
            files_per_second_message = ngettext ("(%d file/sec)",
                                                 "(%d files/sec)",
                                                 (int) transfer_rate);
            concat_detail = g_strconcat (time_left_message, " ", files_per_second_message, NULL);

            formatted_time = get_formatted_time (remaining_time);
            details = g_strdup_printf (concat_detail,
                                       transfer_info->num_files + 1, source_info->num_files,
                                       formatted_time,
                                       (int) transfer_rate);

            g_free (concat_detail);
        }
        else
        {
            /* To translators: %'d is the number of files completed for the operation,
             * so it will be something like 2/14. */
            details = g_strdup_printf (_("%'d / %'d"),
                                       transfer_info->num_files,
                                       source_info->num_files);
        }
    }
    nautilus_progress_info_take_details (job->progress, details);

    if (elapsed > SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE)
    {
        nautilus_progress_info_set_remaining_time (job->progress,
                                                   remaining_time);
        nautilus_progress_info_set_elapsed_time (job->progress,
                                                 elapsed);
    }

    if (source_info->num_files != 0)
    {
        nautilus_progress_info_set_progress (job->progress, transfer_info->num_files, source_info->num_files);
    }
}
#pragma GCC diagnostic pop

typedef void (*DeleteCallback) (GFile   *file,
                                GError  *error,
                                gpointer callback_data);

static gboolean
delete_file_recursively (GFile          *file,
                         GCancellable   *cancellable,
                         DeleteCallback  callback,
                         gpointer        callback_data)
{
    gboolean success;
    g_autoptr (GError) error = NULL;

    do
    {
        g_autoptr (GFileEnumerator) enumerator = NULL;

        success = g_file_delete (file, cancellable, &error);
        if (success ||
            !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_EMPTY))
        {
            break;
        }

        g_clear_error (&error);

        enumerator = g_file_enumerate_children (file,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                G_FILE_QUERY_INFO_NONE,
                                                cancellable, &error);

        if (enumerator)
        {
            GFileInfo *info;

            success = TRUE;

            info = g_file_enumerator_next_file (enumerator,
                                                cancellable,
                                                &error);

            while (info != NULL)
            {
                g_autoptr (GFile) child = NULL;

                child = g_file_enumerator_get_child (enumerator, info);

                success = success && delete_file_recursively (child,
                                                              cancellable,
                                                              callback,
                                                              callback_data);

                g_object_unref (info);

                info = g_file_enumerator_next_file (enumerator,
                                                    cancellable,
                                                    &error);
            }
        }

        if (error != NULL)
        {
            success = FALSE;
        }
    }
    while (success);

    if (callback)
    {
        callback (file, error, callback_data);
    }

    return success;
}

typedef struct
{
    CommonJob *job;
    SourceInfo *source_info;
    TransferInfo *transfer_info;
} DeleteData;

static void
file_deleted_callback (GFile    *file,
                       GError   *error,
                       gpointer  callback_data)
{
    DeleteData *data = callback_data;
    CommonJob *job;
    SourceInfo *source_info;
    TransferInfo *transfer_info;
    GFileType file_type;
    char *primary;
    char *secondary;
    char *details = NULL;
    int response;
    g_autofree gchar *basename = NULL;

    job = data->job;
    source_info = data->source_info;
    transfer_info = data->transfer_info;

    data->transfer_info->num_files++;

    if (error == NULL)
    {
        nautilus_file_changes_queue_file_removed (file);
        report_delete_progress (data->job, data->source_info, data->transfer_info);

        return;
    }

    if (job_aborted (job) ||
        job->skip_all_error ||
        should_skip_file (job, file) ||
        should_skip_readdir_error (job, file))
    {
        return;
    }

    primary = g_strdup (_("Error while deleting."));

    file_type = g_file_query_file_type (file,
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        job->cancellable);

    basename = get_basename (file);

    if (file_type == G_FILE_TYPE_DIRECTORY)
    {
        secondary = IS_IO_ERROR (error, PERMISSION_DENIED) ?
                    g_strdup_printf (_("There was an error deleting the "
                                       "folder “%s”."),
                                     basename) :
                    g_strdup_printf (_("You do not have sufficient permissions "
                                       "to delete the folder “%s”."),
                                     basename);
    }
    else
    {
        secondary = IS_IO_ERROR (error, PERMISSION_DENIED) ?
                    g_strdup_printf (_("There was an error deleting the "
                                       "file “%s”."),
                                     basename) :
                    g_strdup_printf (_("You do not have sufficient permissions "
                                       "to delete the file “%s”."),
                                     basename);
    }

    details = error->message;

    response = run_cancel_or_skip_warning (job,
                                           primary,
                                           secondary,
                                           details,
                                           source_info->num_files,
                                           source_info->num_files - transfer_info->num_files);

    if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
    {
        abort_job (job);
    }
    else if (response == 1)
    {
        /* skip all */
        job->skip_all_error = TRUE;
    }
}

static void
delete_files (CommonJob *job,
              GList     *files,
              int       *files_skipped)
{
    GList *l;
    GFile *file;
    g_auto (SourceInfo) source_info = SOURCE_INFO_INIT;
    TransferInfo transfer_info;
    DeleteData data;

    if (job_aborted (job))
    {
        return;
    }

    scan_sources (files,
                  &source_info,
                  job,
                  OP_KIND_DELETE);
    if (job_aborted (job))
    {
        return;
    }

    g_timer_start (job->time);

    memset (&transfer_info, 0, sizeof (transfer_info));
    report_delete_progress (job, &source_info, &transfer_info);

    data.job = job;
    data.source_info = &source_info;
    data.transfer_info = &transfer_info;

    for (l = files;
         l != NULL && !job_aborted (job);
         l = l->next)
    {
        gboolean success;

        file = l->data;

        if (should_skip_file (job, file))
        {
            (*files_skipped)++;
            continue;
        }

        success = delete_file_recursively (file, job->cancellable,
                                           file_deleted_callback,
                                           &data);

        if (!success)
        {
            (*files_skipped)++;
        }
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
report_trash_progress (CommonJob    *job,
                       SourceInfo   *source_info,
                       TransferInfo *transfer_info)
{
    int files_left;
    double elapsed, transfer_rate;
    int remaining_time;
    gint64 now;
    char *details;
    char *status;
    DeleteJob *delete_job;

    delete_job = (DeleteJob *) job;
    now = g_get_monotonic_time ();
    files_left = source_info->num_files - transfer_info->num_files;

    /* Races and whatnot could cause this to be negative... */
    if (files_left < 0)
    {
        files_left = 0;
    }

    /* If the number of files left is 0, we want to update the status without
     * considering this time, since we want to change the status to completed
     * and probably we won't get more calls to this function */
    if (transfer_info->last_report_time != 0 &&
        ABS ((gint64) (transfer_info->last_report_time - now)) < 100 * NSEC_PER_MICROSEC &&
        files_left > 0)
    {
        return;
    }

    transfer_info->last_report_time = now;

    if (source_info->num_files == 1)
    {
        g_autofree gchar *basename = NULL;

        if (files_left > 0)
        {
            status = _("Trashing “%s”");
        }
        else
        {
            status = _("Trashed “%s”");
        }

        basename = get_basename (G_FILE (delete_job->files->data));
        nautilus_progress_info_take_status (job->progress,
                                            g_strdup_printf (status, basename));
    }
    else
    {
        if (files_left > 0)
        {
            status = ngettext ("Trashing %'d file",
                               "Trashing %'d files",
                               source_info->num_files);
        }
        else
        {
            status = ngettext ("Trashed %'d file",
                               "Trashed %'d files",
                               source_info->num_files);
        }
        nautilus_progress_info_take_status (job->progress,
                                            g_strdup_printf (status,
                                                             source_info->num_files));
    }


    elapsed = g_timer_elapsed (job->time, NULL);
    transfer_rate = 0;
    remaining_time = INT_MAX;
    if (elapsed > 0)
    {
        transfer_rate = transfer_info->num_files / elapsed;
        if (transfer_rate > 0)
        {
            remaining_time = (source_info->num_files - transfer_info->num_files) / transfer_rate;
        }
    }

    if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE ||
        transfer_rate == 0)
    {
        if (files_left > 0)
        {
            /* To translators: %'d is the number of files completed for the operation,
             * so it will be something like 2/14. */
            details = g_strdup_printf (_("%'d / %'d"),
                                       transfer_info->num_files + 1,
                                       source_info->num_files);
        }
        else
        {
            /* To translators: %'d is the number of files completed for the operation,
             * so it will be something like 2/14. */
            details = g_strdup_printf (_("%'d / %'d"),
                                       transfer_info->num_files,
                                       source_info->num_files);
        }
    }
    else
    {
        if (files_left > 0)
        {
            gchar *time_left_message;
            gchar *files_per_second_message;
            gchar *concat_detail;
            g_autofree gchar *formatted_time = NULL;

            /* To translators: %s will expand to a time duration like "2 minutes".
             * So the whole thing will be something like "1 / 5 -- 2 hours left (4 files/sec)"
             *
             * The singular/plural form will be used depending on the remaining time (i.e. the %s argument).
             */
            time_left_message = ngettext ("%'d / %'d \xE2\x80\x94 %s left",
                                          "%'d / %'d \xE2\x80\x94 %s left",
                                          seconds_count_format_time_units (remaining_time));
            files_per_second_message = ngettext ("(%d file/sec)",
                                                 "(%d files/sec)",
                                                 (int) (transfer_rate + 0.5));
            concat_detail = g_strconcat (time_left_message, " ", files_per_second_message, NULL);

            formatted_time = get_formatted_time (remaining_time);
            details = g_strdup_printf (concat_detail,
                                       transfer_info->num_files + 1,
                                       source_info->num_files,
                                       formatted_time,
                                       (int) transfer_rate + 0.5);

            g_free (concat_detail);
        }
        else
        {
            /* To translators: %'d is the number of files completed for the operation,
             * so it will be something like 2/14. */
            details = g_strdup_printf (_("%'d / %'d"),
                                       transfer_info->num_files,
                                       source_info->num_files);
        }
    }
    nautilus_progress_info_set_details (job->progress, details);

    if (elapsed > SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE)
    {
        nautilus_progress_info_set_remaining_time (job->progress,
                                                   remaining_time);
        nautilus_progress_info_set_elapsed_time (job->progress,
                                                 elapsed);
    }

    if (source_info->num_files != 0)
    {
        nautilus_progress_info_set_progress (job->progress, transfer_info->num_files, source_info->num_files);
    }
}
#pragma GCC diagnostic pop

static void
trash_file (CommonJob     *job,
            GFile         *file,
            gboolean      *skipped_file,
            SourceInfo    *source_info,
            TransferInfo  *transfer_info,
            gboolean       toplevel,
            GList        **to_delete)
{
    GError *error;
    char *primary, *secondary, *details;
    int response;
    g_autofree gchar *basename = NULL;

    if (should_skip_file (job, file))
    {
        *skipped_file = TRUE;
        return;
    }

    error = NULL;

    if (g_file_trash (file, job->cancellable, &error))
    {
        transfer_info->num_files++;
        nautilus_file_changes_queue_file_removed (file);

        if (job->undo_info != NULL)
        {
            nautilus_file_undo_info_trash_add_file (NAUTILUS_FILE_UNDO_INFO_TRASH (job->undo_info), file);
        }

        report_trash_progress (job, source_info, transfer_info);
        return;
    }

    if (job->skip_all_error)
    {
        *skipped_file = TRUE;
        goto skip;
    }

    if (job->delete_all)
    {
        *to_delete = g_list_prepend (*to_delete, file);
        goto skip;
    }

    basename = get_basename (file);
    /* Translators: %s is a file name */
    primary = g_strdup_printf (_("“%s” can’t be put in the trash. Do you want "
                                 "to delete it immediately?"),
                               basename);

    details = NULL;
    secondary = NULL;
    if (!IS_IO_ERROR (error, NOT_SUPPORTED))
    {
        details = error->message;
    }
    else if (!g_file_is_native (file))
    {
        secondary = g_strdup (_("This remote location does not support sending items to the trash."));
    }

    response = run_question (job,
                             primary,
                             secondary,
                             details,
                             (source_info->num_files - transfer_info->num_files) > 1,
                             CANCEL, SKIP_ALL, SKIP, DELETE_ALL, DELETE,
                             NULL);

    if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
    {
        ((DeleteJob *) job)->user_cancel = TRUE;
        abort_job (job);
    }
    else if (response == 1)         /* skip all */
    {
        *skipped_file = TRUE;
        job->skip_all_error = TRUE;
    }
    else if (response == 2)         /* skip */
    {
        *skipped_file = TRUE;
        job->skip_all_error = TRUE;
    }
    else if (response == 3)         /* delete all */
    {
        *to_delete = g_list_prepend (*to_delete, file);
        job->delete_all = TRUE;
    }
    else if (response == 4)         /* delete */
    {
        *to_delete = g_list_prepend (*to_delete, file);
    }

skip:
    g_error_free (error);
}

static void
source_info_remove_descendent_files_from_count (GFile         *dir,
                                                SourceDirInfo *dir_info,
                                                SourceInfo    *source_info)
{
    GFile *other_dir;
    SourceDirInfo *other_dir_info;
    GHashTableIter dir_info_iter;

    source_info->num_files -= dir_info->num_files_children;
    source_info->num_bytes -= dir_info->num_bytes_children;

    g_hash_table_iter_init (&dir_info_iter, source_info->scanned_dirs_info);
    while (g_hash_table_iter_next (&dir_info_iter, (gpointer *) &other_dir, (gpointer *) &other_dir_info))
    {
        g_assert (other_dir != NULL);
        g_assert (other_dir_info != NULL);

        if (other_dir_info != dir_info &&
            g_file_has_parent (other_dir, dir))
        {
            source_info_remove_descendent_files_from_count (other_dir,
                                                            other_dir_info,
                                                            source_info);
        }
    }
}

static void
source_info_remove_file_from_count (GFile      *file,
                                    CommonJob  *job,
                                    SourceInfo *source_info)
{
    g_autoptr (GFileInfo) file_info = NULL;
    SourceDirInfo *dir_info;

    if (g_cancellable_is_cancelled (job->cancellable))
    {
        return;
    }

    file_info = g_file_query_info (file,
                                   G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   job->cancellable,
                                   NULL);

    source_info->num_files--;
    if (file_info != NULL)
    {
        source_info->num_bytes -= g_file_info_get_size (file_info);
    }

    dir_info = g_hash_table_lookup (source_info->scanned_dirs_info, file);

    if (dir_info != NULL)
    {
        source_info_remove_descendent_files_from_count (file,
                                                        dir_info,
                                                        source_info);
    }
}

static void
trash_files (CommonJob *job,
             GList     *files,
             int       *files_skipped)
{
    GList *l;
    GFile *file;
    GList *to_delete;
    g_auto (SourceInfo) source_info = SOURCE_INFO_INIT;
    TransferInfo transfer_info;
    gboolean skipped_file;

    if (job_aborted (job))
    {
        return;
    }

    scan_sources (files,
                  &source_info,
                  job,
                  OP_KIND_TRASH);
    if (job_aborted (job))
    {
        return;
    }

    g_timer_start (job->time);

    memset (&transfer_info, 0, sizeof (transfer_info));
    report_trash_progress (job, &source_info, &transfer_info);

    to_delete = NULL;
    for (l = files;
         l != NULL && !job_aborted (job);
         l = l->next)
    {
        file = l->data;

        skipped_file = FALSE;
        trash_file (job, file,
                    &skipped_file,
                    &source_info, &transfer_info,
                    TRUE, &to_delete);
        if (skipped_file)
        {
            (*files_skipped)++;
            source_info_remove_file_from_count (file, job, &source_info);
            report_trash_progress (job, &source_info, &transfer_info);
        }
    }

    if (to_delete)
    {
        to_delete = g_list_reverse (to_delete);
        delete_files (job, to_delete, files_skipped);
        g_list_free (to_delete);
    }
}

static void
delete_task_done (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
    DeleteJob *job;
    GHashTable *debuting_uris;

    job = user_data;

    g_list_free_full (job->files, g_object_unref);

    if (job->done_callback)
    {
        debuting_uris = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
        job->done_callback (debuting_uris, job->user_cancel, job->done_callback_data);
        g_hash_table_unref (debuting_uris);
    }

    finalize_common ((CommonJob *) job);

    nautilus_file_changes_consume_changes (TRUE);
}

static void
trash_or_delete_internal (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
    DeleteJob *job = task_data;
    g_autoptr (GList) to_trash_files = NULL;
    g_autoptr (GList) to_delete_files = NULL;
    GList *l;
    GFile *file;
    gboolean confirmed;
    CommonJob *common;
    gboolean must_confirm_delete_in_trash;
    gboolean must_confirm_delete;
    int files_skipped;

    common = (CommonJob *) job;

    nautilus_progress_info_start (job->common.progress);

    must_confirm_delete_in_trash = FALSE;
    must_confirm_delete = FALSE;
    files_skipped = 0;

    for (l = job->files; l != NULL; l = l->next)
    {
        file = l->data;

        if (job->try_trash &&
            g_file_has_uri_scheme (file, "trash"))
        {
            must_confirm_delete_in_trash = TRUE;
            to_delete_files = g_list_prepend (to_delete_files, file);
        }
        else if (can_delete_without_confirm (file))
        {
            to_delete_files = g_list_prepend (to_delete_files, file);
        }
        else
        {
            if (job->try_trash)
            {
                to_trash_files = g_list_prepend (to_trash_files, file);
            }
            else
            {
                must_confirm_delete = TRUE;
                to_delete_files = g_list_prepend (to_delete_files, file);
            }
        }
    }

    if (to_delete_files != NULL)
    {
        to_delete_files = g_list_reverse (to_delete_files);
        confirmed = TRUE;
        if (must_confirm_delete_in_trash)
        {
            confirmed = confirm_delete_from_trash (common, to_delete_files);
        }
        else if (must_confirm_delete)
        {
            confirmed = confirm_delete_directly (common, to_delete_files);
        }
        if (confirmed)
        {
            delete_files (common, to_delete_files, &files_skipped);
        }
        else
        {
            job->user_cancel = TRUE;
        }
    }

    if (to_trash_files != NULL)
    {
        to_trash_files = g_list_reverse (to_trash_files);

        trash_files (common, to_trash_files, &files_skipped);
    }

    if (files_skipped == g_list_length (job->files))
    {
        /* User has skipped all files, report user cancel */
        job->user_cancel = TRUE;
    }
}

static DeleteJob *
setup_delete_job (GList                          *files,
                  GtkWindow                      *parent_window,
                  NautilusFileOperationsDBusData *dbus_data,
                  gboolean                        try_trash,
                  NautilusDeleteCallback          done_callback,
                  gpointer                        done_callback_data)
{
    DeleteJob *job;

    /* TODO: special case desktop icon link files ... */
    job = op_job_new (DeleteJob, parent_window, dbus_data);
    job->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
    job->try_trash = try_trash;
    job->user_cancel = FALSE;
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;

    if (g_strcmp0 (g_getenv ("RUNNING_TESTS"), "TRUE"))
    {
        if (try_trash)
        {
            inhibit_power_manager ((CommonJob *) job, _("Trashing Files"));
        }
        else
        {
            inhibit_power_manager ((CommonJob *) job, _("Deleting Files"));
        }
    }

    if (!nautilus_file_undo_manager_is_operating () && try_trash)
    {
        job->common.undo_info = nautilus_file_undo_info_trash_new (g_list_length (files));
    }

    return job;
}

static void
trash_or_delete_internal_sync (GList                          *files,
                               GtkWindow                      *parent_window,
                               NautilusFileOperationsDBusData *dbus_data,
                               gboolean                        try_trash)
{
    GTask *task;
    DeleteJob *job;

    job = setup_delete_job (files,
                            parent_window,
                            dbus_data,
                            try_trash,
                            NULL,
                            NULL);

    task = g_task_new (NULL, NULL, NULL, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread_sync (task, trash_or_delete_internal);
    g_object_unref (task);
    /* Since g_task_run_in_thread_sync doesn't work with callbacks (in this case not reaching
     * delete_task_done) we need to set up the undo information ourselves.
     */
    delete_task_done (NULL, NULL, job);
}

static void
trash_or_delete_internal_async (GList                          *files,
                                GtkWindow                      *parent_window,
                                NautilusFileOperationsDBusData *dbus_data,
                                gboolean                        try_trash,
                                NautilusDeleteCallback          done_callback,
                                gpointer                        done_callback_data)
{
    GTask *task;
    DeleteJob *job;

    job = setup_delete_job (files,
                            parent_window,
                            dbus_data,
                            try_trash,
                            done_callback,
                            done_callback_data);

    task = g_task_new (NULL, NULL, delete_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, trash_or_delete_internal);
    g_object_unref (task);
}

void
nautilus_file_operations_trash_or_delete_sync (GList *files)
{
    trash_or_delete_internal_sync (files, NULL, NULL, TRUE);
}

void
nautilus_file_operations_delete_sync (GList *files)
{
    trash_or_delete_internal_sync (files, NULL, NULL, FALSE);
}

void
nautilus_file_operations_trash_or_delete_async (GList                          *files,
                                                GtkWindow                      *parent_window,
                                                NautilusFileOperationsDBusData *dbus_data,
                                                NautilusDeleteCallback          done_callback,
                                                gpointer                        done_callback_data)
{
    trash_or_delete_internal_async (files, parent_window,
                                    dbus_data,
                                    TRUE,
                                    done_callback, done_callback_data);
}

void
nautilus_file_operations_delete_async (GList                          *files,
                                       GtkWindow                      *parent_window,
                                       NautilusFileOperationsDBusData *dbus_data,
                                       NautilusDeleteCallback          done_callback,
                                       gpointer                        done_callback_data)
{
    trash_or_delete_internal_async (files, parent_window,
                                    dbus_data,
                                    FALSE,
                                    done_callback, done_callback_data);
}



typedef struct
{
    gboolean eject;
    GMount *mount;
    GMountOperation *mount_operation;
    GtkWindow *parent_window;
    NautilusUnmountCallback callback;
    gpointer callback_data;
} UnmountData;

static void
unmount_data_free (UnmountData *data)
{
    if (data->parent_window)
    {
        g_object_remove_weak_pointer (G_OBJECT (data->parent_window),
                                      (gpointer *) &data->parent_window);
    }

    g_clear_object (&data->mount_operation);
    g_object_unref (data->mount);
    g_free (data);
}

static void
unmount_mount_callback (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
    UnmountData *data = user_data;
    GError *error;
    char *primary;
    gboolean unmounted;

    error = NULL;
    if (data->eject)
    {
        unmounted = g_mount_eject_with_operation_finish (G_MOUNT (source_object),
                                                         res, &error);
    }
    else
    {
        unmounted = g_mount_unmount_with_operation_finish (G_MOUNT (source_object),
                                                           res, &error);
    }

    if (!unmounted)
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
            g_autofree gchar *mount_name = NULL;

            mount_name = g_mount_get_name (G_MOUNT (source_object));
            if (data->eject)
            {
                primary = g_strdup_printf (_("Unable to eject %s"),
                                           mount_name);
            }
            else
            {
                primary = g_strdup_printf (_("Unable to unmount %s"),
                                           mount_name);
            }
            show_dialog (primary,
                         error->message,
                         data->parent_window,
                         GTK_MESSAGE_ERROR);
            g_free (primary);
        }
    }

    if (data->callback)
    {
        data->callback (data->callback_data);
    }

    if (error != NULL)
    {
        g_error_free (error);
    }

    unmount_data_free (data);
}

static void
do_unmount (UnmountData *data)
{
    GMountOperation *mount_op;

    if (data->mount_operation)
    {
        mount_op = g_object_ref (data->mount_operation);
    }
    else
    {
        mount_op = gtk_mount_operation_new (data->parent_window);
    }

    g_signal_connect (mount_op, "show-unmount-progress",
                      G_CALLBACK (show_unmount_progress_cb), NULL);
    g_signal_connect (mount_op, "aborted",
                      G_CALLBACK (show_unmount_progress_aborted_cb), NULL);

    if (data->eject)
    {
        g_mount_eject_with_operation (data->mount,
                                      0,
                                      mount_op,
                                      NULL,
                                      unmount_mount_callback,
                                      data);
    }
    else
    {
        g_mount_unmount_with_operation (data->mount,
                                        0,
                                        mount_op,
                                        NULL,
                                        unmount_mount_callback,
                                        data);
    }
    g_object_unref (mount_op);
}

static gboolean
dir_has_files (GFile *dir)
{
    GFileEnumerator *enumerator;
    gboolean res;
    GFileInfo *file_info;

    res = FALSE;

    enumerator = g_file_enumerate_children (dir,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            0,
                                            NULL, NULL);
    if (enumerator)
    {
        file_info = g_file_enumerator_next_file (enumerator, NULL, NULL);
        if (file_info != NULL)
        {
            res = TRUE;
            g_object_unref (file_info);
        }

        g_file_enumerator_close (enumerator, NULL, NULL);
        g_object_unref (enumerator);
    }


    return res;
}

static GList *
get_trash_dirs_for_mount (GMount *mount)
{
    GFile *root;
    GFile *trash;
    char *relpath;
    GList *list;

    root = g_mount_get_root (mount);
    if (root == NULL)
    {
        return NULL;
    }

    list = NULL;

    if (g_file_is_native (root))
    {
        relpath = g_strdup_printf (".Trash/%d", getuid ());
        trash = g_file_resolve_relative_path (root, relpath);
        g_free (relpath);

        list = g_list_prepend (list, g_file_get_child (trash, "files"));
        list = g_list_prepend (list, g_file_get_child (trash, "info"));

        g_object_unref (trash);

        relpath = g_strdup_printf (".Trash-%d", getuid ());
        trash = g_file_get_child (root, relpath);
        g_free (relpath);

        list = g_list_prepend (list, g_file_get_child (trash, "files"));
        list = g_list_prepend (list, g_file_get_child (trash, "info"));

        g_object_unref (trash);
    }

    g_object_unref (root);

    return list;
}

static gboolean
has_trash_files (GMount *mount)
{
    GList *dirs, *l;
    GFile *dir;
    gboolean res;

    dirs = get_trash_dirs_for_mount (mount);

    res = FALSE;

    for (l = dirs; l != NULL; l = l->next)
    {
        dir = l->data;

        if (dir_has_files (dir))
        {
            res = TRUE;
            break;
        }
    }

    g_list_free_full (dirs, g_object_unref);

    return res;
}


static gint
prompt_empty_trash (GtkWindow *parent_window)
{
    gint result;
    GtkWidget *dialog;
    GdkScreen *screen;

    screen = NULL;
    if (parent_window != NULL)
    {
        screen = gtk_widget_get_screen (GTK_WIDGET (parent_window));
    }

    /* Do we need to be modal ? */
    dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                     _("Do you want to empty the trash before you unmount?"));
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              _("In order to regain the "
                                                "free space on this volume "
                                                "the trash must be emptied. "
                                                "All trashed items on the volume "
                                                "will be permanently lost."));
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            _("Do _not Empty Trash"), GTK_RESPONSE_REJECT,
                            CANCEL, GTK_RESPONSE_CANCEL,
                            _("Empty _Trash"), GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_window_set_title (GTK_WINDOW (dialog), "");     /* as per HIG */
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
    if (screen)
    {
        gtk_window_set_screen (GTK_WINDOW (dialog), screen);
    }
    atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);

    /* Make transient for the window group */
    gtk_widget_realize (dialog);
    if (screen != NULL)
    {
        gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)),
                                      gdk_screen_get_root_window (screen));
    }

    result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return result;
}

static void
empty_trash_for_unmount_done (gboolean success,
                              gpointer user_data)
{
    UnmountData *data = user_data;
    do_unmount (data);
}

void
nautilus_file_operations_unmount_mount_full (GtkWindow               *parent_window,
                                             GMount                  *mount,
                                             GMountOperation         *mount_operation,
                                             gboolean                 eject,
                                             gboolean                 check_trash,
                                             NautilusUnmountCallback  callback,
                                             gpointer                 callback_data)
{
    UnmountData *data;
    int response;

    data = g_new0 (UnmountData, 1);
    data->callback = callback;
    data->callback_data = callback_data;
    if (parent_window)
    {
        data->parent_window = parent_window;
        g_object_add_weak_pointer (G_OBJECT (data->parent_window),
                                   (gpointer *) &data->parent_window);
    }
    if (mount_operation)
    {
        data->mount_operation = g_object_ref (mount_operation);
    }
    data->eject = eject;
    data->mount = g_object_ref (mount);

    if (check_trash && has_trash_files (mount))
    {
        response = prompt_empty_trash (parent_window);

        if (response == GTK_RESPONSE_ACCEPT)
        {
            GTask *task;
            EmptyTrashJob *job;

            job = op_job_new (EmptyTrashJob, parent_window, NULL);
            job->should_confirm = FALSE;
            job->trash_dirs = get_trash_dirs_for_mount (mount);
            job->done_callback = empty_trash_for_unmount_done;
            job->done_callback_data = data;

            task = g_task_new (NULL, NULL, empty_trash_task_done, job);
            g_task_set_task_data (task, job, NULL);
            g_task_run_in_thread (task, empty_trash_thread_func);
            g_object_unref (task);
            return;
        }
        else if (response == GTK_RESPONSE_CANCEL)
        {
            if (callback)
            {
                callback (callback_data);
            }

            unmount_data_free (data);
            return;
        }
    }

    do_unmount (data);
}

void
nautilus_file_operations_unmount_mount (GtkWindow *parent_window,
                                        GMount    *mount,
                                        gboolean   eject,
                                        gboolean   check_trash)
{
    nautilus_file_operations_unmount_mount_full (parent_window, mount, NULL, eject,
                                                 check_trash, NULL, NULL);
}

static void
mount_callback_data_notify (gpointer  data,
                            GObject  *object)
{
    GMountOperation *mount_op;

    mount_op = G_MOUNT_OPERATION (data);
    g_object_set_data (G_OBJECT (mount_op), "mount-callback", NULL);
    g_object_set_data (G_OBJECT (mount_op), "mount-callback-data", NULL);
}

static void
volume_mount_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
    NautilusMountCallback mount_callback;
    GObject *mount_callback_data_object;
    GMountOperation *mount_op = user_data;
    GError *error;
    char *primary;
    char *name;
    gboolean success;

    success = TRUE;
    error = NULL;
    if (!g_volume_mount_finish (G_VOLUME (source_object), res, &error))
    {
        if (error->code != G_IO_ERROR_FAILED_HANDLED &&
            error->code != G_IO_ERROR_ALREADY_MOUNTED)
        {
            GtkWindow *parent;

            parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));
            name = g_volume_get_name (G_VOLUME (source_object));
            primary = g_strdup_printf (_("Unable to access “%s”"), name);
            g_free (name);
            success = FALSE;
            show_dialog (primary,
                         error->message,
                         parent,
                         GTK_MESSAGE_ERROR);
            g_free (primary);
        }
        g_error_free (error);
    }

    mount_callback = (NautilusMountCallback)
                     g_object_get_data (G_OBJECT (mount_op), "mount-callback");
    mount_callback_data_object =
        g_object_get_data (G_OBJECT (mount_op), "mount-callback-data");

    if (mount_callback != NULL)
    {
        (*mount_callback)(G_VOLUME (source_object),
                          success,
                          mount_callback_data_object);

        if (mount_callback_data_object != NULL)
        {
            g_object_weak_unref (mount_callback_data_object,
                                 mount_callback_data_notify,
                                 mount_op);
        }
    }

    g_object_unref (mount_op);
}


void
nautilus_file_operations_mount_volume (GtkWindow *parent_window,
                                       GVolume   *volume)
{
    nautilus_file_operations_mount_volume_full (parent_window, volume,
                                                NULL, NULL);
}

void
nautilus_file_operations_mount_volume_full (GtkWindow             *parent_window,
                                            GVolume               *volume,
                                            NautilusMountCallback  mount_callback,
                                            GObject               *mount_callback_data_object)
{
    GMountOperation *mount_op;

    mount_op = gtk_mount_operation_new (parent_window);
    g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
    g_object_set_data (G_OBJECT (mount_op),
                       "mount-callback",
                       mount_callback);

    if (mount_callback != NULL &&
        mount_callback_data_object != NULL)
    {
        g_object_weak_ref (mount_callback_data_object,
                           mount_callback_data_notify,
                           mount_op);
    }
    g_object_set_data (G_OBJECT (mount_op),
                       "mount-callback-data",
                       mount_callback_data_object);

    g_volume_mount (volume, 0, mount_op, NULL, volume_mount_cb, mount_op);
}

static void
report_preparing_count_progress (CommonJob  *job,
                                 SourceInfo *source_info)
{
    char *s;

    switch (source_info->op)
    {
        default:
        case OP_KIND_COPY:
        {
            g_autofree gchar *formatted_size = NULL;

            formatted_size = g_format_size (source_info->num_bytes);
            s = g_strdup_printf (ngettext ("Preparing to copy %'d file (%s)",
                                           "Preparing to copy %'d files (%s)",
                                           source_info->num_files),
                                 source_info->num_files,
                                 formatted_size);
        }
        break;

        case OP_KIND_MOVE:
        {
            g_autofree gchar *formatted_size = NULL;

            formatted_size = g_format_size (source_info->num_bytes);
            s = g_strdup_printf (ngettext ("Preparing to move %'d file (%s)",
                                           "Preparing to move %'d files (%s)",
                                           source_info->num_files),
                                 source_info->num_files,
                                 formatted_size);
        }
        break;

        case OP_KIND_DELETE:
        {
            g_autofree gchar *formatted_size = NULL;

            formatted_size = g_format_size (source_info->num_bytes);
            s = g_strdup_printf (ngettext ("Preparing to delete %'d file (%s)",
                                           "Preparing to delete %'d files (%s)",
                                           source_info->num_files),
                                 source_info->num_files,
                                 formatted_size);
        }
        break;

        case OP_KIND_TRASH:
        {
            s = g_strdup_printf (ngettext ("Preparing to trash %'d file",
                                           "Preparing to trash %'d files",
                                           source_info->num_files),
                                 source_info->num_files);
        }
        break;

        case OP_KIND_COMPRESS:
        {
            s = g_strdup_printf (ngettext ("Preparing to compress %'d file",
                                           "Preparing to compress %'d files",
                                           source_info->num_files),
                                 source_info->num_files);
        }
    }

    nautilus_progress_info_take_details (job->progress, s);
    nautilus_progress_info_pulse_progress (job->progress);
}

static void
count_file (GFileInfo     *info,
            CommonJob     *job,
            SourceInfo    *source_info,
            SourceDirInfo *dir_info)
{
    goffset num_bytes = g_file_info_get_size (info);

    source_info->num_files += 1;
    source_info->num_bytes += num_bytes;

    if (dir_info != NULL)
    {
        dir_info->num_files_children += 1;
        dir_info->num_bytes_children += num_bytes;
    }

    if (source_info->num_files_since_progress++ > 100)
    {
        report_preparing_count_progress (job, source_info);
        source_info->num_files_since_progress = 0;
    }
}

static char *
get_scan_primary (OpKind kind)
{
    switch (kind)
    {
        default:
        case OP_KIND_COPY:
        {
            return g_strdup (_("Error while copying."));
        }

        case OP_KIND_MOVE:
        {
            return g_strdup (_("Error while moving."));
        }

        case OP_KIND_DELETE:
        {
            return g_strdup (_("Error while deleting."));
        }

        case OP_KIND_TRASH:
        {
            return g_strdup (_("Error while moving files to trash."));
        }

        case OP_KIND_COMPRESS:
        {
            return g_strdup (_("Error while compressing files."));
        }
    }
}

static void
scan_dir (GFile      *dir,
          SourceInfo *source_info,
          CommonJob  *job,
          GQueue     *dirs)
{
    GFileInfo *info;
    GError *error;
    GFile *subdir;
    GFileEnumerator *enumerator;
    char *primary, *secondary, *details;
    int response;
    SourceInfo saved_info;
    g_autolist (GFile) subdirs = NULL;
    SourceDirInfo *dir_info = NULL;
    gboolean skip_subdirs = FALSE;

    /* It is possible for this function to be called multiple times for
     * the same directory.
     * We pass a NULL SourceDirInfo into count_file() if this directory has
     * already been scanned once so that its children are not counted more
     * than once in the SourceDirInfo corresponding to this directory.
     */

    if (!g_hash_table_contains (source_info->scanned_dirs_info, dir))
    {
        dir_info = g_new0 (SourceDirInfo, 1);

        g_hash_table_insert (source_info->scanned_dirs_info,
                             g_object_ref (dir),
                             dir_info);
    }

    /* Stash a copy of the struct to restore state before goto retry. Note that
     * this assumes the code below does not access any pointer member */
    saved_info = *source_info;

retry:

    if (dir_info != NULL)
    {
        dir_info->num_files_children = 0;
        dir_info->num_bytes_children = 0;
    }

    error = NULL;
    enumerator = g_file_enumerate_children (dir,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            job->cancellable,
                                            &error);
    if (enumerator)
    {
        error = NULL;
        while ((info = g_file_enumerator_next_file (enumerator, job->cancellable, &error)) != NULL)
        {
            count_file (info, job, source_info, dir_info);

            if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
            {
                subdir = g_file_get_child (dir,
                                           g_file_info_get_name (info));

                subdirs = g_list_prepend (subdirs, subdir);
            }

            g_object_unref (info);
        }
        g_file_enumerator_close (enumerator, job->cancellable, NULL);
        g_object_unref (enumerator);

        if (error && IS_IO_ERROR (error, CANCELLED))
        {
            g_error_free (error);
        }
        else if (error)
        {
            g_autofree gchar *basename = NULL;

            primary = get_scan_primary (source_info->op);
            details = NULL;
            basename = get_basename (dir);

            if (IS_IO_ERROR (error, PERMISSION_DENIED))
            {
                secondary = g_strdup_printf (_("Files in the folder “%s” cannot be handled "
                                               "because you do not have permissions to see them."),
                                             basename);
            }
            else
            {
                secondary = g_strdup_printf (_("There was an error getting information about the "
                                               "files in the folder “%s”."), basename);
                details = error->message;
            }

            response = run_warning (job,
                                    primary,
                                    secondary,
                                    details,
                                    FALSE,
                                    CANCEL, RETRY, SKIP,
                                    NULL);

            g_error_free (error);

            if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
            {
                abort_job (job);
                skip_subdirs = TRUE;
            }
            else if (response == 1)
            {
                g_clear_list (&subdirs, g_object_unref);
                *source_info = saved_info;
                goto retry;
            }
            else if (response == 2)
            {
                skip_readdir_error (job, dir);
            }
            else
            {
                g_assert_not_reached ();
            }
        }
    }
    else if (job->skip_all_error)
    {
        g_error_free (error);
        skip_file (job, dir);
        skip_subdirs = TRUE;
    }
    else if (IS_IO_ERROR (error, CANCELLED))
    {
        g_error_free (error);
    }
    else
    {
        g_autofree gchar *basename = NULL;

        primary = get_scan_primary (source_info->op);
        details = NULL;
        basename = get_basename (dir);
        if (IS_IO_ERROR (error, PERMISSION_DENIED))
        {
            secondary = g_strdup_printf (_("The folder “%s” cannot be handled because you "
                                           "do not have permissions to read it."),
                                         basename);
        }
        else
        {
            secondary = g_strdup_printf (_("There was an error reading the folder “%s”."),
                                         basename);
            details = error->message;
        }
        /* set show_all to TRUE here, as we don't know how many
         * files we'll end up processing yet.
         */
        response = run_warning (job,
                                primary,
                                secondary,
                                details,
                                TRUE,
                                CANCEL, SKIP_ALL, SKIP, RETRY,
                                NULL);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
            skip_subdirs = TRUE;
        }
        else if (response == 1 || response == 2)
        {
            if (response == 1)
            {
                job->skip_all_error = TRUE;
            }
            skip_file (job, dir);
            skip_subdirs = TRUE;
        }
        else if (response == 3)
        {
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }
    }

    if (!skip_subdirs)
    {
        while (subdirs != NULL)
        {
            GList *l = subdirs;
            subdirs = g_list_remove_link (subdirs, l);

            /* Push to head, since we want depth-first */
            g_queue_push_head_link (dirs, l);
        }
    }
}

static void
scan_file (GFile      *file,
           SourceInfo *source_info,
           CommonJob  *job)
{
    GFileInfo *info;
    GError *error;
    GQueue *dirs;
    GFile *dir;
    char *primary;
    char *secondary;
    char *details;
    int response;

    dirs = g_queue_new ();

retry:
    error = NULL;
    info = g_file_query_info (file,
                              G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                              G_FILE_ATTRIBUTE_STANDARD_SIZE,
                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                              job->cancellable,
                              &error);

    if (info)
    {
        count_file (info, job, source_info, NULL);

        /* trashing operation doesn't recurse */
        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY &&
            source_info->op != OP_KIND_TRASH)
        {
            g_queue_push_head (dirs, g_object_ref (file));
        }
        g_object_unref (info);
    }
    else if (job->skip_all_error)
    {
        g_error_free (error);
        skip_file (job, file);
    }
    else if (IS_IO_ERROR (error, CANCELLED))
    {
        g_error_free (error);
    }
    else
    {
        g_autofree gchar *basename = NULL;

        primary = get_scan_primary (source_info->op);
        details = NULL;
        basename = get_basename (file);

        if (IS_IO_ERROR (error, PERMISSION_DENIED))
        {
            secondary = g_strdup_printf (_("The file “%s” cannot be handled because you do not have "
                                           "permissions to read it."), basename);
        }
        else
        {
            secondary = g_strdup_printf (_("There was an error getting information about “%s”."),
                                         basename);
            details = error->message;
        }
        /* set show_all to TRUE here, as we don't know how many
         * files we'll end up processing yet.
         */
        response = run_warning (job,
                                primary,
                                secondary,
                                details,
                                TRUE,
                                CANCEL, SKIP_ALL, SKIP, RETRY,
                                NULL);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1 || response == 2)
        {
            if (response == 1)
            {
                job->skip_all_error = TRUE;
            }
            skip_file (job, file);
        }
        else if (response == 3)
        {
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }
    }

    while (!job_aborted (job) &&
           (dir = g_queue_pop_head (dirs)) != NULL)
    {
        scan_dir (dir, source_info, job, dirs);
        g_object_unref (dir);
    }

    /* Free all from queue if we exited early */
    g_queue_foreach (dirs, (GFunc) g_object_unref, NULL);
    g_queue_free (dirs);
}

static void
scan_sources (GList      *files,
              SourceInfo *source_info,
              CommonJob  *job,
              OpKind      kind)
{
    GList *l;
    GFile *file;

    source_info->op = kind;
    source_info->scanned_dirs_info = g_hash_table_new_full (g_file_hash,
                                                            (GEqualFunc) g_file_equal,
                                                            (GDestroyNotify) g_object_unref,
                                                            (GDestroyNotify) g_free);

    report_preparing_count_progress (job, source_info);

    for (l = files; l != NULL && !job_aborted (job); l = l->next)
    {
        file = l->data;

        scan_file (file,
                   source_info,
                   job);
    }

    /* Make sure we report the final count */
    report_preparing_count_progress (job, source_info);
}

static void
verify_destination (CommonJob  *job,
                    GFile      *dest,
                    char      **dest_fs_id,
                    goffset     required_size)
{
    GFileInfo *info, *fsinfo;
    GError *error;
    guint64 free_size;
    guint64 size_difference;
    char *primary, *secondary, *details;
    int response;
    GFileType file_type;
    gboolean dest_is_symlink = FALSE;

    if (dest_fs_id)
    {
        *dest_fs_id = NULL;
    }

retry:

    error = NULL;
    info = g_file_query_info (dest,
                              G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                              G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                              dest_is_symlink ? G_FILE_QUERY_INFO_NONE : G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                              job->cancellable,
                              &error);

    if (info == NULL)
    {
        g_autofree gchar *basename = NULL;

        if (IS_IO_ERROR (error, CANCELLED))
        {
            g_error_free (error);
            return;
        }

        basename = get_basename (dest);
        primary = g_strdup_printf (_("Error while copying to “%s”."), basename);
        details = NULL;

        if (IS_IO_ERROR (error, PERMISSION_DENIED))
        {
            secondary = g_strdup (_("You do not have permissions to access the destination folder."));
        }
        else
        {
            secondary = g_strdup (_("There was an error getting information about the destination."));
            details = error->message;
        }

        response = run_error (job,
                              primary,
                              secondary,
                              details,
                              FALSE,
                              CANCEL, RETRY,
                              NULL);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)
        {
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }

        return;
    }

    file_type = g_file_info_get_file_type (info);
    if (!dest_is_symlink && file_type == G_FILE_TYPE_SYMBOLIC_LINK)
    {
        /* Record that destination is a symlink and do real stat() once again */
        dest_is_symlink = TRUE;
        g_object_unref (info);
        goto retry;
    }

    if (dest_fs_id)
    {
        *dest_fs_id =
            g_strdup (g_file_info_get_attribute_string (info,
                                                        G_FILE_ATTRIBUTE_ID_FILESYSTEM));
    }

    g_object_unref (info);

    if (file_type != G_FILE_TYPE_DIRECTORY)
    {
        g_autofree gchar *basename = NULL;

        basename = get_basename (dest);
        primary = g_strdup_printf (_("Error while copying to “%s”."), basename);
        secondary = g_strdup (_("The destination is not a folder."));

        run_error (job,
                   primary,
                   secondary,
                   NULL,
                   FALSE,
                   CANCEL,
                   NULL);

        abort_job (job);
        return;
    }

    if (dest_is_symlink)
    {
        /* We can't reliably statfs() destination if it's a symlink, thus not doing any further checks. */
        return;
    }

    fsinfo = g_file_query_filesystem_info (dest,
                                           G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
                                           G_FILE_ATTRIBUTE_FILESYSTEM_READONLY,
                                           job->cancellable,
                                           NULL);
    if (fsinfo == NULL)
    {
        /* All sorts of things can go wrong getting the fs info (like not supported)
         * only check these things if the fs returns them
         */
        return;
    }

    if (required_size > 0 &&
        g_file_info_has_attribute (fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
    {
        free_size = g_file_info_get_attribute_uint64 (fsinfo,
                                                      G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

        if (free_size < required_size)
        {
            g_autofree gchar *basename = NULL;
            g_autofree gchar *formatted_size = NULL;

            basename = get_basename (dest);
            size_difference = required_size - free_size;
            primary = g_strdup_printf (_("Error while copying to “%s”."), basename);
            secondary = g_strdup (_("There is not enough space on the destination."
                                    " Try to remove files to make space."));

            formatted_size = g_format_size (size_difference);
            details = g_strdup_printf (_("%s more space is required to copy to the destination."),
                                       formatted_size);

            response = run_warning (job,
                                    primary,
                                    secondary,
                                    details,
                                    FALSE,
                                    CANCEL,
                                    COPY_FORCE,
                                    RETRY,
                                    NULL);

            if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
            {
                abort_job (job);
            }
            else if (response == 2)
            {
                goto retry;
            }
            else if (response == 1)
            {
                /* We are forced to copy - just fall through ... */
            }
            else
            {
                g_assert_not_reached ();
            }
        }
    }

    if (!job_aborted (job) &&
        g_file_info_get_attribute_boolean (fsinfo,
                                           G_FILE_ATTRIBUTE_FILESYSTEM_READONLY))
    {
        g_autofree gchar *basename = NULL;

        basename = get_basename (dest);
        primary = g_strdup_printf (_("Error while copying to “%s”."), basename);
        secondary = g_strdup (_("The destination is read-only."));

        run_error (job,
                   primary,
                   secondary,
                   NULL,
                   FALSE,
                   CANCEL,
                   NULL);

        g_error_free (error);

        abort_job (job);
    }

    g_object_unref (fsinfo);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
report_copy_progress (CopyMoveJob  *copy_job,
                      SourceInfo   *source_info,
                      TransferInfo *transfer_info)
{
    int files_left;
    goffset total_size;
    double elapsed, transfer_rate;
    int remaining_time;
    guint64 now;
    CommonJob *job;
    gboolean is_move;
    gchar *status;
    char *details;
    gchar *tmp;

    job = (CommonJob *) copy_job;

    is_move = copy_job->is_move;

    now = g_get_monotonic_time ();

    files_left = source_info->num_files - transfer_info->num_files;

    /* Races and whatnot could cause this to be negative... */
    if (files_left < 0)
    {
        files_left = 0;
    }

    /* If the number of files left is 0, we want to update the status without
     * considering this time, since we want to change the status to completed
     * and probably we won't get more calls to this function */
    if (transfer_info->last_report_time != 0 &&
        ABS ((gint64) (transfer_info->last_report_time - now)) < 100 * NSEC_PER_MICROSEC &&
        files_left > 0)
    {
        return;
    }
    transfer_info->last_report_time = now;

    if (files_left != transfer_info->last_reported_files_left ||
        transfer_info->last_reported_files_left == 0)
    {
        /* Avoid changing this unless files_left changed since last time */
        transfer_info->last_reported_files_left = files_left;

        if (source_info->num_files == 1)
        {
            g_autofree gchar *basename_dest = NULL;

            if (copy_job->destination != NULL)
            {
                if (is_move)
                {
                    if (files_left > 0)
                    {
                        status = _("Moving “%s” to “%s”");
                    }
                    else
                    {
                        status = _("Moved “%s” to “%s”");
                    }
                }
                else
                {
                    if (files_left > 0)
                    {
                        status = _("Copying “%s” to “%s”");
                    }
                    else
                    {
                        status = _("Copied “%s” to “%s”");
                    }
                }

                basename_dest = get_basename (G_FILE (copy_job->destination));

                if (copy_job->fake_display_source != NULL)
                {
                    g_autofree gchar *basename_fake_display_source = NULL;

                    basename_fake_display_source = get_basename (copy_job->fake_display_source);
                    tmp = g_strdup_printf (status,
                                           basename_fake_display_source,
                                           basename_dest);
                }
                else
                {
                    g_autofree gchar *basename_data = NULL;

                    basename_data = get_basename (G_FILE (copy_job->files->data));
                    tmp = g_strdup_printf (status,
                                           basename_data,
                                           basename_dest);
                }

                nautilus_progress_info_take_status (job->progress,
                                                    tmp);
            }
            else
            {
                g_autofree gchar *basename = NULL;

                if (files_left > 0)
                {
                    status = _("Duplicating “%s”");
                }
                else
                {
                    status = _("Duplicated “%s”");
                }

                basename = get_basename (G_FILE (copy_job->files->data));
                nautilus_progress_info_take_status (job->progress,
                                                    g_strdup_printf (status,
                                                                     basename));
            }
        }
        else if (copy_job->files != NULL)
        {
            if (copy_job->destination != NULL)
            {
                if (files_left > 0)
                {
                    g_autofree gchar *basename = NULL;

                    if (is_move)
                    {
                        status = ngettext ("Moving %'d file to “%s”",
                                           "Moving %'d files to “%s”",
                                           source_info->num_files);
                    }
                    else
                    {
                        status = ngettext ("Copying %'d file to “%s”",
                                           "Copying %'d files to “%s”",
                                           source_info->num_files);
                    }

                    basename = get_basename (G_FILE (copy_job->destination));
                    tmp = g_strdup_printf (status,
                                           source_info->num_files,
                                           basename);

                    nautilus_progress_info_take_status (job->progress,
                                                        tmp);
                }
                else
                {
                    g_autofree gchar *basename = NULL;

                    if (is_move)
                    {
                        status = ngettext ("Moved %'d file to “%s”",
                                           "Moved %'d files to “%s”",
                                           source_info->num_files);
                    }
                    else
                    {
                        status = ngettext ("Copied %'d file to “%s”",
                                           "Copied %'d files to “%s”",
                                           source_info->num_files);
                    }

                    basename = get_basename (G_FILE (copy_job->destination));
                    tmp = g_strdup_printf (status,
                                           source_info->num_files,
                                           basename);

                    nautilus_progress_info_take_status (job->progress,
                                                        tmp);
                }
            }
            else
            {
                GFile *parent;
                g_autofree gchar *basename = NULL;

                parent = g_file_get_parent (copy_job->files->data);
                basename = get_basename (parent);
                if (files_left > 0)
                {
                    status = ngettext ("Duplicating %'d file in “%s”",
                                       "Duplicating %'d files in “%s”",
                                       source_info->num_files);
                    nautilus_progress_info_take_status (job->progress,
                                                        g_strdup_printf (status,
                                                                         source_info->num_files,
                                                                         basename));
                }
                else
                {
                    status = ngettext ("Duplicated %'d file in “%s”",
                                       "Duplicated %'d files in “%s”",
                                       source_info->num_files);
                    nautilus_progress_info_take_status (job->progress,
                                                        g_strdup_printf (status,
                                                                         source_info->num_files,
                                                                         basename));
                }
                g_object_unref (parent);
            }
        }
    }

    total_size = MAX (source_info->num_bytes, transfer_info->num_bytes);

    elapsed = g_timer_elapsed (job->time, NULL);
    transfer_rate = 0;
    remaining_time = INT_MAX;
    if (elapsed > 0)
    {
        transfer_rate = transfer_info->num_bytes / elapsed;
        if (transfer_rate > 0)
        {
            remaining_time = (total_size - transfer_info->num_bytes) / transfer_rate;
        }
    }

    if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE ||
        transfer_rate == 0 ||
        !transfer_info->partial_progress)
    {
        if (source_info->num_files == 1)
        {
            g_autofree gchar *formatted_size_num_bytes = NULL;
            g_autofree gchar *formatted_size_total_size = NULL;

            formatted_size_num_bytes = g_format_size (transfer_info->num_bytes);
            formatted_size_total_size = g_format_size (total_size);
            /* To translators: %s will expand to a size like "2 bytes" or "3 MB", so something like "4 kb / 4 MB" */
            details = g_strdup_printf (_("%s / %s"),
                                       formatted_size_num_bytes,
                                       formatted_size_total_size);
        }
        else
        {
            if (files_left > 0)
            {
                /* To translators: %'d is the number of files completed for the operation,
                 * so it will be something like 2/14. */
                details = g_strdup_printf (_("%'d / %'d"),
                                           transfer_info->num_files + 1,
                                           source_info->num_files);
            }
            else
            {
                /* To translators: %'d is the number of files completed for the operation,
                 * so it will be something like 2/14. */
                details = g_strdup_printf (_("%'d / %'d"),
                                           transfer_info->num_files,
                                           source_info->num_files);
            }
        }
    }
    else
    {
        if (source_info->num_files == 1)
        {
            if (files_left > 0)
            {
                g_autofree gchar *formatted_time = NULL;
                g_autofree gchar *formatted_size_num_bytes = NULL;
                g_autofree gchar *formatted_size_total_size = NULL;
                g_autofree gchar *formatted_size_transfer_rate = NULL;

                formatted_time = get_formatted_time (remaining_time);
                formatted_size_num_bytes = g_format_size (transfer_info->num_bytes);
                formatted_size_total_size = g_format_size (total_size);
                formatted_size_transfer_rate = g_format_size ((goffset) transfer_rate);
                /* To translators: %s will expand to a size like "2 bytes" or "3 MB", %s to a time duration like
                 * "2 minutes". So the whole thing will be something like "2 kb / 4 MB -- 2 hours left (4kb/sec)"
                 *
                 * The singular/plural form will be used depending on the remaining time (i.e. the %s argument).
                 */
                details = g_strdup_printf (ngettext ("%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                                     "%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                                     seconds_count_format_time_units (remaining_time)),
                                           formatted_size_num_bytes,
                                           formatted_size_total_size,
                                           formatted_time,
                                           formatted_size_transfer_rate);
            }
            else
            {
                g_autofree gchar *formatted_size_num_bytes = NULL;
                g_autofree gchar *formatted_size_total_size = NULL;

                formatted_size_num_bytes = g_format_size (transfer_info->num_bytes);
                formatted_size_total_size = g_format_size (total_size);
                /* To translators: %s will expand to a size like "2 bytes" or "3 MB". */
                details = g_strdup_printf (_("%s / %s"),
                                           formatted_size_num_bytes,
                                           formatted_size_total_size);
            }
        }
        else
        {
            if (files_left > 0)
            {
                g_autofree gchar *formatted_time = NULL;
                g_autofree gchar *formatted_size = NULL;
                formatted_time = get_formatted_time (remaining_time);
                formatted_size = g_format_size ((goffset) transfer_rate);
                /* To translators: %s will expand to a time duration like "2 minutes".
                 * So the whole thing will be something like "1 / 5 -- 2 hours left (4kb/sec)"
                 *
                 * The singular/plural form will be used depending on the remaining time (i.e. the %s argument).
                 */
                details = g_strdup_printf (ngettext ("%'d / %'d \xE2\x80\x94 %s left (%s/sec)",
                                                     "%'d / %'d \xE2\x80\x94 %s left (%s/sec)",
                                                     seconds_count_format_time_units (remaining_time)),
                                           transfer_info->num_files + 1, source_info->num_files,
                                           formatted_time,
                                           formatted_size);
            }
            else
            {
                /* To translators: %'d is the number of files completed for the operation,
                 * so it will be something like 2/14. */
                details = g_strdup_printf (_("%'d / %'d"),
                                           transfer_info->num_files,
                                           source_info->num_files);
            }
        }
    }
    nautilus_progress_info_take_details (job->progress, details);

    if (elapsed > SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE)
    {
        nautilus_progress_info_set_remaining_time (job->progress,
                                                   remaining_time);
        nautilus_progress_info_set_elapsed_time (job->progress,
                                                 elapsed);
    }

    nautilus_progress_info_set_progress (job->progress, transfer_info->num_bytes, total_size);
}
#pragma GCC diagnostic pop

#define FAT_FORBIDDEN_CHARACTERS "/:*?\"<>\\|"

static gboolean
fat_str_replace (char *str,
                 char  replacement)
{
    gboolean success;
    int i;

    success = FALSE;
    for (i = 0; str[i] != '\0'; i++)
    {
        if (strchr (FAT_FORBIDDEN_CHARACTERS, str[i]) ||
            str[i] < 32)
        {
            success = TRUE;
            str[i] = replacement;
        }
    }

    return success;
}

static gboolean
make_file_name_valid_for_dest_fs (char       *filename,
                                  const char *dest_fs_type)
{
    if (dest_fs_type != NULL && filename != NULL)
    {
        if (!strcmp (dest_fs_type, "fat") ||
            !strcmp (dest_fs_type, "vfat") ||
            /* The fuseblk filesystem type could be of any type
             * in theory, but in practice is usually NTFS or exFAT.
             * This assumption is a pragmatic way to solve
             * https://gitlab.gnome.org/GNOME/nautilus/-/issues/1343 */
            !strcmp (dest_fs_type, "fuse") ||
            !strcmp (dest_fs_type, "ntfs") ||
            !strcmp (dest_fs_type, "msdos") ||
            !strcmp (dest_fs_type, "msdosfs"))
        {
            gboolean ret;
            int i, old_len;

            ret = fat_str_replace (filename, '_');

            old_len = strlen (filename);
            for (i = 0; i < old_len; i++)
            {
                if (filename[i] != ' ')
                {
                    g_strchomp (filename);
                    ret |= (old_len != strlen (filename));
                    break;
                }
            }

            return ret;
        }
    }

    return FALSE;
}

static GFile *
get_unique_target_file (GFile      *src,
                        GFile      *dest_dir,
                        gboolean    same_fs,
                        const char *dest_fs_type,
                        int         count)
{
    const char *editname, *end;
    char *basename, *new_name;
    GFileInfo *info;
    GFile *dest;
    int max_length;
    NautilusFile *file;
    gboolean ignore_extension;

    max_length = nautilus_get_max_child_name_length_for_location (dest_dir);

    file = nautilus_file_get (src);
    ignore_extension = nautilus_file_is_directory (file);
    nautilus_file_unref (file);

    dest = NULL;
    info = g_file_query_info (src,
                              G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME,
                              0, NULL, NULL);
    if (info != NULL)
    {
        editname = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);

        if (editname != NULL)
        {
            new_name = get_duplicate_name (editname, count, max_length, ignore_extension);
            make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
            dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
            g_free (new_name);
        }

        g_object_unref (info);
    }

    if (dest == NULL)
    {
        basename = g_file_get_basename (src);

        if (g_utf8_validate (basename, -1, NULL))
        {
            new_name = get_duplicate_name (basename, count, max_length, ignore_extension);
            make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
            dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
            g_free (new_name);
        }

        if (dest == NULL)
        {
            end = strrchr (basename, '.');
            if (end != NULL)
            {
                count += atoi (end + 1);
            }
            new_name = g_strdup_printf ("%s.%d", basename, count);
            make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
            dest = g_file_get_child (dest_dir, new_name);
            g_free (new_name);
        }

        g_free (basename);
    }

    return dest;
}

static GFile *
get_target_file_for_link (GFile      *src,
                          GFile      *dest_dir,
                          const char *dest_fs_type,
                          int         count)
{
    const char *editname;
    char *basename, *new_name;
    GFileInfo *info;
    GFile *dest;
    int max_length;

    max_length = nautilus_get_max_child_name_length_for_location (dest_dir);

    dest = NULL;
    info = g_file_query_info (src,
                              G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME,
                              0, NULL, NULL);
    if (info != NULL)
    {
        editname = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);

        if (editname != NULL)
        {
            new_name = get_link_name (editname, count, max_length);
            make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
            dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
            g_free (new_name);
        }

        g_object_unref (info);
    }

    if (dest == NULL)
    {
        basename = g_file_get_basename (src);
        make_file_name_valid_for_dest_fs (basename, dest_fs_type);

        if (g_utf8_validate (basename, -1, NULL))
        {
            new_name = get_link_name (basename, count, max_length);
            make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
            dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
            g_free (new_name);
        }

        if (dest == NULL)
        {
            if (count == 1)
            {
                new_name = g_strdup_printf ("%s.lnk", basename);
            }
            else
            {
                new_name = g_strdup_printf ("%s.lnk%d", basename, count);
            }
            make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
            dest = g_file_get_child (dest_dir, new_name);
            g_free (new_name);
        }

        g_free (basename);
    }

    return dest;
}

static GFile *
get_target_file_with_custom_name (GFile       *src,
                                  GFile       *dest_dir,
                                  const char  *dest_fs_type,
                                  gboolean     same_fs,
                                  const gchar *custom_name)
{
    char *basename;
    GFile *dest;
    GFileInfo *info;
    char *copyname;

    dest = NULL;

    if (custom_name != NULL)
    {
        copyname = g_strdup (custom_name);
        make_file_name_valid_for_dest_fs (copyname, dest_fs_type);
        dest = g_file_get_child_for_display_name (dest_dir, copyname, NULL);

        g_free (copyname);
    }

    if (dest == NULL && !same_fs)
    {
        info = g_file_query_info (src,
                                  G_FILE_ATTRIBUTE_STANDARD_COPY_NAME ","
                                  G_FILE_ATTRIBUTE_TRASH_ORIG_PATH,
                                  0, NULL, NULL);

        if (info)
        {
            copyname = NULL;

            /* if file is being restored from trash make sure it uses its original name */
            if (g_file_has_uri_scheme (src, "trash"))
            {
                copyname = g_path_get_basename (g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_TRASH_ORIG_PATH));
            }

            if (copyname == NULL)
            {
                copyname = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_COPY_NAME));
            }

            if (copyname)
            {
                make_file_name_valid_for_dest_fs (copyname, dest_fs_type);
                dest = g_file_get_child_for_display_name (dest_dir, copyname, NULL);
                g_free (copyname);
            }

            g_object_unref (info);
        }
    }

    if (dest == NULL)
    {
        basename = g_file_get_basename (src);
        make_file_name_valid_for_dest_fs (basename, dest_fs_type);
        dest = g_file_get_child (dest_dir, basename);
        g_free (basename);
    }

    return dest;
}

static GFile *
get_target_file (GFile      *src,
                 GFile      *dest_dir,
                 const char *dest_fs_type,
                 gboolean    same_fs)
{
    return get_target_file_with_custom_name (src, dest_dir, dest_fs_type, same_fs, NULL);
}

static gboolean
has_fs_id (GFile      *file,
           const char *fs_id)
{
    const char *id;
    GFileInfo *info;
    gboolean res;

    res = FALSE;
    info = g_file_query_info (file,
                              G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                              NULL, NULL);

    if (info)
    {
        id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);

        if (id && strcmp (id, fs_id) == 0)
        {
            res = TRUE;
        }

        g_object_unref (info);
    }

    return res;
}

static gboolean
is_dir (GFile *file)
{
    GFileType file_type;

    file_type = g_file_query_file_type (file,
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        NULL);

    return file_type == G_FILE_TYPE_DIRECTORY;
}

static GFile *
map_possibly_volatile_file_to_real (GFile         *volatile_file,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
    GFile *real_file = NULL;
    GFileInfo *info = NULL;

    info = g_file_query_info (volatile_file,
                              G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                              G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE ","
                              G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                              cancellable,
                              error);
    if (info == NULL)
    {
        return NULL;
    }
    else
    {
        gboolean is_volatile;

        is_volatile = g_file_info_get_attribute_boolean (info,
                                                         G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE);
        if (is_volatile)
        {
            const gchar *target;

            target = g_file_info_get_symlink_target (info);
            real_file = g_file_resolve_relative_path (volatile_file, target);
        }
    }

    g_object_unref (info);

    if (real_file == NULL)
    {
        real_file = g_object_ref (volatile_file);
    }

    return real_file;
}

static GFile *
map_possibly_volatile_file_to_real_on_write (GFile              *volatile_file,
                                             GFileOutputStream  *stream,
                                             GCancellable       *cancellable,
                                             GError            **error)
{
    GFile *real_file = NULL;
    GFileInfo *info = NULL;

    info = g_file_output_stream_query_info (stream,
                                            G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                            G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE ","
                                            G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                            cancellable,
                                            error);
    if (info == NULL)
    {
        return NULL;
    }
    else
    {
        gboolean is_volatile;

        is_volatile = g_file_info_get_attribute_boolean (info,
                                                         G_FILE_ATTRIBUTE_STANDARD_IS_VOLATILE);
        if (is_volatile)
        {
            const gchar *target;

            target = g_file_info_get_symlink_target (info);
            real_file = g_file_resolve_relative_path (volatile_file, target);
        }
    }

    g_object_unref (info);

    if (real_file == NULL)
    {
        real_file = g_object_ref (volatile_file);
    }

    return real_file;
}

static void copy_move_file (CopyMoveJob  *job,
                            GFile        *src,
                            GFile        *dest_dir,
                            gboolean      same_fs,
                            gboolean      unique_names,
                            char        **dest_fs_type,
                            SourceInfo   *source_info,
                            TransferInfo *transfer_info,
                            GHashTable   *debuting_files,
                            gboolean      overwrite,
                            gboolean     *skipped_file,
                            gboolean      readonly_source_fs);

typedef enum
{
    CREATE_DEST_DIR_RETRY,
    CREATE_DEST_DIR_FAILED,
    CREATE_DEST_DIR_SUCCESS
} CreateDestDirResult;

static CreateDestDirResult
create_dest_dir (CommonJob  *job,
                 GFile      *src,
                 GFile     **dest,
                 gboolean    same_fs,
                 char      **dest_fs_type)
{
    GError *error;
    GFile *new_dest, *dest_dir;
    char *primary, *secondary, *details;
    int response;
    gboolean handled_invalid_filename;
    gboolean res;

    handled_invalid_filename = *dest_fs_type != NULL;

retry:
    /* First create the directory, then copy stuff to it before
     *  copying the attributes, because we need to be sure we can write to it */

    error = NULL;
    res = g_file_make_directory (*dest, job->cancellable, &error);

    if (res)
    {
        GFile *real;

        real = map_possibly_volatile_file_to_real (*dest, job->cancellable, &error);
        if (real == NULL)
        {
            res = FALSE;
        }
        else
        {
            g_object_unref (*dest);
            *dest = real;
        }
    }

    if (!res)
    {
        g_autofree gchar *basename = NULL;

        if (IS_IO_ERROR (error, CANCELLED))
        {
            g_error_free (error);
            return CREATE_DEST_DIR_FAILED;
        }
        else if (IS_IO_ERROR (error, INVALID_FILENAME) &&
                 !handled_invalid_filename)
        {
            handled_invalid_filename = TRUE;

            g_assert (*dest_fs_type == NULL);

            dest_dir = g_file_get_parent (*dest);

            if (dest_dir != NULL)
            {
                *dest_fs_type = query_fs_type (dest_dir, job->cancellable);

                new_dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
                g_object_unref (dest_dir);

                if (!g_file_equal (*dest, new_dest))
                {
                    g_object_unref (*dest);
                    *dest = new_dest;
                    g_error_free (error);
                    return CREATE_DEST_DIR_RETRY;
                }
                else
                {
                    g_object_unref (new_dest);
                }
            }
        }

        primary = g_strdup (_("Error while copying."));
        details = NULL;
        basename = get_basename (src);

        if (IS_IO_ERROR (error, PERMISSION_DENIED))
        {
            secondary = g_strdup_printf (_("The folder “%s” cannot be copied because you do not "
                                           "have permissions to create it in the destination."),
                                         basename);
        }
        else
        {
            secondary = g_strdup_printf (_("There was an error creating the folder “%s”."),
                                         basename);
            details = error->message;
        }

        response = run_warning (job,
                                primary,
                                secondary,
                                details,
                                FALSE,
                                CANCEL, SKIP, RETRY,
                                NULL);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)
        {
            /* Skip: Do Nothing  */
        }
        else if (response == 2)
        {
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }
        return CREATE_DEST_DIR_FAILED;
    }
    nautilus_file_changes_queue_file_added (*dest);

    if (job->undo_info != NULL)
    {
        nautilus_file_undo_info_ext_add_origin_target_pair (NAUTILUS_FILE_UNDO_INFO_EXT (job->undo_info),
                                                            src, *dest);
    }

    return CREATE_DEST_DIR_SUCCESS;
}

/* a return value of FALSE means retry, i.e.
 * the destination has changed and the source
 * is expected to re-try the preceding
 * g_file_move() or g_file_copy() call with
 * the new destination.
 */
static gboolean
copy_move_directory (CopyMoveJob   *copy_job,
                     GFile         *src,
                     GFile        **dest,
                     gboolean       same_fs,
                     gboolean       create_dest,
                     char         **parent_dest_fs_type,
                     SourceInfo    *source_info,
                     TransferInfo  *transfer_info,
                     GHashTable    *debuting_files,
                     gboolean      *skipped_file,
                     gboolean       readonly_source_fs)
{
    g_autoptr (GFileInfo) src_info = NULL;
    GFileInfo *info;
    GError *error;
    GFile *src_file;
    GFileEnumerator *enumerator;
    char *primary, *secondary, *details;
    char *dest_fs_type;
    int response;
    gboolean skip_error;
    gboolean local_skipped_file;
    CommonJob *job;
    GFileCopyFlags flags;

    job = (CommonJob *) copy_job;
    *skipped_file = FALSE;

    if (create_dest)
    {
        g_autofree char *attrs_to_read = NULL;

        switch (create_dest_dir (job, src, dest, same_fs, parent_dest_fs_type))
        {
            case CREATE_DEST_DIR_RETRY:
            {
                /* next time copy_move_directory() is called,
                 * create_dest will be FALSE if a directory already
                 * exists under the new name (i.e. WOULD_RECURSE)
                 */
                return FALSE;
            }

            case CREATE_DEST_DIR_FAILED:
            {
                *skipped_file = TRUE;
                return TRUE;
            }

            case CREATE_DEST_DIR_SUCCESS:
            default:
            {
            }
            break;
        }

        if (debuting_files)
        {
            g_hash_table_replace (debuting_files, g_object_ref (*dest), GINT_TO_POINTER (TRUE));
        }

        flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;
        if (readonly_source_fs)
        {
            flags |= G_FILE_COPY_TARGET_DEFAULT_PERMS;
        }
        if (copy_job->is_move)
        {
            flags |= G_FILE_COPY_ALL_METADATA;
        }

        /* Ignore errors here. Failure to copy metadata is not a hard error */
        attrs_to_read = g_file_build_attribute_list_for_copy (*dest, flags, job->cancellable, NULL);
        if (attrs_to_read != NULL)
        {
            src_info = g_file_query_info (src, attrs_to_read, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, job->cancellable, NULL);
        }
    }

    local_skipped_file = FALSE;
    dest_fs_type = NULL;

    skip_error = should_skip_readdir_error (job, src);
retry:
    error = NULL;
    enumerator = g_file_enumerate_children (src,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            job->cancellable,
                                            &error);
    if (enumerator)
    {
        error = NULL;

        while (!job_aborted (job) &&
               (info = g_file_enumerator_next_file (enumerator, job->cancellable, skip_error ? NULL : &error)) != NULL)
        {
            src_file = g_file_get_child (src,
                                         g_file_info_get_name (info));
            copy_move_file (copy_job, src_file, *dest, same_fs, FALSE, &dest_fs_type,
                            source_info, transfer_info, NULL, FALSE, &local_skipped_file,
                            readonly_source_fs);

            if (local_skipped_file)
            {
                source_info_remove_file_from_count (src_file, job, source_info);
                report_copy_progress (copy_job, source_info, transfer_info);
            }

            g_object_unref (src_file);
            g_object_unref (info);
        }
        g_file_enumerator_close (enumerator, job->cancellable, NULL);
        g_object_unref (enumerator);

        if (error && IS_IO_ERROR (error, CANCELLED))
        {
            g_error_free (error);
        }
        else if (error)
        {
            g_autofree gchar *basename = NULL;

            if (copy_job->is_move)
            {
                primary = g_strdup (_("Error while moving."));
            }
            else
            {
                primary = g_strdup (_("Error while copying."));
            }
            details = NULL;
            basename = get_basename (src);

            if (IS_IO_ERROR (error, PERMISSION_DENIED))
            {
                secondary = g_strdup_printf (_("Files in the folder “%s” cannot be copied because you do "
                                               "not have permissions to see them."), basename);
            }
            else
            {
                secondary = g_strdup_printf (_("There was an error getting information about "
                                               "the files in the folder “%s”."),
                                             basename);
                details = error->message;
            }

            response = run_warning (job,
                                    primary,
                                    secondary,
                                    details,
                                    FALSE,
                                    CANCEL, _("_Skip files"),
                                    NULL);

            g_error_free (error);

            if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
            {
                abort_job (job);
            }
            else if (response == 1)
            {
                /* Skip: Do Nothing */
                local_skipped_file = TRUE;
            }
            else
            {
                g_assert_not_reached ();
            }
        }

        /* Count the copied directory as a file */
        transfer_info->num_files++;

        info = g_file_query_info (src,
                                  G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  job->cancellable,
                                  NULL);

        g_warn_if_fail (info != NULL);

        if (info != NULL)
        {
            transfer_info->num_bytes += g_file_info_get_size (info);

            g_object_unref (info);
        }

        report_copy_progress (copy_job, source_info, transfer_info);

        if (debuting_files)
        {
            g_hash_table_replace (debuting_files, g_object_ref (*dest), GINT_TO_POINTER (create_dest));
        }
    }
    else if (IS_IO_ERROR (error, CANCELLED))
    {
        g_error_free (error);
    }
    else
    {
        g_autofree gchar *basename = NULL;

        if (copy_job->is_move)
        {
            primary = g_strdup (_("Error while moving."));
        }
        else
        {
            primary = g_strdup (_("Error while copying."));
        }
        details = NULL;
        basename = get_basename (src);

        if (IS_IO_ERROR (error, PERMISSION_DENIED))
        {
            secondary = g_strdup_printf (_("The folder “%s” cannot be copied because you do not have "
                                           "permissions to read it."), basename);
        }
        else
        {
            secondary = g_strdup_printf (_("There was an error reading the folder “%s”."),
                                         basename);

            details = error->message;
        }

        response = run_warning (job,
                                primary,
                                secondary,
                                details,
                                FALSE,
                                CANCEL, SKIP, RETRY,
                                NULL);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)
        {
            /* Skip: Do Nothing  */
            *skipped_file = TRUE;
        }
        else if (response == 2)
        {
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }
    }

    if (src_info != NULL)
    {
        /* Ignore errors here. Failure to copy metadata is not a hard error */
        g_file_set_attributes_from_info (*dest,
                                         src_info,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         job->cancellable,
                                         NULL);
    }

    if (!job_aborted (job) && copy_job->is_move &&
        /* Don't delete source if there was a skipped file */
        !*skipped_file &&
        !local_skipped_file)
    {
        if (!g_file_delete (src, job->cancellable, &error))
        {
            g_autofree gchar *basename = NULL;

            if (job->skip_all_error)
            {
                *skipped_file = TRUE;
                goto skip;
            }
            basename = get_basename (src);
            primary = g_strdup_printf (_("Error while moving “%s”."), basename);
            secondary = g_strdup (_("Could not remove the source folder."));
            details = error->message;

            response = run_cancel_or_skip_warning (job,
                                                   primary,
                                                   secondary,
                                                   details,
                                                   source_info->num_files,
                                                   source_info->num_files - transfer_info->num_files);

            if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
            {
                abort_job (job);
            }
            else if (response == 1)                 /* skip all */
            {
                job->skip_all_error = TRUE;
                *skipped_file = TRUE;
            }
            else if (response == 2)                 /* skip */
            {
                *skipped_file = TRUE;
            }
            else
            {
                g_assert_not_reached ();
            }

skip:
            g_error_free (error);
        }
    }

    g_free (dest_fs_type);
    return TRUE;
}


typedef struct
{
    CommonJob *job;
    GFile *source;
} DeleteExistingFileData;

typedef struct
{
    CopyMoveJob *job;
    goffset last_size;
    SourceInfo *source_info;
    TransferInfo *transfer_info;
} ProgressData;

static void
copy_file_progress_callback (goffset  current_num_bytes,
                             goffset  total_num_bytes,
                             gpointer user_data)
{
    ProgressData *pdata;
    goffset new_size;

    pdata = user_data;

    if (current_num_bytes != 0 &&
        current_num_bytes != total_num_bytes)
    {
        pdata->transfer_info->partial_progress = TRUE;
    }

    new_size = current_num_bytes - pdata->last_size;

    if (new_size > 0)
    {
        pdata->transfer_info->num_bytes += new_size;
        pdata->last_size = current_num_bytes;
        report_copy_progress (pdata->job,
                              pdata->source_info,
                              pdata->transfer_info);
    }
}

static gboolean
test_dir_is_parent (GFile *child,
                    GFile *root)
{
    GFile *f, *tmp;

    f = g_file_dup (child);
    while (f)
    {
        if (g_file_equal (f, root))
        {
            g_object_unref (f);
            return TRUE;
        }
        tmp = f;
        f = g_file_get_parent (f);
        g_object_unref (tmp);
    }
    if (f)
    {
        g_object_unref (f);
    }
    return FALSE;
}

static char *
query_fs_type (GFile        *file,
               GCancellable *cancellable)
{
    GFileInfo *fsinfo;
    char *ret;

    ret = NULL;

    fsinfo = g_file_query_filesystem_info (file,
                                           G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
                                           cancellable,
                                           NULL);
    if (fsinfo != NULL)
    {
        ret = g_strdup (g_file_info_get_attribute_string (fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
        g_object_unref (fsinfo);
    }

    if (ret == NULL)
    {
        /* ensure that we don't attempt to query
         * the FS type for each file in a given
         * directory, if it can't be queried. */
        ret = g_strdup ("");
    }

    return ret;
}

static FileConflictResponse *
handle_copy_move_conflict (CommonJob *job,
                           GFile     *src,
                           GFile     *dest,
                           GFile     *dest_dir)
{
    FileConflictResponse *response;
    g_autofree gchar *basename = NULL;
    g_autoptr (GFile) suggested_file = NULL;
    g_autofree gchar *suggestion = NULL;

    g_timer_stop (job->time);
    nautilus_progress_info_pause (job->progress);

    basename = g_file_get_basename (dest);
    suggested_file = nautilus_generate_unique_file_in_directory (dest_dir, basename);
    suggestion = g_file_get_basename (suggested_file);

    response = copy_move_conflict_ask_user_action (job->parent_window,
                                                   src,
                                                   dest,
                                                   dest_dir,
                                                   suggestion);

    nautilus_progress_info_resume (job->progress);
    g_timer_continue (job->time);

    return response;
}

static GFile *
get_target_file_for_display_name (GFile       *dir,
                                  const gchar *name)
{
    GFile *dest;

    dest = NULL;
    dest = g_file_get_child_for_display_name (dir, name, NULL);

    if (dest == NULL)
    {
        dest = g_file_get_child (dir, name);
    }

    return dest;
}

/* This is a workaround to resolve broken conflict dialog for google-drive
 * locations. This is needed to provide POSIX-like behavior for google-drive
 * filesystem, where each file has an unique identificator that is not tied to
 * its display_name. See the following MR for more details:
 * https://gitlab.gnome.org/GNOME/nautilus/merge_requests/514.
 */
static GFile *
get_target_file_from_source_display_name (CopyMoveJob *copy_job,
                                          GFile       *src,
                                          GFile       *dir)
{
    CommonJob *job;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInfo) info = NULL;
    gchar *primary, *secondary;
    GFile *dest = NULL;

    job = (CommonJob *) copy_job;

    info = g_file_query_info (src, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, &error);
    if (info == NULL)
    {
        if (copy_job->is_move)
        {
            primary = g_strdup (_("Error while moving."));
        }
        else
        {
            primary = g_strdup (_("Error while copying."));
        }
        secondary = g_strdup (_("There was an error getting information about the source."));

        run_error (job,
                   primary,
                   secondary,
                   error->message,
                   FALSE,
                   CANCEL,
                   NULL);

        abort_job (job);
    }
    else
    {
        dest = get_target_file_for_display_name (dir, g_file_info_get_display_name (info));
    }

    return dest;
}


/* Debuting files is non-NULL only for toplevel items */
static void
copy_move_file (CopyMoveJob   *copy_job,
                GFile         *src,
                GFile         *dest_dir,
                gboolean       same_fs,
                gboolean       unique_names,
                char         **dest_fs_type,
                SourceInfo    *source_info,
                TransferInfo  *transfer_info,
                GHashTable    *debuting_files,
                gboolean       overwrite,
                gboolean      *skipped_file,
                gboolean       readonly_source_fs)
{
    GFile *dest, *new_dest;
    g_autofree gchar *dest_uri = NULL;
    GError *error;
    GFileCopyFlags flags;
    char *primary, *secondary, *details;
    ProgressData pdata;
    gboolean would_recurse;
    CommonJob *job;
    gboolean res;
    int unique_name_nr;
    gboolean handled_invalid_filename;

    job = (CommonJob *) copy_job;

    *skipped_file = FALSE;

    if (should_skip_file (job, src))
    {
        *skipped_file = TRUE;
        return;
    }

    unique_name_nr = 1;

    /* another file in the same directory might have handled the invalid
     * filename condition for us
     */
    handled_invalid_filename = *dest_fs_type != NULL;

    if (unique_names)
    {
        dest = get_unique_target_file (src, dest_dir, same_fs, *dest_fs_type, unique_name_nr++);
    }
    else if (copy_job->target_name != NULL)
    {
        dest = get_target_file_with_custom_name (src, dest_dir, *dest_fs_type, same_fs,
                                                 copy_job->target_name);
    }
    else if (g_file_has_uri_scheme (src, "google-drive") &&
             g_file_has_uri_scheme (dest_dir, "google-drive"))
    {
        dest = get_target_file_from_source_display_name (copy_job, src, dest_dir);
        if (dest == NULL)
        {
            *skipped_file = TRUE;
            return;
        }
    }
    else
    {
        dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
    }

    /* Don't allow recursive move/copy into itself.
     * (We would get a file system error if we proceeded but it is nicer to
     * detect and report it at this level) */
    if (test_dir_is_parent (dest_dir, src))
    {
        int response;

        if (job->skip_all_error)
        {
            goto out;
        }

        /*  the run_warning() frees all strings passed in automatically  */
        primary = copy_job->is_move ? g_strdup (_("You cannot move a folder into itself."))
                  : g_strdup (_("You cannot copy a folder into itself."));
        secondary = g_strdup (_("The destination folder is inside the source folder."));

        response = run_cancel_or_skip_warning (job,
                                               primary,
                                               secondary,
                                               NULL,
                                               source_info->num_files,
                                               source_info->num_files - transfer_info->num_files);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)             /* skip all */
        {
            job->skip_all_error = TRUE;
        }
        else if (response == 2)             /* skip */
        {               /* do nothing */
        }
        else
        {
            g_assert_not_reached ();
        }

        goto out;
    }

    /* Don't allow copying over the source or one of the parents of the source.
     */
    if (test_dir_is_parent (src, dest))
    {
        int response;

        if (job->skip_all_error)
        {
            goto out;
        }

        /*  the run_warning() frees all strings passed in automatically  */
        primary = copy_job->is_move ? g_strdup (_("You cannot move a file over itself."))
                  : g_strdup (_("You cannot copy a file over itself."));
        secondary = g_strdup (_("The source file would be overwritten by the destination."));

        response = run_cancel_or_skip_warning (job,
                                               primary,
                                               secondary,
                                               NULL,
                                               source_info->num_files,
                                               source_info->num_files - transfer_info->num_files);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)             /* skip all */
        {
            job->skip_all_error = TRUE;
        }
        else if (response == 2)             /* skip */
        {               /* do nothing */
        }
        else
        {
            g_assert_not_reached ();
        }

        goto out;
    }


retry:

    error = NULL;
    flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;
    if (overwrite)
    {
        flags |= G_FILE_COPY_OVERWRITE;
    }
    if (readonly_source_fs)
    {
        flags |= G_FILE_COPY_TARGET_DEFAULT_PERMS;
    }

    pdata.job = copy_job;
    pdata.last_size = 0;
    pdata.source_info = source_info;
    pdata.transfer_info = transfer_info;

    if (copy_job->is_move)
    {
        res = g_file_move (src, dest,
                           flags,
                           job->cancellable,
                           copy_file_progress_callback,
                           &pdata,
                           &error);
    }
    else
    {
        res = g_file_copy (src, dest,
                           flags,
                           job->cancellable,
                           copy_file_progress_callback,
                           &pdata,
                           &error);
    }

    if (res)
    {
        GFile *real;

        real = map_possibly_volatile_file_to_real (dest, job->cancellable, &error);
        if (real == NULL)
        {
            res = FALSE;
        }
        else
        {
            g_object_unref (dest);
            dest = real;
        }
    }

    if (res)
    {
        transfer_info->num_files++;
        report_copy_progress (copy_job, source_info, transfer_info);

        if (debuting_files)
        {
            dest_uri = g_file_get_uri (dest);

            g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
        }
        if (copy_job->is_move)
        {
            nautilus_file_changes_queue_file_moved (src, dest);
        }
        else
        {
            nautilus_file_changes_queue_file_added (dest);
        }

        if (job->undo_info != NULL)
        {
            nautilus_file_undo_info_ext_add_origin_target_pair (NAUTILUS_FILE_UNDO_INFO_EXT (job->undo_info),
                                                                src, dest);
        }

        g_object_unref (dest);
        return;
    }

    if (!handled_invalid_filename &&
        IS_IO_ERROR (error, INVALID_FILENAME))
    {
        handled_invalid_filename = TRUE;

        g_assert (*dest_fs_type == NULL);
        *dest_fs_type = query_fs_type (dest_dir, job->cancellable);

        if (unique_names)
        {
            new_dest = get_unique_target_file (src, dest_dir, same_fs, *dest_fs_type, unique_name_nr);
        }
        else
        {
            new_dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
        }

        if (!g_file_equal (dest, new_dest))
        {
            g_object_unref (dest);
            dest = new_dest;

            g_error_free (error);
            goto retry;
        }
        else
        {
            g_object_unref (new_dest);
        }
    }

    /* Conflict */
    if (!overwrite &&
        IS_IO_ERROR (error, EXISTS))
    {
        gboolean source_is_directory;
        gboolean destination_is_directory;
        gboolean is_merge;
        FileConflictResponse *response;

        source_is_directory = is_dir (src);
        destination_is_directory = is_dir (dest);

        g_error_free (error);

        if (unique_names)
        {
            g_object_unref (dest);
            dest = get_unique_target_file (src, dest_dir, same_fs, *dest_fs_type, unique_name_nr++);
            goto retry;
        }

        is_merge = FALSE;

        if (source_is_directory && destination_is_directory)
        {
            is_merge = TRUE;
        }
        else if (!source_is_directory && destination_is_directory)
        {
            /* Any sane backend will fail with G_IO_ERROR_IS_DIRECTORY. */
            overwrite = TRUE;
            goto retry;
        }

        if ((is_merge && job->merge_all) ||
            (!is_merge && job->replace_all))
        {
            overwrite = TRUE;
            goto retry;
        }

        if (job->skip_all_conflict)
        {
            goto out;
        }

        response = handle_copy_move_conflict (job, src, dest, dest_dir);

        if (response->id == GTK_RESPONSE_CANCEL ||
            response->id == GTK_RESPONSE_DELETE_EVENT)
        {
            file_conflict_response_free (response);
            abort_job (job);
        }
        else if (response->id == CONFLICT_RESPONSE_SKIP)
        {
            if (response->apply_to_all)
            {
                job->skip_all_conflict = TRUE;
            }
            file_conflict_response_free (response);
        }
        else if (response->id == CONFLICT_RESPONSE_REPLACE)             /* merge/replace */
        {
            if (response->apply_to_all)
            {
                if (is_merge)
                {
                    job->merge_all = TRUE;
                }
                else
                {
                    job->replace_all = TRUE;
                }
            }
            overwrite = TRUE;
            file_conflict_response_free (response);
            goto retry;
        }
        else if (response->id == CONFLICT_RESPONSE_RENAME)
        {
            g_object_unref (dest);
            dest = get_target_file_for_display_name (dest_dir,
                                                     response->new_name);
            file_conflict_response_free (response);
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }
    }
    /* Needs to recurse */
    else if (IS_IO_ERROR (error, WOULD_RECURSE) ||
             IS_IO_ERROR (error, WOULD_MERGE))
    {
        gboolean is_merge;

        is_merge = error->code == G_IO_ERROR_WOULD_MERGE;
        would_recurse = error->code == G_IO_ERROR_WOULD_RECURSE;
        g_error_free (error);

        if (overwrite && would_recurse)
        {
            error = NULL;

            /* Copying a dir onto file, first remove the file */
            if (!g_file_delete (dest, job->cancellable, &error) &&
                !IS_IO_ERROR (error, NOT_FOUND))
            {
                g_autofree gchar *basename = NULL;
                g_autofree gchar *filename = NULL;
                int response;

                if (job->skip_all_error)
                {
                    g_error_free (error);
                    goto out;
                }

                basename = get_basename (src);
                if (copy_job->is_move)
                {
                    primary = g_strdup_printf (_("Error while moving “%s”."), basename);
                }
                else
                {
                    primary = g_strdup_printf (_("Error while copying “%s”."), basename);
                }
                filename = get_truncated_parse_name (dest_dir);
                secondary = g_strdup_printf (_("Could not remove the already existing file "
                                               "with the same name in %s."),
                                             filename);
                details = error->message;

                /* setting TRUE on show_all here, as we could have
                 * another error on the same file later.
                 */
                response = run_warning (job,
                                        primary,
                                        secondary,
                                        details,
                                        TRUE,
                                        CANCEL, SKIP_ALL, SKIP,
                                        NULL);

                g_error_free (error);

                if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
                {
                    abort_job (job);
                }
                else if (response == 1)                     /* skip all */
                {
                    job->skip_all_error = TRUE;
                }
                else if (response == 2)                     /* skip */
                {                       /* do nothing */
                }
                else
                {
                    g_assert_not_reached ();
                }
                goto out;
            }
            if (error)
            {
                g_error_free (error);
                error = NULL;
            }
            nautilus_file_changes_queue_file_removed (dest);
        }

        if (is_merge)
        {
            /* On merge we now write in the target directory, which may not
             *   be in the same directory as the source, even if the parent is
             *   (if the merged directory is a mountpoint). This could cause
             *   problems as we then don't transcode filenames.
             *   We just set same_fs to FALSE which is safe but a bit slower. */
            same_fs = FALSE;
        }

        if (!copy_move_directory (copy_job, src, &dest, same_fs,
                                  would_recurse, dest_fs_type,
                                  source_info, transfer_info,
                                  debuting_files, skipped_file,
                                  readonly_source_fs))
        {
            /* destination changed, since it was an invalid file name */
            g_assert (*dest_fs_type != NULL);
            handled_invalid_filename = TRUE;
            goto retry;
        }

        g_object_unref (dest);
        return;
    }
    else if (IS_IO_ERROR (error, CANCELLED))
    {
        g_error_free (error);
    }
    /* Other error */
    else
    {
        g_autofree gchar *basename = NULL;
        g_autofree gchar *filename = NULL;
        int response;

        if (job->skip_all_error)
        {
            g_error_free (error);
            goto out;
        }
        basename = get_basename (src);
        primary = g_strdup_printf (_("Error while copying “%s”."), basename);
        filename = get_truncated_parse_name (dest_dir);
        secondary = g_strdup_printf (_("There was an error copying the file into %s."),
                                     filename);
        details = error->message;

        response = run_cancel_or_skip_warning (job,
                                               primary,
                                               secondary,
                                               details,
                                               source_info->num_files,
                                               source_info->num_files - transfer_info->num_files);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)             /* skip all */
        {
            job->skip_all_error = TRUE;
        }
        else if (response == 2)             /* skip */
        {               /* do nothing */
        }
        else
        {
            g_assert_not_reached ();
        }
    }
out:
    *skipped_file = TRUE;     /* Or aborted, but same-same */
    g_object_unref (dest);
}

static void
copy_files (CopyMoveJob  *job,
            const char   *dest_fs_id,
            SourceInfo   *source_info,
            TransferInfo *transfer_info)
{
    CommonJob *common;
    GList *l;
    GFile *src;
    gboolean same_fs;
    int i;
    gboolean skipped_file;
    gboolean unique_names;
    GFile *dest;
    GFile *source_dir;
    char *dest_fs_type;
    GFileInfo *inf;
    gboolean readonly_source_fs;

    dest_fs_type = NULL;
    readonly_source_fs = FALSE;

    common = &job->common;

    report_copy_progress (job, source_info, transfer_info);

    /* Query the source dir, not the file because if it's a symlink we'll follow it */
    source_dir = g_file_get_parent ((GFile *) job->files->data);
    if (source_dir)
    {
        inf = g_file_query_filesystem_info (source_dir, "filesystem::readonly", NULL, NULL);
        if (inf != NULL)
        {
            readonly_source_fs = g_file_info_get_attribute_boolean (inf, "filesystem::readonly");
            g_object_unref (inf);
        }
        g_object_unref (source_dir);
    }

    unique_names = (job->destination == NULL);
    i = 0;
    for (l = job->files;
         l != NULL && !job_aborted (common);
         l = l->next)
    {
        src = l->data;

        same_fs = FALSE;
        if (dest_fs_id)
        {
            same_fs = has_fs_id (src, dest_fs_id);
        }

        if (job->destination)
        {
            dest = g_object_ref (job->destination);
        }
        else
        {
            dest = g_file_get_parent (src);
        }
        if (dest)
        {
            skipped_file = FALSE;
            copy_move_file (job, src, dest,
                            same_fs, unique_names,
                            &dest_fs_type,
                            source_info, transfer_info,
                            job->debuting_files,
                            FALSE, &skipped_file,
                            readonly_source_fs);
            g_object_unref (dest);

            if (skipped_file)
            {
                source_info_remove_file_from_count (src, common, source_info);
                report_copy_progress (job, source_info, transfer_info);
            }
        }
        i++;
    }

    g_free (dest_fs_type);
}

static void
copy_task_done (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    CopyMoveJob *job;

    job = user_data;
    if (job->done_callback)
    {
        job->done_callback (job->debuting_files,
                            !job_aborted ((CommonJob *) job),
                            job->done_callback_data);
    }

    g_list_free_full (job->files, g_object_unref);
    if (job->destination)
    {
        g_object_unref (job->destination);
    }
    g_hash_table_unref (job->debuting_files);
    g_free (job->target_name);

    g_clear_object (&job->fake_display_source);

    finalize_common ((CommonJob *) job);

    nautilus_file_changes_consume_changes (TRUE);
}

static CopyMoveJob *
copy_job_setup (GList                          *files,
                GFile                          *target_dir,
                GtkWindow                      *parent_window,
                NautilusFileOperationsDBusData *dbus_data,
                NautilusCopyCallback            done_callback,
                gpointer                        done_callback_data)
{
    CopyMoveJob *job;

    job = op_job_new (CopyMoveJob, parent_window, dbus_data);
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
    job->destination = g_object_ref (target_dir);
    /* Need to indicate the destination for the operation notification open
     * button. */
    nautilus_progress_info_set_destination (((CommonJob *) job)->progress, target_dir);
    job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);

    return job;
}

static void
nautilus_file_operations_copy (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
    CopyMoveJob *job;
    CommonJob *common;
    g_auto (SourceInfo) source_info = SOURCE_INFO_INIT;
    TransferInfo transfer_info;
    g_autofree char *dest_fs_id = NULL;
    GFile *dest;

    job = task_data;
    common = &job->common;

    if (g_strcmp0 (g_getenv ("RUNNING_TESTS"), "TRUE"))
    {
        inhibit_power_manager ((CommonJob *) job, _("Copying Files"));
    }

    if (!nautilus_file_undo_manager_is_operating ())
    {
        g_autoptr (GFile) src_dir = NULL;

        src_dir = g_file_get_parent (job->files->data);
        /* In the case of duplicate, the undo_info is already set, so we don't want to
         * overwrite it wrongfully.
         */
        if (job->common.undo_info == NULL)
        {
            job->common.undo_info = nautilus_file_undo_info_ext_new (NAUTILUS_FILE_UNDO_OP_COPY,
                                                                     g_list_length (job->files),
                                                                     src_dir, job->destination);
        }
    }

    nautilus_progress_info_start (job->common.progress);

    scan_sources (job->files,
                  &source_info,
                  common,
                  OP_KIND_COPY);
    if (job_aborted (common))
    {
        return;
    }

    if (job->destination)
    {
        dest = g_object_ref (job->destination);
    }
    else
    {
        /* Duplication, no dest,
         * use source for free size, etc
         */
        dest = g_file_get_parent (job->files->data);
    }

    verify_destination (&job->common,
                        dest,
                        &dest_fs_id,
                        source_info.num_bytes);
    g_object_unref (dest);
    if (job_aborted (common))
    {
        return;
    }

    g_timer_start (job->common.time);

    memset (&transfer_info, 0, sizeof (transfer_info));
    copy_files (job,
                dest_fs_id,
                &source_info, &transfer_info);
}

void
nautilus_file_operations_copy_sync (GList *files,
                                    GFile *target_dir)
{
    GTask *task;
    CopyMoveJob *job;

    job = copy_job_setup (files,
                          target_dir,
                          NULL,
                          NULL,
                          NULL,
                          NULL);

    task = g_task_new (NULL, job->common.cancellable, NULL, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread_sync (task, nautilus_file_operations_copy);
    g_object_unref (task);
    /* Since g_task_run_in_thread_sync doesn't work with callbacks (in this case not reaching
     * copy_task_done) we need to set up the undo information ourselves.
     */
    copy_task_done (NULL, NULL, job);
}

void
nautilus_file_operations_copy_async (GList                          *files,
                                     GFile                          *target_dir,
                                     GtkWindow                      *parent_window,
                                     NautilusFileOperationsDBusData *dbus_data,
                                     NautilusCopyCallback            done_callback,
                                     gpointer                        done_callback_data)
{
    GTask *task;
    CopyMoveJob *job;

    job = copy_job_setup (files,
                          target_dir,
                          parent_window,
                          dbus_data,
                          done_callback,
                          done_callback_data);

    task = g_task_new (NULL, job->common.cancellable, copy_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, nautilus_file_operations_copy);
    g_object_unref (task);
}

static void
report_preparing_move_progress (CopyMoveJob *move_job,
                                int          total,
                                int          left)
{
    CommonJob *job;
    g_autofree gchar *basename = NULL;

    job = (CommonJob *) move_job;
    basename = get_basename (move_job->destination);

    nautilus_progress_info_take_status (job->progress,
                                        g_strdup_printf (_("Preparing to move to “%s”"),
                                                         basename));

    nautilus_progress_info_take_details (job->progress,
                                         g_strdup_printf (ngettext ("Preparing to move %'d file",
                                                                    "Preparing to move %'d files",
                                                                    left),
                                                          left));

    nautilus_progress_info_pulse_progress (job->progress);
}

typedef struct
{
    GFile *file;
    gboolean overwrite;
} MoveFileCopyFallback;

static MoveFileCopyFallback *
move_copy_file_callback_new (GFile    *file,
                             gboolean  overwrite)
{
    MoveFileCopyFallback *fallback;

    fallback = g_new (MoveFileCopyFallback, 1);
    fallback->file = file;
    fallback->overwrite = overwrite;

    return fallback;
}

static GList *
get_files_from_fallbacks (GList *fallbacks)
{
    MoveFileCopyFallback *fallback;
    GList *res, *l;

    res = NULL;
    for (l = fallbacks; l != NULL; l = l->next)
    {
        fallback = l->data;
        res = g_list_prepend (res, fallback->file);
    }
    return g_list_reverse (res);
}

static void
move_file_prepare (CopyMoveJob  *move_job,
                   GFile        *src,
                   GFile        *dest_dir,
                   gboolean      same_fs,
                   char        **dest_fs_type,
                   GHashTable   *debuting_files,
                   GList       **fallback_files,
                   int           files_left)
{
    GFile *dest, *new_dest;
    g_autofree gchar *dest_uri = NULL;
    GError *error;
    CommonJob *job;
    gboolean overwrite;
    char *primary, *secondary, *details;
    GFileCopyFlags flags;
    MoveFileCopyFallback *fallback;
    gboolean handled_invalid_filename;

    overwrite = FALSE;
    handled_invalid_filename = *dest_fs_type != NULL;

    job = (CommonJob *) move_job;

    if (g_file_has_uri_scheme (src, "google-drive") &&
        g_file_has_uri_scheme (dest_dir, "google-drive"))
    {
        dest = get_target_file_from_source_display_name (move_job, src, dest_dir);
        if (dest == NULL)
        {
            return;
        }
    }
    else
    {
        dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
    }


    /* Don't allow recursive move/copy into itself.
     * (We would get a file system error if we proceeded but it is nicer to
     * detect and report it at this level) */
    if (test_dir_is_parent (dest_dir, src))
    {
        int response;

        if (job->skip_all_error)
        {
            goto out;
        }

        /*  the run_warning() frees all strings passed in automatically  */
        primary = move_job->is_move ? g_strdup (_("You cannot move a folder into itself."))
                  : g_strdup (_("You cannot copy a folder into itself."));
        secondary = g_strdup (_("The destination folder is inside the source folder."));

        response = run_warning (job,
                                primary,
                                secondary,
                                NULL,
                                files_left > 1,
                                CANCEL, SKIP_ALL, SKIP,
                                NULL);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)             /* skip all */
        {
            job->skip_all_error = TRUE;
        }
        else if (response == 2)             /* skip */
        {               /* do nothing */
        }
        else
        {
            g_assert_not_reached ();
        }

        goto out;
    }

retry:

    flags = G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_NO_FALLBACK_FOR_MOVE;
    if (overwrite)
    {
        flags |= G_FILE_COPY_OVERWRITE;
    }

    error = NULL;
    if (g_file_move (src, dest,
                     flags,
                     job->cancellable,
                     NULL,
                     NULL,
                     &error))
    {
        if (debuting_files)
        {
            g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
        }

        nautilus_file_changes_queue_file_moved (src, dest);

        dest_uri = g_file_get_uri (dest);

        if (job->undo_info != NULL)
        {
            nautilus_file_undo_info_ext_add_origin_target_pair (NAUTILUS_FILE_UNDO_INFO_EXT (job->undo_info),
                                                                src, dest);
        }

        return;
    }

    if (IS_IO_ERROR (error, INVALID_FILENAME) &&
        !handled_invalid_filename)
    {
        g_error_free (error);

        handled_invalid_filename = TRUE;

        g_assert (*dest_fs_type == NULL);
        *dest_fs_type = query_fs_type (dest_dir, job->cancellable);

        new_dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
        if (!g_file_equal (dest, new_dest))
        {
            g_object_unref (dest);
            dest = new_dest;
            goto retry;
        }
        else
        {
            g_object_unref (new_dest);
        }
    }
    /* Conflict */
    else if (!overwrite &&
             IS_IO_ERROR (error, EXISTS))
    {
        gboolean source_is_directory;
        gboolean destination_is_directory;
        gboolean is_merge;
        FileConflictResponse *response;

        source_is_directory = is_dir (src);
        destination_is_directory = is_dir (dest);

        g_error_free (error);

        is_merge = FALSE;
        if (source_is_directory && destination_is_directory)
        {
            is_merge = TRUE;
        }
        else if (!source_is_directory && destination_is_directory)
        {
            /* Any sane backend will fail with G_IO_ERROR_IS_DIRECTORY. */
            overwrite = TRUE;
            goto retry;
        }

        if ((is_merge && job->merge_all) ||
            (!is_merge && job->replace_all))
        {
            overwrite = TRUE;
            goto retry;
        }

        if (job->skip_all_conflict)
        {
            goto out;
        }

        response = handle_copy_move_conflict (job, src, dest, dest_dir);

        if (response->id == GTK_RESPONSE_CANCEL ||
            response->id == GTK_RESPONSE_DELETE_EVENT)
        {
            file_conflict_response_free (response);
            abort_job (job);
        }
        else if (response->id == CONFLICT_RESPONSE_SKIP)
        {
            if (response->apply_to_all)
            {
                job->skip_all_conflict = TRUE;
            }
            file_conflict_response_free (response);
        }
        else if (response->id == CONFLICT_RESPONSE_REPLACE)             /* merge/replace */
        {
            if (response->apply_to_all)
            {
                if (is_merge)
                {
                    job->merge_all = TRUE;
                }
                else
                {
                    job->replace_all = TRUE;
                }
            }
            overwrite = TRUE;
            file_conflict_response_free (response);
            goto retry;
        }
        else if (response->id == CONFLICT_RESPONSE_RENAME)
        {
            g_object_unref (dest);
            dest = get_target_file_for_display_name (dest_dir,
                                                     response->new_name);
            file_conflict_response_free (response);
            goto retry;
        }
        else
        {
            g_assert_not_reached ();
        }
    }
    else if (IS_IO_ERROR (error, WOULD_RECURSE) ||
             IS_IO_ERROR (error, WOULD_MERGE) ||
             IS_IO_ERROR (error, NOT_SUPPORTED))
    {
        g_error_free (error);

        fallback = move_copy_file_callback_new (src,
                                                overwrite);
        *fallback_files = g_list_prepend (*fallback_files, fallback);
    }
    else if (IS_IO_ERROR (error, CANCELLED))
    {
        g_error_free (error);
    }
    /* Other error */
    else
    {
        g_autofree gchar *basename = NULL;
        g_autofree gchar *filename = NULL;
        int response;

        if (job->skip_all_error)
        {
            g_error_free (error);
            goto out;
        }
        basename = get_basename (src);
        primary = g_strdup_printf (_("Error while moving “%s”."), basename);
        filename = get_truncated_parse_name (dest_dir);
        secondary = g_strdup_printf (_("There was an error moving the file into %s."),
                                     filename);

        details = error->message;

        response = run_warning (job,
                                primary,
                                secondary,
                                details,
                                files_left > 1,
                                CANCEL, SKIP_ALL, SKIP,
                                NULL);

        g_error_free (error);

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (job);
        }
        else if (response == 1)             /* skip all */
        {
            job->skip_all_error = TRUE;
        }
        else if (response == 2)             /* skip */
        {               /* do nothing */
        }
        else
        {
            g_assert_not_reached ();
        }
    }

out:
    g_object_unref (dest);
}

static void
move_files_prepare (CopyMoveJob  *job,
                    const char   *dest_fs_id,
                    char        **dest_fs_type,
                    GList       **fallbacks)
{
    CommonJob *common;
    GList *l;
    GFile *src;
    gboolean same_fs;
    int i;
    int total, left;

    common = &job->common;

    total = left = g_list_length (job->files);

    report_preparing_move_progress (job, total, left);

    i = 0;
    for (l = job->files;
         l != NULL && !job_aborted (common);
         l = l->next)
    {
        src = l->data;

        same_fs = FALSE;
        if (dest_fs_id)
        {
            same_fs = has_fs_id (src, dest_fs_id);
        }

        move_file_prepare (job, src, job->destination,
                           same_fs, dest_fs_type,
                           job->debuting_files,
                           fallbacks,
                           left);
        report_preparing_move_progress (job, total, --left);
        i++;
    }

    *fallbacks = g_list_reverse (*fallbacks);
}

static void
move_files (CopyMoveJob   *job,
            GList         *fallbacks,
            const char    *dest_fs_id,
            char         **dest_fs_type,
            SourceInfo    *source_info,
            TransferInfo  *transfer_info)
{
    CommonJob *common;
    GList *l;
    GFile *src;
    gboolean same_fs;
    int i;
    gboolean skipped_file;
    MoveFileCopyFallback *fallback;
    common = &job->common;

    report_copy_progress (job, source_info, transfer_info);

    i = 0;
    for (l = fallbacks;
         l != NULL && !job_aborted (common);
         l = l->next)
    {
        fallback = l->data;
        src = fallback->file;

        same_fs = FALSE;
        if (dest_fs_id)
        {
            same_fs = has_fs_id (src, dest_fs_id);
        }

        /* Set overwrite to true, as the user has
         *  selected overwrite on all toplevel items */
        skipped_file = FALSE;
        copy_move_file (job, src, job->destination,
                        same_fs, FALSE, dest_fs_type,
                        source_info, transfer_info,
                        job->debuting_files,
                        fallback->overwrite, &skipped_file, FALSE);
        i++;

        if (skipped_file)
        {
            source_info_remove_file_from_count (src, common, source_info);
            report_copy_progress (job, source_info, transfer_info);
        }
    }
}


static void
move_task_done (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    CopyMoveJob *job;

    job = user_data;
    if (job->done_callback)
    {
        job->done_callback (job->debuting_files,
                            !job_aborted ((CommonJob *) job),
                            job->done_callback_data);
    }

    g_list_free_full (job->files, g_object_unref);
    g_object_unref (job->destination);
    g_hash_table_unref (job->debuting_files);

    finalize_common ((CommonJob *) job);

    nautilus_file_changes_consume_changes (TRUE);
}

static CopyMoveJob *
move_job_setup (GList                          *files,
                GFile                          *target_dir,
                GtkWindow                      *parent_window,
                NautilusFileOperationsDBusData *dbus_data,
                NautilusCopyCallback            done_callback,
                gpointer                        done_callback_data)
{
    CopyMoveJob *job;

    job = op_job_new (CopyMoveJob, parent_window, dbus_data);
    job->is_move = TRUE;
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
    job->destination = g_object_ref (target_dir);

    /* Need to indicate the destination for the operation notification open
     * button. */
    nautilus_progress_info_set_destination (((CommonJob *) job)->progress, job->destination);
    job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);

    return job;
}

void
nautilus_file_operations_move_sync (GList *files,
                                    GFile *target_dir)
{
    GTask *task;
    CopyMoveJob *job;

    job = move_job_setup (files, target_dir, NULL, NULL, NULL, NULL);
    task = g_task_new (NULL, job->common.cancellable, NULL, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread_sync (task, nautilus_file_operations_move);
    g_object_unref (task);
    /* Since g_task_run_in_thread_sync doesn't work with callbacks (in this case not reaching
     * move_task_done) we need to set up the undo information ourselves.
     */
    move_task_done (NULL, NULL, job);
}

void
nautilus_file_operations_move_async (GList                          *files,
                                     GFile                          *target_dir,
                                     GtkWindow                      *parent_window,
                                     NautilusFileOperationsDBusData *dbus_data,
                                     NautilusCopyCallback            done_callback,
                                     gpointer                        done_callback_data)
{
    GTask *task;
    CopyMoveJob *job;

    job = move_job_setup (files,
                          target_dir,
                          parent_window,
                          dbus_data,
                          done_callback,
                          done_callback_data);

    task = g_task_new (NULL, job->common.cancellable, move_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, nautilus_file_operations_move);
    g_object_unref (task);
}

static void
nautilus_file_operations_move (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
    CopyMoveJob *job;
    CommonJob *common;
    GList *fallbacks;
    g_auto (SourceInfo) source_info = SOURCE_INFO_INIT;
    TransferInfo transfer_info;
    g_autofree char *dest_fs_id = NULL;
    g_autofree char *dest_fs_type = NULL;
    GList *fallback_files;

    job = task_data;

    /* Since we never initiate the app (in the case of
     * testing), we can't inhibit its power manager.
     * This would terminate the testing. So we avoid
     * doing this by checking if the RUNNING_TESTS
     * environment variable is set to "TRUE".
     */
    if (g_strcmp0 (g_getenv ("RUNNING_TESTS"), "TRUE"))
    {
        inhibit_power_manager ((CommonJob *) job, _("Moving Files"));
    }

    if (!nautilus_file_undo_manager_is_operating ())
    {
        g_autoptr (GFile) src_dir = NULL;

        src_dir = g_file_get_parent ((job->files)->data);

        if (g_file_has_uri_scheme (g_list_first (job->files)->data, "trash"))
        {
            job->common.undo_info = nautilus_file_undo_info_ext_new (NAUTILUS_FILE_UNDO_OP_RESTORE_FROM_TRASH,
                                                                     g_list_length (job->files),
                                                                     src_dir, job->destination);
        }
        else
        {
            job->common.undo_info = nautilus_file_undo_info_ext_new (NAUTILUS_FILE_UNDO_OP_MOVE,
                                                                     g_list_length (job->files),
                                                                     src_dir, job->destination);
        }
    }

    common = &job->common;

    nautilus_progress_info_start (job->common.progress);

    fallbacks = NULL;

    verify_destination (&job->common,
                        job->destination,
                        &dest_fs_id,
                        -1);
    if (job_aborted (common))
    {
        goto aborted;
    }

    /* This moves all files that we can do without copy + delete */
    move_files_prepare (job, dest_fs_id, &dest_fs_type, &fallbacks);
    if (job_aborted (common))
    {
        goto aborted;
    }

    if (fallbacks == NULL)
    {
        gint total;

        total = g_list_length (job->files);

        memset (&source_info, 0, sizeof (source_info));
        source_info.num_files = total;
        memset (&transfer_info, 0, sizeof (transfer_info));
        transfer_info.num_files = total;
        report_copy_progress (job, &source_info, &transfer_info);

        return;
    }

    /* The rest we need to do deep copy + delete behind on,
     *  so scan for size */

    fallback_files = get_files_from_fallbacks (fallbacks);
    scan_sources (fallback_files,
                  &source_info,
                  common,
                  OP_KIND_MOVE);

    g_list_free (fallback_files);

    if (job_aborted (common))
    {
        goto aborted;
    }

    verify_destination (&job->common,
                        job->destination,
                        NULL,
                        source_info.num_bytes);
    if (job_aborted (common))
    {
        goto aborted;
    }

    memset (&transfer_info, 0, sizeof (transfer_info));
    move_files (job,
                fallbacks,
                dest_fs_id, &dest_fs_type,
                &source_info, &transfer_info);

aborted:
    g_list_free_full (fallbacks, g_free);
}

static void
report_preparing_link_progress (CopyMoveJob *link_job,
                                int          total,
                                int          left)
{
    CommonJob *job;
    g_autofree gchar *basename = NULL;

    job = (CommonJob *) link_job;
    basename = get_basename (link_job->destination);
    nautilus_progress_info_take_status (job->progress,
                                        g_strdup_printf (_("Creating links in “%s”"),
                                                         basename));

    nautilus_progress_info_take_details (job->progress,
                                         g_strdup_printf (ngettext ("Making link to %'d file",
                                                                    "Making links to %'d files",
                                                                    left),
                                                          left));

    nautilus_progress_info_set_progress (job->progress, left, total);
}

static char *
get_abs_path_for_symlink (GFile *file,
                          GFile *destination)
{
    GFile *root, *parent;
    char *relative, *abs;

    if (g_file_is_native (file) || g_file_is_native (destination))
    {
        return g_file_get_path (file);
    }

    root = g_object_ref (file);
    while ((parent = g_file_get_parent (root)) != NULL)
    {
        g_object_unref (root);
        root = parent;
    }

    relative = g_file_get_relative_path (root, file);
    g_object_unref (root);
    abs = g_strconcat ("/", relative, NULL);
    g_free (relative);
    return abs;
}


static void
link_file (CopyMoveJob  *job,
           GFile        *src,
           GFile        *dest_dir,
           char        **dest_fs_type,
           GHashTable   *debuting_files,
           int           files_left)
{
    GFile *src_dir;
    GFile *new_dest;
    g_autoptr (GFile) dest = NULL;
    g_autofree gchar *dest_uri = NULL;
    int count;
    char *path;
    gboolean not_local;
    GError *error;
    CommonJob *common;
    char *primary, *secondary, *details;
    int response;
    gboolean handled_invalid_filename;

    common = (CommonJob *) job;

    count = 0;

    src_dir = g_file_get_parent (src);
    if (g_file_equal (src_dir, dest_dir))
    {
        count = 1;
    }
    g_object_unref (src_dir);

    handled_invalid_filename = *dest_fs_type != NULL;

    dest = get_target_file_for_link (src, dest_dir, *dest_fs_type, count);

retry:
    error = NULL;
    not_local = FALSE;

    path = get_abs_path_for_symlink (src, dest);
    if (path == NULL)
    {
        not_local = TRUE;
    }
    else if (g_file_make_symbolic_link (dest,
                                        path,
                                        common->cancellable,
                                        &error))
    {
        if (common->undo_info != NULL)
        {
            nautilus_file_undo_info_ext_add_origin_target_pair (NAUTILUS_FILE_UNDO_INFO_EXT (common->undo_info),
                                                                src, dest);
        }

        g_free (path);
        if (debuting_files)
        {
            g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
        }

        nautilus_file_changes_queue_file_added (dest);
        dest_uri = g_file_get_uri (dest);

        return;
    }
    g_free (path);

    if (error != NULL &&
        IS_IO_ERROR (error, INVALID_FILENAME) &&
        !handled_invalid_filename)
    {
        handled_invalid_filename = TRUE;

        g_assert (*dest_fs_type == NULL);
        *dest_fs_type = query_fs_type (dest_dir, common->cancellable);

        new_dest = get_target_file_for_link (src, dest_dir, *dest_fs_type, count);

        if (!g_file_equal (dest, new_dest))
        {
            g_object_unref (dest);
            dest = new_dest;
            g_error_free (error);

            goto retry;
        }
        else
        {
            g_object_unref (new_dest);
        }
    }
    /* Conflict */
    if (error != NULL && IS_IO_ERROR (error, EXISTS))
    {
        g_object_unref (dest);
        dest = get_target_file_for_link (src, dest_dir, *dest_fs_type, count++);
        g_error_free (error);
        goto retry;
    }
    else if (error != NULL && IS_IO_ERROR (error, CANCELLED))
    {
        g_error_free (error);
    }
    /* Other error */
    else if (error != NULL)
    {
        g_autofree gchar *basename = NULL;

        if (common->skip_all_error)
        {
            return;
        }
        basename = get_basename (src);
        primary = g_strdup_printf (_("Error while creating link to %s."),
                                   basename);
        if (not_local)
        {
            secondary = g_strdup (_("Symbolic links only supported for local files"));
            details = NULL;
        }
        else if (IS_IO_ERROR (error, NOT_SUPPORTED))
        {
            secondary = g_strdup (_("The target doesn’t support symbolic links."));
            details = NULL;
        }
        else
        {
            g_autofree gchar *filename = NULL;

            filename = get_truncated_parse_name (dest_dir);
            secondary = g_strdup_printf (_("There was an error creating the symlink in %s."),
                                         filename);
            details = error->message;
        }

        response = run_warning (common,
                                primary,
                                secondary,
                                details,
                                files_left > 1,
                                CANCEL, SKIP_ALL, SKIP,
                                NULL);

        if (error)
        {
            g_error_free (error);
        }

        if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
        {
            abort_job (common);
        }
        else if (response == 1)             /* skip all */
        {
            common->skip_all_error = TRUE;
        }
        else if (response == 2)             /* skip */
        {               /* do nothing */
        }
        else
        {
            g_assert_not_reached ();
        }
    }
}

static void
link_task_done (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    CopyMoveJob *job;

    job = user_data;
    if (job->done_callback)
    {
        job->done_callback (job->debuting_files,
                            !job_aborted ((CommonJob *) job),
                            job->done_callback_data);
    }

    g_list_free_full (job->files, g_object_unref);
    g_object_unref (job->destination);
    g_hash_table_unref (job->debuting_files);

    finalize_common ((CommonJob *) job);

    nautilus_file_changes_consume_changes (TRUE);
}

static void
link_task_thread_func (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
    CopyMoveJob *job;
    CommonJob *common;
    GFile *src;
    g_autofree char *dest_fs_type = NULL;
    int total, left;
    int i;
    GList *l;

    job = task_data;
    common = &job->common;

    nautilus_progress_info_start (job->common.progress);

    verify_destination (&job->common,
                        job->destination,
                        NULL,
                        -1);
    if (job_aborted (common))
    {
        return;
    }

    total = left = g_list_length (job->files);

    report_preparing_link_progress (job, total, left);

    i = 0;
    for (l = job->files;
         l != NULL && !job_aborted (common);
         l = l->next)
    {
        src = l->data;

        link_file (job, src, job->destination,
                   &dest_fs_type, job->debuting_files,
                   left);
        report_preparing_link_progress (job, total, --left);
        i++;
    }
}

void
nautilus_file_operations_link (GList                          *files,
                               GFile                          *target_dir,
                               GtkWindow                      *parent_window,
                               NautilusFileOperationsDBusData *dbus_data,
                               NautilusCopyCallback            done_callback,
                               gpointer                        done_callback_data)
{
    g_autoptr (GTask) task = NULL;
    CopyMoveJob *job;

    job = op_job_new (CopyMoveJob, parent_window, dbus_data);
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
    job->destination = g_object_ref (target_dir);
    /* Need to indicate the destination for the operation notification open
     * button. */
    nautilus_progress_info_set_destination (((CommonJob *) job)->progress, target_dir);
    job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);

    if (!nautilus_file_undo_manager_is_operating ())
    {
        g_autoptr (GFile) src_dir = NULL;

        src_dir = g_file_get_parent (files->data);
        job->common.undo_info = nautilus_file_undo_info_ext_new (NAUTILUS_FILE_UNDO_OP_CREATE_LINK,
                                                                 g_list_length (files),
                                                                 src_dir, target_dir);
    }

    task = g_task_new (NULL, job->common.cancellable, link_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, link_task_thread_func);
}


void
nautilus_file_operations_duplicate (GList                          *files,
                                    GtkWindow                      *parent_window,
                                    NautilusFileOperationsDBusData *dbus_data,
                                    NautilusCopyCallback            done_callback,
                                    gpointer                        done_callback_data)
{
    g_autoptr (GTask) task = NULL;
    CopyMoveJob *job;
    g_autoptr (GFile) parent = NULL;

    job = op_job_new (CopyMoveJob, parent_window, dbus_data);
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
    job->destination = NULL;
    /* Duplicate files doesn't have a destination, since is the same as source.
     * For that set as destination the source parent folder */
    parent = g_file_get_parent (files->data);
    /* Need to indicate the destination for the operation notification open
     * button. */
    nautilus_progress_info_set_destination (((CommonJob *) job)->progress, parent);
    job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);

    if (!nautilus_file_undo_manager_is_operating ())
    {
        g_autoptr (GFile) src_dir = NULL;

        src_dir = g_file_get_parent (files->data);
        job->common.undo_info =
            nautilus_file_undo_info_ext_new (NAUTILUS_FILE_UNDO_OP_DUPLICATE,
                                             g_list_length (files),
                                             src_dir, src_dir);
    }

    task = g_task_new (NULL, job->common.cancellable, copy_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, nautilus_file_operations_copy);
}

static void
set_permissions_task_done (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
    SetPermissionsJob *job;

    job = user_data;

    g_object_unref (job->file);

    if (job->done_callback)
    {
        job->done_callback (!job_aborted ((CommonJob *) job),
                            job->done_callback_data);
    }

    finalize_common ((CommonJob *) job);
}

static void
set_permissions_file (SetPermissionsJob *job,
                      GFile             *file,
                      GFileInfo         *info);

static void
set_permissions_contained_files (SetPermissionsJob *job,
                                 GFile             *file)
{
    CommonJob *common;
    GFileEnumerator *enumerator;

    common = (CommonJob *) job;

    enumerator = g_file_enumerate_children (file,
                                            G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                            G_FILE_ATTRIBUTE_UNIX_MODE,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            common->cancellable,
                                            NULL);
    if (enumerator)
    {
        GFileInfo *child_info;

        while (!job_aborted (common) &&
               (child_info = g_file_enumerator_next_file (enumerator, common->cancellable, NULL)) != NULL)
        {
            GFile *child;

            child = g_file_get_child (file,
                                      g_file_info_get_name (child_info));
            set_permissions_file (job, child, child_info);
            g_object_unref (child);
            g_object_unref (child_info);
        }
        g_file_enumerator_close (enumerator, common->cancellable, NULL);
        g_object_unref (enumerator);
    }
}

static void
set_permissions_file (SetPermissionsJob *job,
                      GFile             *file,
                      GFileInfo         *info)
{
    CommonJob *common;
    gboolean free_info;
    guint32 current;
    guint32 value;
    guint32 mask;

    common = (CommonJob *) job;

    nautilus_progress_info_pulse_progress (common->progress);

    free_info = FALSE;
    if (info == NULL)
    {
        free_info = TRUE;
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                  G_FILE_ATTRIBUTE_UNIX_MODE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  common->cancellable,
                                  NULL);
        /* Ignore errors */
        if (info == NULL)
        {
            return;
        }
    }

    if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    {
        value = job->dir_permissions;
        mask = job->dir_mask;
    }
    else
    {
        value = job->file_permissions;
        mask = job->file_mask;
    }


    if (!job_aborted (common) &&
        g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE))
    {
        current = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

        if (common->undo_info != NULL)
        {
            nautilus_file_undo_info_rec_permissions_add_file (NAUTILUS_FILE_UNDO_INFO_REC_PERMISSIONS (common->undo_info),
                                                              file, current);
        }

        current = (current & ~mask) | value;

        g_file_set_attribute_uint32 (file, G_FILE_ATTRIBUTE_UNIX_MODE,
                                     current, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     common->cancellable, NULL);
    }

    if (!job_aborted (common) &&
        g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    {
        set_permissions_contained_files (job, file);
    }
    if (free_info)
    {
        g_object_unref (info);
    }
}

static void
set_permissions_thread_func (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
    SetPermissionsJob *job = task_data;
    CommonJob *common;

    common = (CommonJob *) job;

    nautilus_progress_info_set_status (common->progress,
                                       _("Setting permissions"));

    nautilus_progress_info_start (job->common.progress);
    set_permissions_contained_files (job, job->file);
}

void
nautilus_file_set_permissions_recursive (const char         *directory,
                                         guint32             file_permissions,
                                         guint32             file_mask,
                                         guint32             dir_permissions,
                                         guint32             dir_mask,
                                         NautilusOpCallback  callback,
                                         gpointer            callback_data)
{
    g_autoptr (GTask) task = NULL;
    SetPermissionsJob *job;

    job = op_job_new (SetPermissionsJob, NULL, NULL);
    job->file = g_file_new_for_uri (directory);
    job->file_permissions = file_permissions;
    job->file_mask = file_mask;
    job->dir_permissions = dir_permissions;
    job->dir_mask = dir_mask;
    job->done_callback = callback;
    job->done_callback_data = callback_data;

    if (!nautilus_file_undo_manager_is_operating ())
    {
        job->common.undo_info =
            nautilus_file_undo_info_rec_permissions_new (job->file,
                                                         file_permissions, file_mask,
                                                         dir_permissions, dir_mask);
    }

    task = g_task_new (NULL, NULL, set_permissions_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, set_permissions_thread_func);
}

static GList *
location_list_from_uri_list (const GList *uris)
{
    const GList *l;
    GList *files;
    GFile *f;

    files = NULL;
    for (l = uris; l != NULL; l = l->next)
    {
        f = g_file_new_for_uri (l->data);
        files = g_list_prepend (files, f);
    }

    return g_list_reverse (files);
}

typedef struct
{
    NautilusCopyCallback real_callback;
    gpointer real_data;
} MoveTrashCBData;

static void
callback_for_move_to_trash (GHashTable      *debuting_uris,
                            gboolean         user_cancelled,
                            MoveTrashCBData *data)
{
    if (data->real_callback)
    {
        data->real_callback (debuting_uris, !user_cancelled, data->real_data);
    }
    g_slice_free (MoveTrashCBData, data);
}

void
nautilus_file_operations_copy_move (const GList                    *item_uris,
                                    const char                     *target_dir,
                                    GdkDragAction                   copy_action,
                                    GtkWidget                      *parent_view,
                                    NautilusFileOperationsDBusData *dbus_data,
                                    NautilusCopyCallback            done_callback,
                                    gpointer                        done_callback_data)
{
    GList *locations;
    GList *p;
    GFile *dest, *src_dir;
    GtkWindow *parent_window;
    gboolean target_is_mapping;
    gboolean have_nonmapping_source;

    dest = NULL;
    target_is_mapping = FALSE;
    have_nonmapping_source = FALSE;

    if (target_dir)
    {
        dest = g_file_new_for_uri (target_dir);
        if (g_file_has_uri_scheme (dest, "burn"))
        {
            target_is_mapping = TRUE;
        }
    }

    locations = location_list_from_uri_list (item_uris);

    for (p = locations; p != NULL; p = p->next)
    {
        if (!g_file_has_uri_scheme ((GFile * ) p->data, "burn"))
        {
            have_nonmapping_source = TRUE;
        }
    }

    if (target_is_mapping && have_nonmapping_source && copy_action == GDK_ACTION_MOVE)
    {
        /* never move to "burn:///", but fall back to copy.
         * This is a workaround, because otherwise the source files would be removed.
         */
        copy_action = GDK_ACTION_COPY;
    }

    parent_window = NULL;
    if (parent_view)
    {
        parent_window = (GtkWindow *) gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
    }

    if (copy_action == GDK_ACTION_COPY)
    {
        src_dir = g_file_get_parent (locations->data);
        if (target_dir == NULL ||
            (src_dir != NULL &&
             g_file_equal (src_dir, dest)))
        {
            nautilus_file_operations_duplicate (locations,
                                                parent_window,
                                                dbus_data,
                                                done_callback, done_callback_data);
        }
        else
        {
            nautilus_file_operations_copy_async (locations,
                                                 dest,
                                                 parent_window,
                                                 dbus_data,
                                                 done_callback, done_callback_data);
        }
        if (src_dir)
        {
            g_object_unref (src_dir);
        }
    }
    else if (copy_action == GDK_ACTION_MOVE)
    {
        if (g_file_has_uri_scheme (dest, "trash"))
        {
            MoveTrashCBData *cb_data;

            cb_data = g_slice_new0 (MoveTrashCBData);
            cb_data->real_callback = done_callback;
            cb_data->real_data = done_callback_data;

            nautilus_file_operations_trash_or_delete_async (locations,
                                                            parent_window,
                                                            dbus_data,
                                                            (NautilusDeleteCallback) callback_for_move_to_trash,
                                                            cb_data);
        }
        else
        {
            nautilus_file_operations_move_async (locations,
                                                 dest,
                                                 parent_window,
                                                 dbus_data,
                                                 done_callback, done_callback_data);
        }
    }
    else
    {
        nautilus_file_operations_link (locations,
                                       dest,
                                       parent_window,
                                       dbus_data,
                                       done_callback, done_callback_data);
    }

    g_list_free_full (locations, g_object_unref);
    if (dest)
    {
        g_object_unref (dest);
    }
}

static void
create_task_done (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
    CreateJob *job;

    job = user_data;
    if (job->done_callback)
    {
        job->done_callback (job->created_file,
                            !job_aborted ((CommonJob *) job),
                            job->done_callback_data);
    }

    g_object_unref (job->dest_dir);
    if (job->src)
    {
        g_object_unref (job->src);
    }
    g_free (job->src_data);
    g_free (job->filename);
    if (job->created_file)
    {
        g_object_unref (job->created_file);
    }

    finalize_common ((CommonJob *) job);

    nautilus_file_changes_consume_changes (TRUE);
}

static void
create_task_thread_func (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
    CreateJob *job;
    CommonJob *common;
    int count;
    g_autoptr (GFile) dest = NULL;
    g_autofree gchar *dest_uri = NULL;
    g_autofree char *filename = NULL;
    char *filename_base;
    g_autofree char *dest_fs_type = NULL;
    GError *error;
    gboolean res;
    gboolean filename_is_utf8;
    char *primary, *secondary, *details;
    int response;
    char *data;
    int length;
    GFileOutputStream *out;
    gboolean handled_invalid_filename;
    int max_length, offset;

    job = task_data;
    common = &job->common;

    nautilus_progress_info_start (job->common.progress);

    handled_invalid_filename = FALSE;

    max_length = nautilus_get_max_child_name_length_for_location (job->dest_dir);

    verify_destination (common,
                        job->dest_dir,
                        NULL, -1);
    if (job_aborted (common))
    {
        return;
    }

    filename = g_strdup (job->filename);
    filename_is_utf8 = FALSE;
    if (filename)
    {
        filename_is_utf8 = g_utf8_validate (filename, -1, NULL);
    }
    if (filename == NULL)
    {
        if (job->make_dir)
        {
            /* localizers: the initial name of a new folder  */
            filename = g_strdup (_("Untitled Folder"));
            filename_is_utf8 = TRUE;             /* Pass in utf8 */
        }
        else
        {
            if (job->src != NULL)
            {
                g_autofree char *basename = NULL;

                basename = g_file_get_basename (job->src);
                filename = g_strdup_printf ("%s", basename);
            }
            if (filename == NULL)
            {
                /* localizers: the initial name of a new empty document */
                filename = g_strdup (_("Untitled Document"));
                filename_is_utf8 = TRUE;                 /* Pass in utf8 */
            }
        }
    }

    make_file_name_valid_for_dest_fs (filename, dest_fs_type);
    if (filename_is_utf8)
    {
        dest = g_file_get_child_for_display_name (job->dest_dir, filename, NULL);
    }
    if (dest == NULL)
    {
        dest = g_file_get_child (job->dest_dir, filename);
    }
    count = 1;

retry:

    error = NULL;
    if (job->make_dir)
    {
        res = g_file_make_directory (dest,
                                     common->cancellable,
                                     &error);

        if (res)
        {
            GFile *real;

            real = map_possibly_volatile_file_to_real (dest, common->cancellable, &error);
            if (real == NULL)
            {
                res = FALSE;
            }
            else
            {
                g_object_unref (dest);
                dest = real;
            }
        }

        if (res && common->undo_info != NULL)
        {
            nautilus_file_undo_info_create_set_data (NAUTILUS_FILE_UNDO_INFO_CREATE (common->undo_info),
                                                     dest, NULL, 0);
        }
    }
    else
    {
        if (job->src)
        {
            res = g_file_copy (job->src,
                               dest,
                               G_FILE_COPY_TARGET_DEFAULT_PERMS,
                               common->cancellable,
                               NULL, NULL,
                               &error);

            if (res)
            {
                GFile *real;

                real = map_possibly_volatile_file_to_real (dest, common->cancellable, &error);
                if (real == NULL)
                {
                    res = FALSE;
                }
                else
                {
                    g_object_unref (dest);
                    dest = real;
                }
            }

            if (res && common->undo_info != NULL)
            {
                g_autofree gchar *uri = NULL;

                uri = g_file_get_uri (job->src);
                nautilus_file_undo_info_create_set_data (NAUTILUS_FILE_UNDO_INFO_CREATE (common->undo_info),
                                                         dest, uri, 0);
            }
        }
        else
        {
            data = "";
            length = 0;
            if (job->src_data)
            {
                data = job->src_data;
                length = job->length;
            }

            out = g_file_create (dest,
                                 G_FILE_CREATE_NONE,
                                 common->cancellable,
                                 &error);
            if (out)
            {
                GFile *real;

                real = map_possibly_volatile_file_to_real_on_write (dest,
                                                                    out,
                                                                    common->cancellable,
                                                                    &error);
                if (real == NULL)
                {
                    res = FALSE;
                    g_object_unref (out);
                }
                else
                {
                    g_object_unref (dest);
                    dest = real;

                    res = g_output_stream_write_all (G_OUTPUT_STREAM (out),
                                                     data, length,
                                                     NULL,
                                                     common->cancellable,
                                                     &error);
                    if (res)
                    {
                        res = g_output_stream_close (G_OUTPUT_STREAM (out),
                                                     common->cancellable,
                                                     &error);

                        if (res && common->undo_info != NULL)
                        {
                            nautilus_file_undo_info_create_set_data (NAUTILUS_FILE_UNDO_INFO_CREATE (common->undo_info),
                                                                     dest, data, length);
                        }
                    }

                    /* This will close if the write failed and we didn't close */
                    g_object_unref (out);
                }
            }
            else
            {
                res = FALSE;
            }
        }
    }

    if (res)
    {
        job->created_file = g_object_ref (dest);
        nautilus_file_changes_queue_file_added (dest);
        dest_uri = g_file_get_uri (dest);
        gtk_recent_manager_add_item (gtk_recent_manager_get_default (), dest_uri);
    }
    else
    {
        g_assert (error != NULL);

        if (IS_IO_ERROR (error, INVALID_FILENAME) &&
            !handled_invalid_filename)
        {
            g_autofree gchar *new_filename = NULL;

            handled_invalid_filename = TRUE;

            g_assert (dest_fs_type == NULL);
            dest_fs_type = query_fs_type (job->dest_dir, common->cancellable);

            if (count == 1)
            {
                new_filename = g_strdup (filename);
            }
            else
            {
                g_autofree char *filename2 = NULL;
                g_autofree char *suffix = NULL;

                filename_base = filename;
                if (job->src != NULL)
                {
                    g_autoptr (NautilusFile) file = NULL;
                    file = nautilus_file_get (job->src);
                    if (!nautilus_file_is_directory (file))
                    {
                        filename_base = eel_filename_strip_extension (filename);
                    }
                }

                offset = strlen (filename_base);
                suffix = g_strdup (filename + offset);

                filename2 = g_strdup_printf ("%s %d%s", filename_base, count, suffix);

                new_filename = NULL;
                if (max_length > 0 && strlen (filename2) > max_length)
                {
                    new_filename = shorten_utf8_string (filename2, strlen (filename2) - max_length);
                }

                if (new_filename == NULL)
                {
                    new_filename = g_strdup (filename2);
                }
            }

            if (make_file_name_valid_for_dest_fs (new_filename, dest_fs_type))
            {
                g_object_unref (dest);

                if (filename_is_utf8)
                {
                    dest = g_file_get_child_for_display_name (job->dest_dir, new_filename, NULL);
                }
                if (dest == NULL)
                {
                    dest = g_file_get_child (job->dest_dir, new_filename);
                }

                g_error_free (error);
                goto retry;
            }
        }

        if (IS_IO_ERROR (error, EXISTS))
        {
            g_autofree char *suffix = NULL;
            g_autofree gchar *filename2 = NULL;

            g_clear_object (&dest);

            filename_base = filename;
            if (job->src != NULL)
            {
                g_autoptr (NautilusFile) file = NULL;

                file = nautilus_file_get (job->src);
                if (!nautilus_file_is_directory (file))
                {
                    filename_base = eel_filename_strip_extension (filename);
                }
            }


            offset = strlen (filename_base);
            suffix = g_strdup (filename + offset);

            filename2 = g_strdup_printf ("%s %d%s", filename_base, ++count, suffix);

            if (max_length > 0 && strlen (filename2) > max_length)
            {
                g_autofree char *new_filename = NULL;

                new_filename = shorten_utf8_string (filename2, strlen (filename2) - max_length);
                if (new_filename != NULL)
                {
                    g_free (filename2);
                    filename2 = new_filename;
                }
            }

            make_file_name_valid_for_dest_fs (filename2, dest_fs_type);
            if (filename_is_utf8)
            {
                dest = g_file_get_child_for_display_name (job->dest_dir, filename2, NULL);
            }
            if (dest == NULL)
            {
                dest = g_file_get_child (job->dest_dir, filename2);
            }
            g_error_free (error);
            goto retry;
        }
        else if (IS_IO_ERROR (error, CANCELLED))
        {
            g_error_free (error);
        }
        /* Other error */
        else
        {
            g_autofree gchar *basename = NULL;
            g_autofree gchar *parse_name = NULL;

            basename = get_basename (dest);
            if (job->make_dir)
            {
                primary = g_strdup_printf (_("Error while creating directory %s."),
                                           basename);
            }
            else
            {
                primary = g_strdup_printf (_("Error while creating file %s."),
                                           basename);
            }
            parse_name = get_truncated_parse_name (job->dest_dir);
            secondary = g_strdup_printf (_("There was an error creating the directory in %s."),
                                         parse_name);

            details = error->message;

            response = run_warning (common,
                                    primary,
                                    secondary,
                                    details,
                                    FALSE,
                                    CANCEL, SKIP,
                                    NULL);

            g_error_free (error);

            if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
            {
                abort_job (common);
            }
            else if (response == 1)                 /* skip */
            {                   /* do nothing */
            }
            else
            {
                g_assert_not_reached ();
            }
        }
    }
}

void
nautilus_file_operations_new_folder (GtkWidget                      *parent_view,
                                     NautilusFileOperationsDBusData *dbus_data,
                                     const char                     *parent_dir,
                                     const char                     *folder_name,
                                     NautilusCreateCallback          done_callback,
                                     gpointer                        done_callback_data)
{
    g_autoptr (GTask) task = NULL;
    CreateJob *job;
    GtkWindow *parent_window;

    parent_window = NULL;
    if (parent_view)
    {
        parent_window = (GtkWindow *) gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
    }

    job = op_job_new (CreateJob, parent_window, dbus_data);
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->dest_dir = g_file_new_for_uri (parent_dir);
    job->filename = g_strdup (folder_name);
    job->make_dir = TRUE;

    if (!nautilus_file_undo_manager_is_operating ())
    {
        job->common.undo_info = nautilus_file_undo_info_create_new (NAUTILUS_FILE_UNDO_OP_CREATE_FOLDER);
    }

    task = g_task_new (NULL, job->common.cancellable, create_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, create_task_thread_func);
}

void
nautilus_file_operations_new_file_from_template (GtkWidget              *parent_view,
                                                 const char             *parent_dir,
                                                 const char             *target_filename,
                                                 const char             *template_uri,
                                                 NautilusCreateCallback  done_callback,
                                                 gpointer                done_callback_data)
{
    g_autoptr (GTask) task = NULL;
    CreateJob *job;
    GtkWindow *parent_window;

    parent_window = NULL;
    if (parent_view)
    {
        parent_window = (GtkWindow *) gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
    }

    job = op_job_new (CreateJob, parent_window, NULL);
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->dest_dir = g_file_new_for_uri (parent_dir);
    job->filename = g_strdup (target_filename);

    if (template_uri)
    {
        job->src = g_file_new_for_uri (template_uri);
    }

    if (!nautilus_file_undo_manager_is_operating ())
    {
        job->common.undo_info = nautilus_file_undo_info_create_new (NAUTILUS_FILE_UNDO_OP_CREATE_FILE_FROM_TEMPLATE);
    }

    task = g_task_new (NULL, job->common.cancellable, create_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, create_task_thread_func);
}

void
nautilus_file_operations_new_file (GtkWidget              *parent_view,
                                   const char             *parent_dir,
                                   const char             *target_filename,
                                   const char             *initial_contents,
                                   int                     length,
                                   NautilusCreateCallback  done_callback,
                                   gpointer                done_callback_data)
{
    g_autoptr (GTask) task = NULL;
    CreateJob *job;
    GtkWindow *parent_window;

    parent_window = NULL;
    if (parent_view)
    {
        parent_window = (GtkWindow *) gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
    }

    job = op_job_new (CreateJob, parent_window, NULL);
    job->done_callback = done_callback;
    job->done_callback_data = done_callback_data;
    job->dest_dir = g_file_new_for_uri (parent_dir);
    job->src_data = g_memdup (initial_contents, length);
    job->length = length;
    job->filename = g_strdup (target_filename);

    if (!nautilus_file_undo_manager_is_operating ())
    {
        job->common.undo_info = nautilus_file_undo_info_create_new (NAUTILUS_FILE_UNDO_OP_CREATE_EMPTY_FILE);
    }

    task = g_task_new (NULL, job->common.cancellable, create_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, create_task_thread_func);
}



static void
delete_trash_file (CommonJob *job,
                   GFile     *file,
                   gboolean   del_file,
                   gboolean   del_children)
{
    GFileInfo *info;
    GFile *child;
    GFileEnumerator *enumerator;

    if (job_aborted (job))
    {
        return;
    }

    if (del_children)
    {
        gboolean should_recurse;

        /* The g_file_delete operation works differently for locations provided
         * by the trash backend as it prevents modifications of trashed items
         * For that reason, it is enough to call g_file_delete on top-level
         * items only.
         */
        should_recurse = !g_file_has_uri_scheme (file, "trash");

        enumerator = g_file_enumerate_children (file,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                job->cancellable,
                                                NULL);
        if (enumerator)
        {
            while (!job_aborted (job) &&
                   (info = g_file_enumerator_next_file (enumerator, job->cancellable, NULL)) != NULL)
            {
                gboolean is_dir;

                child = g_file_get_child (file,
                                          g_file_info_get_name (info));
                is_dir = (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY);

                delete_trash_file (job, child, TRUE, should_recurse && is_dir);
                g_object_unref (child);
                g_object_unref (info);
            }
            g_file_enumerator_close (enumerator, job->cancellable, NULL);
            g_object_unref (enumerator);
        }
    }

    if (!job_aborted (job) && del_file)
    {
        g_file_delete (file, job->cancellable, NULL);
    }
}

static void
empty_trash_task_done (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    EmptyTrashJob *job;

    job = user_data;

    g_list_free_full (job->trash_dirs, g_object_unref);

    if (job->done_callback)
    {
        job->done_callback (!job_aborted ((CommonJob *) job),
                            job->done_callback_data);
    }

    finalize_common ((CommonJob *) job);
}

static void
empty_trash_thread_func (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
    EmptyTrashJob *job = task_data;
    CommonJob *common;
    GList *l;
    gboolean confirmed;

    common = (CommonJob *) job;

    nautilus_progress_info_start (job->common.progress);

    if (job->should_confirm)
    {
        confirmed = confirm_empty_trash (common);
    }
    else
    {
        confirmed = TRUE;
    }
    if (confirmed)
    {
        for (l = job->trash_dirs;
             l != NULL && !job_aborted (common);
             l = l->next)
        {
            delete_trash_file (common, l->data, FALSE, TRUE);
        }
    }
}

void
nautilus_file_operations_empty_trash (GtkWidget                      *parent_view,
                                      gboolean                        ask_confirmation,
                                      NautilusFileOperationsDBusData *dbus_data)
{
    g_autoptr (GTask) task = NULL;
    EmptyTrashJob *job;
    GtkWindow *parent_window;

    parent_window = NULL;
    if (parent_view)
    {
        parent_window = (GtkWindow *) gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
    }

    job = op_job_new (EmptyTrashJob, parent_window, dbus_data);
    job->trash_dirs = g_list_prepend (job->trash_dirs,
                                      g_file_new_for_uri ("trash:"));
    job->should_confirm = ask_confirmation;

    inhibit_power_manager ((CommonJob *) job, _("Emptying Trash"));

    task = g_task_new (NULL, NULL, empty_trash_task_done, job);
    g_task_set_task_data (task, job, NULL);
    g_task_run_in_thread (task, empty_trash_thread_func);
}

static void
extract_task_done (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
    ExtractJob *extract_job;

    extract_job = user_data;

    if (extract_job->done_callback)
    {
        extract_job->done_callback (extract_job->output_files,
                                    extract_job->done_callback_data);
    }

    g_list_free_full (extract_job->source_files, g_object_unref);
    g_list_free_full (extract_job->output_files, g_object_unref);
    g_object_unref (extract_job->destination_directory);

    finalize_common ((CommonJob *) extract_job);

    nautilus_file_changes_consume_changes (TRUE);
}

static GFile *
extract_job_on_decide_destination (AutoarExtractor *extractor,
                                   GFile           *destination,
                                   GList           *files,
                                   gpointer         user_data)
{
    ExtractJob *extract_job = user_data;
    GFile *decided_destination;
    g_autofree char *basename = NULL;

    nautilus_progress_info_set_details (extract_job->common.progress,
                                        _("Verifying destination"));

    basename = g_file_get_basename (destination);
    decided_destination = nautilus_generate_unique_file_in_directory (extract_job->destination_directory,
                                                                      basename);

    if (job_aborted ((CommonJob *) extract_job))
    {
        g_object_unref (decided_destination);
        return NULL;
    }

    /* The extract_job->destination_decided variable signalizes whether the
     * extract_job->output_files list already contains the final location as
     * its first link. There is no way to get this over the AutoarExtractor
     * API currently.
     */
    extract_job->output_files = g_list_prepend (extract_job->output_files,
                                                decided_destination);
    extract_job->destination_decided = TRUE;

    return g_object_ref (decided_destination);
}

static void
extract_job_on_progress (AutoarExtractor *extractor,
                         guint64          archive_current_decompressed_size,
                         guint            archive_current_decompressed_files,
                         gpointer         user_data)
{
    ExtractJob *extract_job = user_data;
    CommonJob *common = user_data;
    GFile *source_file;
    char *details;
    double elapsed;
    double transfer_rate;
    int remaining_time;
    guint64 archive_total_decompressed_size;
    gdouble archive_weight;
    gdouble archive_decompress_progress;
    guint64 job_completed_size;
    gdouble job_progress;
    g_autofree gchar *basename = NULL;
    g_autofree gchar *formatted_size_job_completed_size = NULL;
    g_autofree gchar *formatted_size_total_compressed_size = NULL;

    source_file = autoar_extractor_get_source_file (extractor);

    basename = get_basename (source_file);
    nautilus_progress_info_take_status (common->progress,
                                        g_strdup_printf (_("Extracting “%s”"),
                                                         basename));

    archive_total_decompressed_size = autoar_extractor_get_total_size (extractor);

    archive_decompress_progress = (gdouble) archive_current_decompressed_size /
                                  (gdouble) archive_total_decompressed_size;

    archive_weight = 0;
    if (extract_job->total_compressed_size)
    {
        archive_weight = (gdouble) extract_job->archive_compressed_size /
                         (gdouble) extract_job->total_compressed_size;
    }

    job_progress = archive_decompress_progress * archive_weight + extract_job->base_progress;

    elapsed = g_timer_elapsed (common->time, NULL);

    transfer_rate = 0;
    remaining_time = -1;

    job_completed_size = job_progress * extract_job->total_compressed_size;

    if (elapsed > 0)
    {
        transfer_rate = job_completed_size / elapsed;
    }
    if (transfer_rate > 0)
    {
        remaining_time = (extract_job->total_compressed_size - job_completed_size) /
                         transfer_rate;
    }

    formatted_size_job_completed_size = g_format_size (job_completed_size);
    formatted_size_total_compressed_size = g_format_size (extract_job->total_compressed_size);
    if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE ||
        transfer_rate == 0)
    {
        /* To translators: %s will expand to a size like "2 bytes" or
         * "3 MB", so something like "4 kb / 4 MB"
         */
        details = g_strdup_printf (_("%s / %s"), formatted_size_job_completed_size,
                                   formatted_size_total_compressed_size);
    }
    else
    {
        g_autofree gchar *formatted_time = NULL;
        g_autofree gchar *formatted_size_transfer_rate = NULL;

        formatted_time = get_formatted_time (remaining_time);
        formatted_size_transfer_rate = g_format_size ((goffset) transfer_rate);
        /* To translators: %s will expand to a size like "2 bytes" or
         * "3 MB", %s to a time duration like "2 minutes". So the whole
         * thing will be something like
         * "2 kb / 4 MB -- 2 hours left (4kb/sec)"
         *
         * The singular/plural form will be used depending on the
         * remaining time (i.e. the %s argument).
         */
        details = g_strdup_printf (ngettext ("%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                             "%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                             seconds_count_format_time_units (remaining_time)),
                                   formatted_size_job_completed_size,
                                   formatted_size_total_compressed_size,
                                   formatted_time,
                                   formatted_size_transfer_rate);
    }

    nautilus_progress_info_take_details (common->progress, details);

    if (elapsed > SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE)
    {
        nautilus_progress_info_set_remaining_time (common->progress,
                                                   remaining_time);
        nautilus_progress_info_set_elapsed_time (common->progress,
                                                 elapsed);
    }

    nautilus_progress_info_set_progress (common->progress, job_progress, 1);
}

static void
extract_job_on_error (AutoarExtractor *extractor,
                      GError          *error,
                      gpointer         user_data)
{
    ExtractJob *extract_job = user_data;
    GFile *source_file;
    gint response_id;
    g_autofree gchar *basename = NULL;

    source_file = autoar_extractor_get_source_file (extractor);

    if (IS_IO_ERROR (error, NOT_SUPPORTED))
    {
        handle_unsupported_compressed_file (extract_job->common.parent_window,
                                            source_file);

        return;
    }

    /* It is safe to use extract_job->output_files->data only when the
     * extract_job->destination_decided variable was set, see comment in the
     * extract_job_on_decide_destination function.
     */
    if (extract_job->destination_decided)
    {
        delete_file_recursively (extract_job->output_files->data, NULL, NULL, NULL);
    }

    basename = get_basename (source_file);
    nautilus_progress_info_take_status (extract_job->common.progress,
                                        g_strdup_printf (_("Error extracting “%s”"),
                                                         basename));

    response_id = run_warning ((CommonJob *) extract_job,
                               g_strdup_printf (_("There was an error while extracting “%s”."),
                                                basename),
                               g_strdup (error->message),
                               NULL,
                               FALSE,
                               CANCEL,
                               SKIP,
                               NULL);

    if (response_id == 0 || response_id == GTK_RESPONSE_DELETE_EVENT)
    {
        abort_job ((CommonJob *) extract_job);
    }
}

static void
extract_job_on_completed (AutoarExtractor *extractor,
                          gpointer         user_data)
{
    ExtractJob *extract_job = user_data;
    GFile *output_file;

    output_file = G_FILE (extract_job->output_files->data);

    nautilus_file_changes_queue_file_added (output_file);
}

typedef struct
{
    ExtractJob *extract_job;
    AutoarExtractor *extractor;
    gchar *passphrase;
    GtkWidget *passphrase_entry;
    GMutex mutex;
    GCond cond;
    gboolean completed;
} PassphraseRequestData;

static void
on_request_passphrase_cb (GtkDialog *dialog,
                          gint       response_id,
                          gpointer   user_data)
{
    PassphraseRequestData *data = user_data;

    if (response_id == GTK_RESPONSE_CANCEL ||
        response_id == GTK_RESPONSE_DELETE_EVENT)
    {
        abort_job ((CommonJob *) data->extract_job);
    }
    else
    {
        data->passphrase = g_strdup (gtk_entry_get_text (GTK_ENTRY (data->passphrase_entry)));
    }

    data->completed = TRUE;

    gtk_widget_destroy (GTK_WIDGET (dialog));

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);
}

static gboolean
run_passphrase_dialog (gpointer user_data)
{
    PassphraseRequestData *data = user_data;
    g_autofree gchar *label_str = NULL;
    g_autofree gchar *basename = NULL;
    GtkWidget *dialog;
    GtkWidget *entry;
    GtkWidget *label;
    GtkWidget *box;
    GFile *source_file;

    g_mutex_lock (&data->mutex);

    dialog = gtk_dialog_new_with_buttons (_("Password Required"),
                                          data->extract_job->common.parent_window,
                                          GTK_DIALOG_USE_HEADER_BAR | GTK_DIALOG_MODAL,
                                          _("Cancel"), GTK_RESPONSE_CANCEL,
                                          _("Extract"), GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    source_file = autoar_extractor_get_source_file (data->extractor);
    basename = get_basename (source_file);

    box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_widget_set_margin_start (box, 20);
    gtk_widget_set_margin_end (box, 20);
    gtk_widget_set_margin_top (box, 20);
    gtk_widget_set_margin_bottom (box, 20);

    label_str = g_strdup_printf (_("“%s” is password-protected."), basename);
    label = gtk_label_new (label_str);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
    gtk_container_add (GTK_CONTAINER (box), label);

    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_widget_set_valign (entry, GTK_ALIGN_END);
    gtk_widget_set_vexpand (entry, TRUE);
    gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Enter password…"));
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_entry_set_input_purpose (GTK_ENTRY (entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_container_add (GTK_CONTAINER (box), entry);

    data->passphrase_entry = entry;
    g_signal_connect (dialog, "response", G_CALLBACK (on_request_passphrase_cb), data);
    gtk_widget_show_all (dialog);

    return G_SOURCE_REMOVE;
}

static gchar *
extract_job_on_request_passphrase (AutoarExtractor *extractor,
                                   gpointer         user_data)
{
    PassphraseRequestData *data;
    ExtractJob *extract_job = user_data;
    gchar *passphrase;

    data = g_new0 (PassphraseRequestData, 1);
    g_mutex_init (&data->mutex);
    g_cond_init (&data->cond);
    data->extract_job = extract_job;
    data->extractor = extractor;

    g_mutex_lock (&data->mutex);

    g_main_context_invoke (NULL,
                           run_passphrase_dialog,
                           data);

    while (!data->completed)
    {
        g_cond_wait (&data->cond, &data->mutex);
    }

    g_mutex_unlock (&data->mutex);
    g_mutex_clear (&data->mutex);
    g_cond_clear (&data->cond);

    passphrase = g_steal_pointer (&data->passphrase);
    g_free (data);

    return passphrase;
}

static void
extract_job_on_scanned (AutoarExtractor *extractor,
                        guint            total_files,
                        gpointer         user_data)
{
    guint64 total_size;
    ExtractJob *extract_job;
    GFile *source_file;
    g_autofree gchar *basename = NULL;
    GFileInfo *fsinfo;
    guint64 free_size;

    extract_job = user_data;
    total_size = autoar_extractor_get_total_size (extractor);
    source_file = autoar_extractor_get_source_file (extractor);
    basename = get_basename (source_file);

    fsinfo = g_file_query_filesystem_info (source_file,
                                           G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
                                           G_FILE_ATTRIBUTE_FILESYSTEM_READONLY,
                                           extract_job->common.cancellable,
                                           NULL);
    free_size = g_file_info_get_attribute_uint64 (fsinfo,
                                                  G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

    /* FIXME: G_MAXUINT64 is the value used by autoar when the file size cannot
     * be determined. Ideally an API should be used instead.
     */
    if (total_size != G_MAXUINT64 && total_size > free_size)
    {
        nautilus_progress_info_take_status (extract_job->common.progress,
                                            g_strdup_printf (_("Error extracting “%s”"),
                                                             basename));
        run_error (&extract_job->common,
                   g_strdup_printf (_("Not enough free space to extract %s"), basename),
                   NULL,
                   NULL,
                   FALSE,
                   CANCEL,
                   NULL);

        abort_job ((CommonJob *) extract_job);
    }
}

static void
report_extract_final_progress (ExtractJob *extract_job,
                               gint        total_files)
{
    char *status;
    g_autofree gchar *basename_dest = NULL;
    g_autofree gchar *formatted_size = NULL;

    nautilus_progress_info_set_destination (extract_job->common.progress,
                                            extract_job->destination_directory);
    basename_dest = get_basename (extract_job->destination_directory);

    if (total_files == 1)
    {
        GFile *source_file;
        g_autofree gchar *basename = NULL;

        source_file = G_FILE (extract_job->source_files->data);
        basename = get_basename (source_file);
        status = g_strdup_printf (_("Extracted “%s” to “%s”"),
                                  basename,
                                  basename_dest);
    }
    else
    {
        status = g_strdup_printf (ngettext ("Extracted %'d file to “%s”",
                                            "Extracted %'d files to “%s”",
                                            total_files),
                                  total_files,
                                  basename_dest);
    }

    nautilus_progress_info_take_status (extract_job->common.progress,
                                        status);
    formatted_size = g_format_size (extract_job->total_compressed_size);
    nautilus_progress_info_take_details (extract_job->common.progress,
                                         g_strdup_printf (_("%s / %s"),
                                                          formatted_size,
                                                          formatted_size));
}

static void
extract_task_thread_func (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
    ExtractJob *extract_job = task_data;
    GList *l;
    GList *existing_output_files = NULL;
    gint total_files;
    g_autofree guint64 *archive_compressed_sizes = NULL;
    gint i;

    g_timer_start (extract_job->common.time);

    nautilus_progress_info_start (extract_job->common.progress);

    nautilus_progress_info_set_details (extract_job->common.progress,
                                        _("Preparing to extract"));

    total_files = g_list_length (extract_job->source_files);

    archive_compressed_sizes = g_malloc0_n (total_files, sizeof (guint64));
    extract_job->total_compressed_size = 0;

    for (l = extract_job->source_files, i = 0;
         l != NULL && !job_aborted ((CommonJob *) extract_job);
         l = l->next, i++)
    {
        GFile *source_file;
        g_autoptr (GFileInfo) info = NULL;

        source_file = G_FILE (l->data);
        info = g_file_query_info (source_file,
                                  G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  extract_job->common.cancellable,
                                  NULL);

        if (info)
        {
            archive_compressed_sizes[i] = g_file_info_get_size (info);
            extract_job->total_compressed_size += archive_compressed_sizes[i];
        }
    }

    extract_job->base_progress = 0;

    for (l = extract_job->source_files, i = 0;
         l != NULL && !job_aborted ((CommonJob *) extract_job);
         l = l->next, i++)
    {
        g_autoptr (AutoarExtractor) extractor = NULL;

        extractor = autoar_extractor_new (G_FILE (l->data),
                                          extract_job->destination_directory);

        autoar_extractor_set_notify_interval (extractor,
                                              PROGRESS_NOTIFY_INTERVAL);
        g_signal_connect (extractor, "scanned",
                          G_CALLBACK (extract_job_on_scanned),
                          extract_job);
        g_signal_connect (extractor, "error",
                          G_CALLBACK (extract_job_on_error),
                          extract_job);
        g_signal_connect (extractor, "decide-destination",
                          G_CALLBACK (extract_job_on_decide_destination),
                          extract_job);
        g_signal_connect (extractor, "progress",
                          G_CALLBACK (extract_job_on_progress),
                          extract_job);
        g_signal_connect (extractor, "completed",
                          G_CALLBACK (extract_job_on_completed),
                          extract_job);
        g_signal_connect (extractor, "request-passphrase",
                          G_CALLBACK (extract_job_on_request_passphrase),
                          extract_job);

        extract_job->archive_compressed_size = archive_compressed_sizes[i];
        extract_job->destination_decided = FALSE;

        autoar_extractor_start (extractor,
                                extract_job->common.cancellable);

        g_signal_handlers_disconnect_by_data (extractor,
                                              extract_job);

        extract_job->base_progress += (gdouble) extract_job->archive_compressed_size /
                                      (gdouble) extract_job->total_compressed_size;
    }

    if (!job_aborted ((CommonJob *) extract_job))
    {
        report_extract_final_progress (extract_job, total_files);
    }

    for (l = extract_job->output_files; l != NULL; l = l->next)
    {
        GFile *output_file;

        output_file = G_FILE (l->data);

        if (g_file_query_exists (output_file, NULL))
        {
            existing_output_files = g_list_prepend (existing_output_files,
                                                    g_object_ref (output_file));
        }
    }

    g_list_free_full (extract_job->output_files, g_object_unref);

    extract_job->output_files = existing_output_files;

    if (extract_job->common.undo_info)
    {
        if (extract_job->output_files)
        {
            NautilusFileUndoInfoExtract *undo_info;

            undo_info = NAUTILUS_FILE_UNDO_INFO_EXTRACT (extract_job->common.undo_info);

            nautilus_file_undo_info_extract_set_outputs (undo_info,
                                                         extract_job->output_files);
        }
        else
        {
            /* There is nothing to undo if there is no output */
            g_clear_object (&extract_job->common.undo_info);
        }
    }
}

void
nautilus_file_operations_extract_files (GList                          *files,
                                        GFile                          *destination_directory,
                                        GtkWindow                      *parent_window,
                                        NautilusFileOperationsDBusData *dbus_data,
                                        NautilusExtractCallback         done_callback,
                                        gpointer                        done_callback_data)
{
    ExtractJob *extract_job;
    g_autoptr (GTask) task = NULL;

    extract_job = op_job_new (ExtractJob, parent_window, dbus_data);
    extract_job->source_files = g_list_copy_deep (files,
                                                  (GCopyFunc) g_object_ref,
                                                  NULL);
    extract_job->destination_directory = g_object_ref (destination_directory);
    extract_job->done_callback = done_callback;
    extract_job->done_callback_data = done_callback_data;

    inhibit_power_manager ((CommonJob *) extract_job, _("Extracting Files"));

    if (!nautilus_file_undo_manager_is_operating ())
    {
        extract_job->common.undo_info = nautilus_file_undo_info_extract_new (files,
                                                                             destination_directory);
    }

    task = g_task_new (NULL, extract_job->common.cancellable,
                       extract_task_done, extract_job);
    g_task_set_task_data (task, extract_job, NULL);
    g_task_run_in_thread (task, extract_task_thread_func);
}

static void
compress_task_done (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    CompressJob *compress_job = user_data;

    if (compress_job->done_callback)
    {
        compress_job->done_callback (compress_job->output_file,
                                     compress_job->success,
                                     compress_job->done_callback_data);
    }

    g_object_unref (compress_job->output_file);
    g_list_free_full (compress_job->source_files, g_object_unref);
    g_free (compress_job->passphrase);

    finalize_common ((CommonJob *) compress_job);

    nautilus_file_changes_consume_changes (TRUE);
}

static void
compress_job_on_progress (AutoarCompressor *compressor,
                          guint64           completed_size,
                          guint             completed_files,
                          gpointer          user_data)
{
    CompressJob *compress_job = user_data;
    CommonJob *common = user_data;
    char *status;
    char *details;
    int files_left;
    double elapsed;
    double transfer_rate;
    int remaining_time;
    g_autofree gchar *basename_output_file = NULL;

    files_left = compress_job->total_files - completed_files;
    basename_output_file = get_basename (compress_job->output_file);
    if (compress_job->total_files == 1)
    {
        g_autofree gchar *basename_data = NULL;

        basename_data = get_basename (G_FILE (compress_job->source_files->data));
        status = g_strdup_printf (_("Compressing “%s” into “%s”"),
                                  basename_data,
                                  basename_output_file);
    }
    else
    {
        status = g_strdup_printf (ngettext ("Compressing %'d file into “%s”",
                                            "Compressing %'d files into “%s”",
                                            compress_job->total_files),
                                  compress_job->total_files,
                                  basename_output_file);
    }
    nautilus_progress_info_take_status (common->progress, status);

    elapsed = g_timer_elapsed (common->time, NULL);

    transfer_rate = 0;
    remaining_time = -1;

    if (elapsed > 0)
    {
        if (completed_size > 0)
        {
            transfer_rate = completed_size / elapsed;
            remaining_time = (compress_job->total_size - completed_size) / transfer_rate;
        }
        else if (completed_files > 0)
        {
            transfer_rate = completed_files / elapsed;
            remaining_time = (compress_job->total_files - completed_files) / transfer_rate;
        }
    }

    if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE ||
        transfer_rate == 0)
    {
        if (compress_job->total_files == 1)
        {
            g_autofree gchar *formatted_size_completed_size = NULL;
            g_autofree gchar *formatted_size_total_size = NULL;

            formatted_size_completed_size = g_format_size (completed_size);
            formatted_size_total_size = g_format_size (compress_job->total_size);
            /* To translators: %s will expand to a size like "2 bytes" or "3 MB", so something like "4 kb / 4 MB" */
            details = g_strdup_printf (_("%s / %s"), formatted_size_completed_size,
                                       formatted_size_total_size);
        }
        else
        {
            details = g_strdup_printf (_("%'d / %'d"),
                                       files_left > 0 ? completed_files + 1 : completed_files,
                                       compress_job->total_files);
        }
    }
    else
    {
        if (compress_job->total_files == 1)
        {
            g_autofree gchar *formatted_size_completed_size = NULL;
            g_autofree gchar *formatted_size_total_size = NULL;

            formatted_size_completed_size = g_format_size (completed_size);
            formatted_size_total_size = g_format_size (compress_job->total_size);

            if (files_left > 0)
            {
                g_autofree gchar *formatted_time = NULL;
                g_autofree gchar *formatted_size_transfer_rate = NULL;

                formatted_time = get_formatted_time (remaining_time);
                formatted_size_transfer_rate = g_format_size ((goffset) transfer_rate);
                /* To translators: %s will expand to a size like "2 bytes" or "3 MB", %s to a time duration like
                 * "2 minutes". So the whole thing will be something like "2 kb / 4 MB -- 2 hours left (4kb/sec)"
                 *
                 * The singular/plural form will be used depending on the remaining time (i.e. the %s argument).
                 */
                details = g_strdup_printf (ngettext ("%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                                     "%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                                     seconds_count_format_time_units (remaining_time)),
                                           formatted_size_completed_size,
                                           formatted_size_total_size,
                                           formatted_time,
                                           formatted_size_transfer_rate);
            }
            else
            {
                /* To translators: %s will expand to a size like "2 bytes" or "3 MB". */
                details = g_strdup_printf (_("%s / %s"),
                                           formatted_size_completed_size,
                                           formatted_size_total_size);
            }
        }
        else
        {
            if (files_left > 0)
            {
                g_autofree gchar *formatted_time = NULL;
                g_autofree gchar *formatted_size = NULL;

                formatted_time = get_formatted_time (remaining_time);
                formatted_size = g_format_size ((goffset) transfer_rate);
                /* To translators: %s will expand to a time duration like "2 minutes".
                 * So the whole thing will be something like "1 / 5 -- 2 hours left (4kb/sec)"
                 *
                 * The singular/plural form will be used depending on the remaining time (i.e. the %s argument).
                 */
                details = g_strdup_printf (ngettext ("%'d / %'d \xE2\x80\x94 %s left (%s/sec)",
                                                     "%'d / %'d \xE2\x80\x94 %s left (%s/sec)",
                                                     seconds_count_format_time_units (remaining_time)),
                                           completed_files + 1, compress_job->total_files,
                                           formatted_time,
                                           formatted_size);
            }
            else
            {
                /* To translators: %'d is the number of files completed for the operation,
                 * so it will be something like 2/14. */
                details = g_strdup_printf (_("%'d / %'d"),
                                           completed_files,
                                           compress_job->total_files);
            }
        }
    }

    nautilus_progress_info_take_details (common->progress, details);

    if (elapsed > SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE)
    {
        nautilus_progress_info_set_remaining_time (common->progress,
                                                   remaining_time);
        nautilus_progress_info_set_elapsed_time (common->progress,
                                                 elapsed);
    }

    nautilus_progress_info_set_progress (common->progress,
                                         completed_size,
                                         compress_job->total_size);
}

static void
compress_job_on_error (AutoarCompressor *compressor,
                       GError           *error,
                       gpointer          user_data)
{
    CompressJob *compress_job = user_data;
    char *status;
    g_autofree gchar *basename_output_file = NULL;

    basename_output_file = get_basename (compress_job->output_file);
    if (compress_job->total_files == 1)
    {
        g_autofree gchar *basename_data = NULL;

        basename_data = get_basename (G_FILE (compress_job->source_files->data));
        status = g_strdup_printf (_("Error compressing “%s” into “%s”"),
                                  basename_data,
                                  basename_output_file);
    }
    else
    {
        status = g_strdup_printf (ngettext ("Error compressing %'d file into “%s”",
                                            "Error compressing %'d files into “%s”",
                                            compress_job->total_files),
                                  compress_job->total_files,
                                  basename_output_file);
    }
    nautilus_progress_info_take_status (compress_job->common.progress,
                                        status);

    run_error ((CommonJob *) compress_job,
               g_strdup (_("There was an error while compressing files.")),
               g_strdup (error->message),
               NULL,
               FALSE,
               CANCEL,
               NULL);

    abort_job ((CommonJob *) compress_job);
}

static void
compress_job_on_completed (AutoarCompressor *compressor,
                           gpointer          user_data)
{
    CompressJob *compress_job = user_data;
    g_autoptr (GFile) destination_directory = NULL;
    char *status;
    g_autofree gchar *basename_output_file = NULL;

    basename_output_file = get_basename (compress_job->output_file);
    if (compress_job->total_files == 1)
    {
        g_autofree gchar *basename_data = NULL;

        basename_data = get_basename (G_FILE (compress_job->source_files->data));
        status = g_strdup_printf (_("Compressed “%s” into “%s”"),
                                  basename_data,
                                  basename_output_file);
    }
    else
    {
        status = g_strdup_printf (ngettext ("Compressed %'d file into “%s”",
                                            "Compressed %'d files into “%s”",
                                            compress_job->total_files),
                                  compress_job->total_files,
                                  basename_output_file);
    }

    nautilus_progress_info_take_status (compress_job->common.progress,
                                        status);

    nautilus_file_changes_queue_file_added (compress_job->output_file);

    destination_directory = g_file_get_parent (compress_job->output_file);
    nautilus_progress_info_set_destination (compress_job->common.progress,
                                            destination_directory);
}

static void
compress_task_thread_func (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
    CompressJob *compress_job = task_data;
    g_auto (SourceInfo) source_info = SOURCE_INFO_INIT;
    g_autoptr (AutoarCompressor) compressor = NULL;

    g_timer_start (compress_job->common.time);

    nautilus_progress_info_start (compress_job->common.progress);

    scan_sources (compress_job->source_files,
                  &source_info,
                  (CommonJob *) compress_job,
                  OP_KIND_COMPRESS);

    compress_job->total_files = source_info.num_files;
    compress_job->total_size = source_info.num_bytes;

    compressor = autoar_compressor_new (compress_job->source_files,
                                        compress_job->output_file,
                                        compress_job->format,
                                        compress_job->filter,
                                        FALSE);
    autoar_compressor_set_passphrase (compressor, compress_job->passphrase);

    autoar_compressor_set_output_is_dest (compressor, TRUE);

    autoar_compressor_set_notify_interval (compressor,
                                           PROGRESS_NOTIFY_INTERVAL);

    g_signal_connect (compressor, "progress",
                      G_CALLBACK (compress_job_on_progress), compress_job);
    g_signal_connect (compressor, "error",
                      G_CALLBACK (compress_job_on_error), compress_job);
    g_signal_connect (compressor, "completed",
                      G_CALLBACK (compress_job_on_completed), compress_job);
    autoar_compressor_start (compressor,
                             compress_job->common.cancellable);

    compress_job->success = g_file_query_exists (compress_job->output_file,
                                                 NULL);

    /* There is nothing to undo if the output was not created */
    if (compress_job->common.undo_info != NULL && !compress_job->success)
    {
        g_clear_object (&compress_job->common.undo_info);
    }
}

void
nautilus_file_operations_compress (GList                          *files,
                                   GFile                          *output,
                                   AutoarFormat                    format,
                                   AutoarFilter                    filter,
                                   const gchar                    *passphrase,
                                   GtkWindow                      *parent_window,
                                   NautilusFileOperationsDBusData *dbus_data,
                                   NautilusCreateCallback          done_callback,
                                   gpointer                        done_callback_data)
{
    g_autoptr (GTask) task = NULL;
    CompressJob *compress_job;

    compress_job = op_job_new (CompressJob, parent_window, dbus_data);
    compress_job->source_files = g_list_copy_deep (files,
                                                   (GCopyFunc) g_object_ref,
                                                   NULL);
    compress_job->output_file = g_object_ref (output);
    compress_job->format = format;
    compress_job->filter = filter;
    compress_job->passphrase = g_strdup (passphrase);
    compress_job->done_callback = done_callback;
    compress_job->done_callback_data = done_callback_data;

    inhibit_power_manager ((CommonJob *) compress_job, _("Compressing Files"));

    if (!nautilus_file_undo_manager_is_operating ())
    {
        compress_job->common.undo_info = nautilus_file_undo_info_compress_new (files,
                                                                               output,
                                                                               format,
                                                                               filter,
                                                                               passphrase);
    }

    task = g_task_new (NULL, compress_job->common.cancellable,
                       compress_task_done, compress_job);
    g_task_set_task_data (task, compress_job, NULL);
    g_task_run_in_thread (task, compress_task_thread_func);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_operations (void)
{
    setlocale (LC_MESSAGES, "C");


    /* test the next duplicate name generator */
    EEL_CHECK_STRING_RESULT (get_duplicate_name (" (copy)", 1, -1, FALSE), " (another copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo", 1, -1, FALSE), "foo (copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name (".bashrc", 1, -1, FALSE), ".bashrc (copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name (".foo.txt", 1, -1, FALSE), ".foo (copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo", 1, -1, FALSE), "foo foo (copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo.txt", 1, -1, FALSE), "foo (copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo.txt", 1, -1, FALSE), "foo foo (copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo.txt txt", 1, -1, FALSE), "foo foo (copy).txt txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo...txt", 1, -1, FALSE), "foo.. (copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo...", 1, -1, FALSE), "foo... (copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo. (copy)", 1, -1, FALSE), "foo. (another copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (copy)", 1, -1, FALSE), "foo (another copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (copy).txt", 1, -1, FALSE), "foo (another copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (another copy)", 1, -1, FALSE), "foo (3rd copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (another copy).txt", 1, -1, FALSE), "foo (3rd copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (another copy).txt", 1, -1, FALSE), "foo foo (3rd copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (13th copy)", 1, -1, FALSE), "foo (14th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (13th copy).txt", 1, -1, FALSE), "foo (14th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (21st copy)", 1, -1, FALSE), "foo (22nd copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (21st copy).txt", 1, -1, FALSE), "foo (22nd copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (22nd copy)", 1, -1, FALSE), "foo (23rd copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (22nd copy).txt", 1, -1, FALSE), "foo (23rd copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (23rd copy)", 1, -1, FALSE), "foo (24th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (23rd copy).txt", 1, -1, FALSE), "foo (24th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (24th copy)", 1, -1, FALSE), "foo (25th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (24th copy).txt", 1, -1, FALSE), "foo (25th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (24th copy)", 1, -1, FALSE), "foo foo (25th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (24th copy).txt", 1, -1, FALSE), "foo foo (25th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (100000000000000th copy).txt", 1, -1, FALSE), "foo foo (copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (10th copy)", 1, -1, FALSE), "foo (11th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (10th copy).txt", 1, -1, FALSE), "foo (11th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (11th copy)", 1, -1, FALSE), "foo (12th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (11th copy).txt", 1, -1, FALSE), "foo (12th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (12th copy)", 1, -1, FALSE), "foo (13th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (12th copy).txt", 1, -1, FALSE), "foo (13th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (110th copy)", 1, -1, FALSE), "foo (111th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (110th copy).txt", 1, -1, FALSE), "foo (111th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (122nd copy)", 1, -1, FALSE), "foo (123rd copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (122nd copy).txt", 1, -1, FALSE), "foo (123rd copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (123rd copy)", 1, -1, FALSE), "foo (124th copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (123rd copy).txt", 1, -1, FALSE), "foo (124th copy).txt");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("dir.with.dots", 1, -1, TRUE), "dir.with.dots (copy)");
    EEL_CHECK_STRING_RESULT (get_duplicate_name ("dir (copy).dir", 1, -1, TRUE), "dir (another copy).dir");

    setlocale (LC_MESSAGES, "");
}

#endif
