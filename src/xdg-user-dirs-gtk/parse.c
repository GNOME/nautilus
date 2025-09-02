#include "parse.h"
#include <string.h>

static char *
get_gtk_bookmarks_filename (void)
{
  char *filename, *legacy_filename;
  gboolean exists, legacy_exists;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  exists = g_file_test (filename, G_FILE_TEST_EXISTS);

  if (exists)
    return filename;

  legacy_filename = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);
  legacy_exists = g_file_test (legacy_filename, G_FILE_TEST_EXISTS);

  if (legacy_exists)
    {
      g_free (filename);
      return legacy_filename;
    }

  g_free (legacy_filename);

  /* if neither exist, return the new filename */
  return filename;
}

char *
parse_xdg_dirs_locale (void)
{
  char *file, *content;
  char *locale;

  locale = NULL;
  file = g_build_filename (g_get_user_config_dir (),
                           "user-dirs.locale", NULL);
  if (g_file_get_contents (file, &content, NULL, NULL))
    locale = g_strchug (g_strchomp (content));
  g_free (file);
  return locale;
}

XdgDirEntry *
parse_xdg_dirs (const char *config_file)
{
  GArray *array;
  char *config_file_free = NULL;
  XdgDirEntry dir;
  char *data;
  char **lines;
  char *p, *d;
  int i;
  char *type_start, *type_end;
  char *value, *unescaped;
  gboolean relative;

  array = g_array_new (TRUE, TRUE, sizeof (XdgDirEntry));

  if (config_file == NULL)
    {
      config_file_free = g_build_filename (g_get_user_config_dir (),
                                           "user-dirs.dirs", NULL);
      config_file = (const char *)config_file_free;
    }

  if (!g_file_get_contents (config_file, &data, NULL, NULL))
    goto out;

  lines = g_strsplit (data, "\n", 0);
  g_free (data);
  for (i = 0; lines[i] != NULL; i++)
    {
      p = lines[i];
      while (g_ascii_isspace (*p))
        p++;

      if (*p == '#')
        continue;

      value = strchr (p, '=');
      if (value == NULL)
        continue;
      *value++ = 0;

      g_strchug (g_strchomp (p));
      if (!g_str_has_prefix (p, "XDG_"))
        continue;
      if (!g_str_has_suffix (p, "_DIR"))
        continue;
      type_start = p + 4;
      type_end = p + strlen (p) - 4;

      while (g_ascii_isspace (*value))
        value++;

      if (*value != '"')
        continue;
      value++;

      relative = FALSE;
      if (g_str_has_prefix (value, "$HOME/"))
        {
          relative = TRUE;
          value += 6;
        }
      else if (*value != '/')
        continue;

      d = unescaped = g_malloc (strlen (value) + 1);
      while (*value && *value != '"')
        {
          if ((*value == '\\') && (*(value + 1) != 0))
            value++;
          *d++ = *value++;
        }
      *d = 0;

      *type_end = 0;
      dir.type = g_strdup (type_start);
      if (relative)
        {
          dir.path = g_build_filename (g_get_home_dir (), unescaped, NULL);
          g_free (unescaped);
        }
      else
        dir.path = unescaped;

      g_array_append_val (array, dir);
    }

  g_strfreev (lines);

 out:
  g_free (config_file_free);
  return (XdgDirEntry *)g_array_free (array, FALSE);
}

GList *
parse_gtk_bookmarks (void)
{
  char *filename, *contents;
  GError **error = NULL;
  char **lines;
  int i;
  GtkBookmark *bookmark;
  GList *bookmarks;

  filename = get_gtk_bookmarks_filename ();
  bookmarks = NULL;

  /* Read new list from file */
  if (g_file_get_contents (filename, &contents, NULL, error))
    {
      lines = g_strsplit (contents, "\n", -1);
      g_free (contents);
      for (i = 0; lines[i]; i++)
        {
          if (lines[i][0])
            {
              /* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space */
              /* we must seperate the bookmark uri and the potential label */
              char *space;

              bookmark = g_new0 (GtkBookmark, 1);

              bookmark->label = NULL;
              space = strchr (lines[i], ' ');
              if (space)
                {
                  *space = '\0';
                  bookmark->label = g_strdup (space + 1);
                }
              bookmark->uri = g_strdup (lines[i]);

              bookmarks = g_list_append (bookmarks, bookmark);
            }
        }
      g_strfreev (lines);
    }
  g_free (filename);

  return bookmarks;
}

void
save_gtk_bookmarks (GList *bookmarks)
{
  char *filename, *dirname;
  GString *str;
  GList *l;
  GtkBookmark *bookmark;

  filename = get_gtk_bookmarks_filename ();
  str = g_string_new ("");

  for (l = bookmarks; l != NULL; l = l->next)
    {
      bookmark = l->data;

      if (bookmark->label)
        g_string_append_printf (str, "%s %s\n", bookmark->uri, bookmark->label);
      else
        g_string_append_printf (str, "%s\n", bookmark->uri);
    }

  dirname = g_path_get_dirname (filename);
  if (g_mkdir_with_parents (dirname, 0700) == 0)
    g_file_set_contents (filename, str->str, str->len, NULL);

  g_string_free (str, TRUE);
  g_free (filename);
  g_free (dirname);
}
