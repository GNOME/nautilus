/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-desktop-file-loader.c

   Copyright (C) 2001 Red Hat, Inc.

   Developers: Havoc Pennington <hp@redhat.com>
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.

*/

#include <config.h>
#include "nautilus-desktop-file-loader.h"
#include "nautilus-program-choosing.h"

#include <libgnome/libgnome.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-vfs-extensions.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <iconv.h>
#include <langinfo.h>

typedef struct NautilusDesktopFileSection NautilusDesktopFileSection;
typedef struct NautilusDesktopFileAddition NautilusDesktopFileAddition;
typedef struct NautilusDesktopFileForeachData NautilusDesktopFileForeachData;

typedef void (* NautilusDesktopFileForeachFunc) (NautilusDesktopFile *df,
						 const char     *name,
						 gpointer        data);


struct NautilusDesktopFile {
        char **lines;
  
        /* hash of section names, from pointer to start of name (just after
         * bracket) to NautilusDesktopFileSection structs
         */  
        GHashTable *section_hash;
	GList *addition_list;

        NautilusDesktopFileSection *main_section;
};

struct NautilusDesktopFileSection {
        /* pointer into a line of df->lines for start of name */
        const char *name;

        /* hash of keys in the section,
         * from pointer to start of key name to
         * pointer to start of value, not copied
         * from df->lines
         */
        GHashTable *key_hash;

        /* First line in the section (not the [section name] line,
         * but the one after that)
         */
        char **start_line;
};

struct NautilusDesktopFileAddition {
	NautilusDesktopFileSection *section;
	char *name;
	char *value;

	/* used for saving */
	gboolean saving_section;
};

struct NautilusDesktopFileForeachData {
        NautilusDesktopFile *df;
        NautilusDesktopFileForeachFunc func;
        gpointer user_data;
        gboolean include_localized;
};

char**                       nautilus_desktop_file_get_lines       (NautilusDesktopFile             *df);
void                         nautilus_desktop_file_foreach_section (NautilusDesktopFile             *df,
								    NautilusDesktopFileForeachFunc   func,
								    gpointer                         user_data);
void                         nautilus_desktop_file_foreach_key     (NautilusDesktopFile             *df,
								    const char                      *section,
								    gboolean                         include_localized,
								    NautilusDesktopFileForeachFunc   func,
								    gpointer                         user_data);

static NautilusDesktopFile *        nautilus_desktop_file_new             (void);
static void                         hash_lines                            (NautilusDesktopFile             *df);
static NautilusDesktopFileSection*  section_new                           (const char                      *name,
									   char                           **start_line);
static void                         section_free                          (NautilusDesktopFileSection      *sect);
static char*                        section_dup_name                      (NautilusDesktopFileSection      *sect);
static char*                        validated_strdup                      (const char                      *str);
static NautilusDesktopFileAddition* addition_new                          (NautilusDesktopFileSection      *section,
									   const char                      *name,
									   const char                      *value);
static void                         addition_free                         (NautilusDesktopFileAddition     *addition);


static NautilusDesktopFile*
nautilus_desktop_file_new (void)
{
        NautilusDesktopFile *df;

        df = g_new0 (NautilusDesktopFile, 1);

        return df;
}

GnomeVFSResult
nautilus_desktop_file_load (const char *uri,
			    NautilusDesktopFile **desktop_file)
{
        char *contents;
        NautilusDesktopFile *df;
	GnomeVFSResult result;
	int file_size;

	*desktop_file = NULL;
	result = eel_read_entire_file (uri, &file_size,
				       &contents);
	if (result != GNOME_VFS_OK) {
		return result;
	}

	df = nautilus_desktop_file_from_string (contents);

        g_free (contents);

	*desktop_file = df;
	
        return GNOME_VFS_OK;
}

NautilusDesktopFile*
nautilus_desktop_file_from_string (const char *data)
{
        NautilusDesktopFile *df;

        df = nautilus_desktop_file_new ();

        df->lines = g_strsplit (data, "\n", G_MAXINT);

        hash_lines (df);
  
        return df;
}

static GnomeVFSResult
write_all (GnomeVFSHandle *handle,
	   gconstpointer buffer,
	   GnomeVFSFileSize bytes)
{
	GnomeVFSFileSize bytes_written;
	GnomeVFSResult result;

	while (bytes > 0) {
		result = gnome_vfs_write (handle, buffer,
					  bytes, &bytes_written);
		if (result != GNOME_VFS_OK) {
			return result;
		}
		bytes -= bytes_written;
	}
	
	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_desktop_file_save (NautilusDesktopFile *df,
			    const char *uri)
{
	NautilusDesktopFileAddition *addition;
	GnomeVFSHandle *handle;
	GnomeVFSResult result;
	char *old_val;
	GList *list;
	gint i;

	g_return_val_if_fail (df != NULL, FALSE);
	g_return_val_if_fail (df->lines != NULL, FALSE);

	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_WRITE);
	if (result != GNOME_VFS_OK) {
		return result;
	}

	for (i = 0; df->lines[i] != NULL; i++) {
		gboolean handled_line;

		handled_line = FALSE;
		for (list = df->addition_list; list; list = list->next) {
			addition = (NautilusDesktopFileAddition *)list->data;
			
			if (addition->section->start_line[0] == df->lines[i]) {
				addition->saving_section = TRUE;
			}

			if (addition->saving_section && df->lines[i][0] == '[') {
				addition->saving_section = FALSE;
			}

			if (addition->saving_section &&
			    (strncmp (addition->name, df->lines[i], strlen (addition->name)) == 0)) {
				old_val = strstr (df->lines[i], "=");
				if (old_val == NULL) {
					continue;
				}
				result = write_all (handle,
						    df->lines[i],
						    old_val + 1 - df->lines[i]);
				if (result != GNOME_VFS_OK) {
					gnome_vfs_close (handle);
					return result;
				}
				result = write_all (handle,
						    addition->value,
						    strlen (addition->value));
				if (result != GNOME_VFS_OK) {
					gnome_vfs_close (handle);
					return result;
				}
				result = write_all (handle, "\n", 1);
				if (result != GNOME_VFS_OK) {
					gnome_vfs_close (handle);
					return result;
				}

				handled_line = TRUE;
			}
		}
		if (handled_line == FALSE) {
			result = write_all (handle,
					    df->lines[i],
					    strlen (df->lines[i]));
			if (result != GNOME_VFS_OK) {
				gnome_vfs_close (handle);
				return result;
			}
			result = write_all (handle, "\n", 1);
			if (result != GNOME_VFS_OK) {
				gnome_vfs_close (handle);
				return result;
			}
		}
	}

	gnome_vfs_close (handle);
	return GNOME_VFS_OK;
}

static void
destroy_foreach (gpointer key, gpointer value, gpointer data)
{
        section_free (value);
}


void
nautilus_desktop_file_free (NautilusDesktopFile *df)
{
        if (df->section_hash != NULL) {
                g_hash_table_foreach (df->section_hash, destroy_foreach, NULL);
                g_hash_table_destroy (df->section_hash);
        }

	g_list_foreach (df->addition_list, (GFunc) addition_free, NULL);
	g_list_free (df->addition_list);
        if (df->lines != NULL)
                g_strfreev (df->lines);

        g_free (df);
}


/* This is cut and pasted from glib 1.3.x
 * It should be removed when porting to glib 2
 */
/**
 * g_strdupv:
 * @str_array: %NULL-terminated array of strings
 * 
 * Copies %NULL-terminated array of strings. The copy is a deep copy;
 * the new array should be freed by first freeing each string, then
 * the array itself. g_strfreev() does this for you. If called
 * on a %NULL value, g_strdupv() simply returns %NULL.
 * 
 * Return value: a new %NULL-terminated array of strings
 **/
static gchar**
cnp_g_strdupv (gchar **str_array)
{
        if (str_array) {
                gint i;
                gchar **retval;

                i = 0;
                while (str_array[i])
                        ++i;
          
                retval = g_new (gchar*, i + 1);

                i = 0;
                while (str_array[i]) {
                        retval[i] = g_strdup (str_array[i]);
                        ++i;
                }
                retval[i] = NULL;

                return retval;
        } else {
                return NULL;
        }
}

char**
nautilus_desktop_file_get_lines (NautilusDesktopFile *df)
{
        return cnp_g_strdupv (df->lines);
}

/* Custom hash functions allow us to avoid strdups */
static gboolean
key_equal (gconstpointer v1,
	   gconstpointer v2)
{
        const gchar *p1 = v1;
        const gchar *p2 = v2;
  
        /* we count '=' and ' ' as terminator
         * and don't count leading/trailing spaces
         */

        while (isspace ((guchar)*p1)) {
                ++p1;
	}

        while (isspace ((guchar)*p2)) {
                ++p2;
	}
  
        while (*p1 && *p2 &&
               *p1 != '=' && *p2 != '=' &&
               !isspace ((guchar)*p1) && !isspace ((guchar)*p2)) {
                if (*p1 != *p2) {
                        return FALSE;
		}

                ++p1;
                ++p2;
        }

        if (*p1 && *p1 != '=' && !isspace ((guchar)*p1)) {
                return FALSE;
	}

        if (*p2 && *p2 != '=' && !isspace ((guchar)*p2)) {
                return FALSE;
	}
  
        return TRUE;
}

static guint
key_hash (gconstpointer key)
{
        const char *p = key;
        guint h = *p;
  
        /* we count '=' and ' ' as terminator
         * and don't count leading/trailing spaces
         */
  
        while (isspace ((guchar)*p)) {
                ++p;
	}

        if (h) {
                for (p += 1; *p != '\0' && *p != '=' && !isspace ((guchar)*p); p++) {
                        h = (h << 5) - h + *p;
		}
	}

        return h;
}

static gboolean
section_equal (gconstpointer v1,
	       gconstpointer v2)
{
        const gchar *p1 = v1;
        const gchar *p2 = v2;
  
        /* we count ']' as terminator */
  
        while (*p1 && *p2 &&
               *p1 != ']' && *p2 != ']') {
                if (*p1 != *p2) {
                        return FALSE;
		}

                ++p1;
                ++p2;
        }

        if (*p1 && *p1 != ']') {
                return FALSE;
	}

        if (*p2 && *p2 != ']') {
                return FALSE;
	}
  
        return TRUE;
}

static guint
section_hash (gconstpointer key)
{
        const char *p = key;
        guint h = *p;
  
        /* we count ']' as terminator */  
  
        if (h) {
                for (p += 1; *p != '\0' && *p != ']'; p++) {
                        h = (h << 5) - h + *p;
		}
	}

        return h;
}

static void
hash_lines (NautilusDesktopFile *df)
{
        NautilusDesktopFileSection *current_sect;
	const char *eq;
	const char *p;
        char **iter;
        
        if (df->section_hash == NULL) {
                df->section_hash = g_hash_table_new (section_hash, section_equal);
	}

        current_sect = NULL;
        iter = df->lines;

        while (iter != NULL && *iter != NULL) {
                p = *iter;

                while (isspace ((guchar)*p)) {
                        ++p;
		}

                /* blank or comment lines */
                if (*p == '\0' || *p == '#') {
                        goto next;
		}
      
                if (*p == '[') {
                        /* Begin a section */
                        ++p;

                        if (*p != ']' &&
                            strchr (p, ']') != NULL) {
                                current_sect = section_new (p, iter + 1);
                                
                                g_hash_table_insert (df->section_hash,
                                                     (char*) current_sect->name,
                                                     current_sect);
                                
                                if (df->main_section == NULL && 
                                    (section_equal (current_sect->name, "Desktop Entry") ||
                                     section_equal (current_sect->name, "KDE Desktop Entry"))) {
                                        df->main_section = current_sect;
                                }
                        }
                } else {
                        /* should be a key=value line, if not
                         * it's some invalid crap
                         */
                        eq = strchr (p, '=');
                        if (eq == NULL) {
                                goto next;
			} else {
                                if (current_sect) {
                                        ++eq;
                                        while (isspace ((guchar)*eq)) {
                                                ++eq;
					}
                                        /* could overwrite an earlier copy of
                                         * the same key name in this section
                                         */
                                        g_hash_table_insert (current_sect->key_hash,
                                                             (char*) p, (char*) eq);
                                }
                        }
                }

        next:
                ++iter;
        }
}

static NautilusDesktopFileSection*
section_new  (const char *name,
              char **start_line)
{
        NautilusDesktopFileSection *sect;

        sect = g_new (NautilusDesktopFileSection, 1);

        sect->name = name;
        sect->start_line = start_line;
        sect->key_hash = g_hash_table_new (key_hash, key_equal);

        return sect;
}

static void
section_free (NautilusDesktopFileSection *sect)
{
        g_hash_table_destroy (sect->key_hash);
        g_free (sect);
}


static NautilusDesktopFileAddition *
addition_new  (NautilusDesktopFileSection *section,
	       const char *name,
	       const char *value)
{
	NautilusDesktopFileAddition *addition;

	addition = g_new (NautilusDesktopFileAddition, 1);

	addition->section = section;
	addition->name = g_strdup (name);
	addition->value = g_strdup (value);

	return addition;
}

static void
addition_free (NautilusDesktopFileAddition *addition)
{
	g_free (addition->name);
	g_free (addition->value);
	g_free (addition);
}

static char*
section_dup_name (NautilusDesktopFileSection *sect)
{
        const char *name_end;
        
        name_end = strchr (sect->name, ']');

        g_assert (name_end); /* we were supposed to verify this on initial parse */

        return g_strndup (sect->name, name_end - sect->name);
}

static NautilusDesktopFileSection*
get_section (NautilusDesktopFile *df,
             const char *section)
{
        if (df->section_hash == NULL) {
                return NULL;
	}
        
        if (section == NULL) {
                return df->main_section;
        } else {
                return g_hash_table_lookup (df->section_hash, section);
        }
}

static void
section_foreach (gpointer key, gpointer value, gpointer data)
{
        NautilusDesktopFileForeachData *fd;
        NautilusDesktopFileSection *sect;
        char *name;

        fd = data;
        sect = value;
        
        name = section_dup_name (sect);
        
        (* fd->func) (fd->df, name, fd->user_data);

        g_free (name);
}

void
nautilus_desktop_file_foreach_section (NautilusDesktopFile *df,
				       NautilusDesktopFileForeachFunc  func,
				       gpointer user_data)
{        
        if (df->section_hash != NULL) {
                NautilusDesktopFileForeachData fd;
                
                fd.df = df;
                fd.func = func;
                fd.user_data = user_data;
                fd.include_localized = FALSE; /* not used */
                
                g_hash_table_foreach (df->section_hash, section_foreach, &fd);
        }
}


static void
key_foreach (gpointer key, gpointer value, gpointer data)
{
        NautilusDesktopFileForeachData *fd;
        char *key_end;
        char *name;

        fd = data;
        
        key_end = (char*) key;
        while (*key_end &&
               !isspace ((guchar)*key_end) &&
               *key_end != '=') {
                ++key_end;
        }

        name = g_strndup (key, key_end - (char*)key);

        if (fd->include_localized ||
            (!fd->include_localized && strchr (name, '[') == NULL)) {
                (* fd->func) (fd->df, name, fd->user_data);
	}

        g_free (name);
}

void
nautilus_desktop_file_foreach_key (NautilusDesktopFile *df,
				   const char *section,
				   gboolean include_localized,
				   NautilusDesktopFileForeachFunc func,
				   gpointer user_data)
{
        NautilusDesktopFileSection *sect;

        sect = get_section (df, section);
        
        if (sect) {
                NautilusDesktopFileForeachData fd;
                
                fd.df = df;
                fd.func = func;
                fd.user_data = user_data;
                fd.include_localized = include_localized;

                g_hash_table_foreach (sect->key_hash, key_foreach, &fd);
        }
}

static const char*
get_keyval (NautilusDesktopFile *df,
            const char *section,
            const char *keyname)
{
	NautilusDesktopFileSection *sect;
	const char *strval;
	
	sect = get_section (df, section);
	
	if (sect == NULL) {
		return FALSE;
	}
	
	strval = g_hash_table_lookup (sect->key_hash,
				      keyname);
	
	return strval;
}

static gboolean 
parse_boolean (const char *strval,
               int         len,
               gboolean   *val)
{
	if (len < 0) {
		len = strlen (strval);
	}

	if (*strval == '1') {
		*val = TRUE;
		return TRUE;
	} else if (len > 3 &&
		   strval[0] == 't' && strval[1] == 'r' &&
		   strval[2] == 'u' && strval[3] == 'e') {
		*val = TRUE;
		return TRUE;
	} else if (*strval == '0') {
		*val = FALSE;
		return TRUE;
	} else if (len > 4 &&
		   strval[0] == 'f' && strval[1] == 'a' &&
		   strval[2] == 'l' && strval[3] == 's' &&
		   strval[4] == 'e') {
		*val = FALSE;
		return TRUE;
	}

	return FALSE;
}

static gboolean 
parse_number (const char *strval,
              int         len,
              double     *val)
{
        char *end;
        double tmp;
  
        if (len < 0) {
                len = strlen (strval);
	}
        
        tmp = strtod (strval, &end);
        if (strval == end) {
                return FALSE;
	}
        
        *val = tmp;
        return TRUE;
}


static void
get_locale (char **lang,
            char **lang_country)
{  
        const char *uscore_pos;
        const char *at_pos;
        const char *dot_pos;
        const char *end_pos;
        const char *locale;
        const char *start_lang;
        const char *end_lang;
        const char *end_country;
        
        *lang = NULL;
        *lang_country = NULL;
  
        locale = setlocale (LC_MESSAGES, NULL);

        if (locale == NULL) {
                return;
	}

        /* lang_country.encoding@modifier */
        
        uscore_pos = strchr (locale, '_');
        dot_pos = strchr (uscore_pos ? uscore_pos : locale, '.');
        at_pos = strchr ((dot_pos != NULL) ? dot_pos : ((uscore_pos != NULL) ? uscore_pos : locale), '@');
        end_pos = locale + strlen (locale);

        start_lang = locale;
        end_lang = ((uscore_pos != NULL) ? uscore_pos :
                    ((dot_pos != NULL) ? dot_pos :
                     ((at_pos != NULL) ? at_pos : end_pos)));
        end_country = ((dot_pos != NULL) ? dot_pos :
                       ((at_pos != NULL) ? at_pos : end_pos));
        
        if (uscore_pos == NULL) {
                *lang = g_strndup (start_lang, end_lang - start_lang);
        } else {
                *lang = g_strndup (start_lang, end_lang - start_lang);
                *lang_country = g_strndup (start_lang,
                                           end_country - start_lang);
        }
}

gboolean
nautilus_desktop_file_get_boolean (NautilusDesktopFile *df,
				   const char *section,
				   const char *keyname,
				   gboolean *val)
{
        const char *strval;
        
        strval = get_keyval (df, section, keyname);
        
        if (strval == NULL) {
                return FALSE;
	}
        
        return parse_boolean (strval, -1, val);
}

gboolean
nautilus_desktop_file_get_number (NautilusDesktopFile *df,
				  const char *section,
				  const char *keyname,
				  double *val)
{
        const char *strval;
        
        strval = get_keyval (df, section, keyname);
        
        if (strval == NULL) {
                return FALSE;
	}

        return parse_number (strval, -1, val);
}

/* Totally bogus UTF-8 stuff */
gboolean
nautilus_desktop_file_get_string (NautilusDesktopFile *df,
				  const char *section,
				  const char *keyname,
				  char **val)
{
        const char *strval;
        char *tmp;
        
        strval = get_keyval (df, section, keyname);
        
        if (strval == NULL) {
                return FALSE;
	}

        tmp = validated_strdup (strval);

        if (tmp != NULL) {
                *val = tmp;
	}
        
        return tmp != NULL;
}

gboolean
nautilus_desktop_file_get_locale_string  (NautilusDesktopFile *df,
					  const char *section,
					  const char *keyname,
					  char **val)
{
        const char *strval;
        char *lang;
        char *lang_country;
        char *s;

        strval = NULL;
        get_locale (&lang, &lang_country);

	/* FIXME - we need to try de_DE.ENCODING in addition to what
	 * we are trying here.
	 */
	
        /* Try "Foo[de_DE]" */
        if (lang_country) {
                s = g_strconcat (keyname, "[", lang_country, "]", NULL);
                strval = get_keyval (df, section, s);
                g_free (s);
                if (strval != NULL)
                        goto done;
        }

        /* Try "Foo[de]" */
        if (lang) {
                s = g_strconcat (keyname, "[", lang, "]", NULL);
                strval = get_keyval (df, section, s);
                g_free (s);
                if (strval != NULL)
                        goto done;
        }

        /* Fall back to not localized */
        strval = get_keyval (df, section, keyname);
        
 done:
        g_free (lang);
        g_free (lang_country);
        
        if (strval == NULL) {
                return FALSE;
	} else {
                char *tmp = validated_strdup (strval);

                if (tmp != NULL) {
                        *val = tmp;
		}
                return tmp != NULL;                  
        }
}

gboolean
nautilus_desktop_file_get_regexp (NautilusDesktopFile   *df,
				  const char *section,
				  const char *keyname,
				  char **val)
{
  return nautilus_desktop_file_get_string (df, section, keyname, val);
}

/* This is extremely broken */

#define F 0   /* character never appears in text */
#define T 1   /* character appears in plain ASCII text */
#define I 2   /* character appears in ISO-8859 text */
#define X 3   /* character appears in non-ISO extended ASCII (Mac, IBM PC) */

static char text_chars[256] = {
        /*                  BEL BS HT LF    FF CR    */
        F, F, F, F, F, F, F, T, T, T, T, F, T, T, F, F,  /* 0x0X */
        /*                              ESC          */
        F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  /* 0x1X */
        T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
        T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
        T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  /* 0x7X */
	/*            NEL                            */
	X, X, X, X, X, T, X, X, X, X, X, X, X, X, X, X,  /* 0x8X */
        X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  /* 0x9X */
        I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xaX */
        I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xbX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xcX */
        I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xdX */
        I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xeX */
        I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I   /* 0xfX */
};

static int
looks_utf8 (const unsigned char *buf,
	    int nbytes,
	    unsigned long *ubuf,
	    int *ulen)
{
        int i, n;
	unsigned long c;
	int gotone = 0;

        *ulen = 0;

	for (i = 0; i < nbytes; i++) {
	        if ((buf[i] & 0x80) == 0) {        /* 0xxxxxxx is plain ASCII */
                 /*
                  * Even if the whole file is valid UTF-8 sequences,
                  * still reject it if it uses weird control characters.
                  */
                        if (text_chars[buf[i]] != T) {
				return 0;
			}

                        if (ubuf != NULL) {
                                ubuf[(*ulen)++] = buf[i];
			}
	        } else if ((buf[i] & 0x40) == 0) { /* 10xxxxxx never 1st byte */

		 	return 0;
                } else {                           /* 11xxxxxx begins UTF-8 */
                        int following;

	                if ((buf[i] & 0x20) == 0) {             /* 110xxxxx */
	                        c = buf[i] & 0x1f;
	                        following = 1;
	                } else if ((buf[i] & 0x10) == 0) {      /* 1110xxxx */
	                        c = buf[i] & 0x0f;
	                        following = 2;
	                } else if ((buf[i] & 0x08) == 0) {      /* 11110xxx */
                                c = buf[i] & 0x07;
                                following = 3;
	                } else if ((buf[i] & 0x04) == 0) {      /* 111110xx */
	                        c = buf[i] & 0x03;
	                        following = 4;
	                } else if ((buf[i] & 0x02) == 0) {      /* 1111110x */
	                        c = buf[i] & 0x01;
	                        following = 5;
	                } else {
	                        return 0;
			}

			for (n = 0; n < following; n++) {
                                i++;
                                if (i >= nbytes) {
                                        goto done;
				}

		                if ((buf[i] & 0x80) == 0 || (buf[i] & 0x40)) {
	                                return 0;
				}

	                        c = (c << 6) + (buf[i] & 0x3f);
	                }

                        if (ubuf != NULL) {
                                ubuf[(*ulen)++] = c;
			}
                        gotone = 1;
                }
	}
 done:
        return gotone;   /* don't claim it's UTF-8 if it's all 7-bit */
}

G_LOCK_DEFINE_STATIC (init_validate);

static char*
validated_strdup (const char *str)
{
        static gchar *locale;
        static gboolean initialized = FALSE;
        gchar *pout, *pin, *buf;
        gint len, ulen = 0, ib, ob;
        iconv_t fd;

        G_LOCK (init_validate);
        if (!initialized) {
                /* whee, we are totally unportable (broken too) */
                setlocale (LC_CTYPE, "");
                locale = nl_langinfo (CODESET);
		initialized = TRUE;
        }
        G_UNLOCK (init_validate);

        buf = NULL;
        
        len = strlen (str);
        if (looks_utf8 (str, len, NULL, &ulen)) {
                if ((fd = iconv_open (locale, "UTF-8")) != (iconv_t)-1) {
                        ib = len;
                        ob = ib * 3;
                        pout = buf = g_new0 (gchar, ob);
                        pin = (char*) str;

                        /* not portable either */
                        
                        if (iconv (fd, &pin, &ib, &pout, &ob) == (size_t)-1) {
                                g_free (buf);
                                buf = NULL;
                        }

                        iconv_close (fd);
                }
        } else {
                buf = g_strdup (str);
        }

        return buf;
}

/* This is cut and pasted from glib 1.3.x
 * It should be removed when porting to glib 2
 */
static void
cnp_g_string_append_len (GString *str,
			 const char *s,
			 int len)
{
	if (len < 0) {
		g_string_append (str, s);
	} else {
		char *tmp;
		tmp = g_strndup (s, len);
		g_string_append (str, tmp);
		g_free (tmp);
	}
}

static char*
nautilus_desktop_file_sub_formats (NautilusDesktopFile *df,
				   const char *src)
{
	GString *new;
	const char *p;
	const char *end;
	char *retval;
	
	new = g_string_new ("");

	p = src;
	end = src;

	p = strchr (p, '%');
	while (p) {
		if (p != end) {
			/* Append what we just scanned over */
		        cnp_g_string_append_len (new, end, p - end);
		}

		end = p;
		
		++p; /* past the % */
		switch (*p) {
		case 'f':
		case 'F':
		case 'u':
		case 'U':
		case 'd':
		case 'D':
		case 'n':
		case 'N':
		case 'i':
		case 'm':
		case 'c':
		case 'k':
		case 'v':
			/* We don't actually sub anything for now */
			++p;
			break;
		case '%':
			/* Escaped % */
			g_string_append (new, "%");
			++p;
			break;
		default:
			/* some broken .desktop-spec-incompliant crack;
			 * try just skipping it.
			 */
			++p;
			break;
		}
		
		p = strchr (p, '%');
	}
	
	g_string_append (new, end);

	retval = new->str;
	g_string_free (new, FALSE);
	return retval;
}

void
nautilus_desktop_file_launch (NautilusDesktopFile *df)
{
	char *type;
	char *url;
	char *exec;
	char *subst;
		
		

	if (!nautilus_desktop_file_get_string (df, NULL, "Type", &type)) {
		return;
	}

	if (strcmp (type, "Link") == 0) {
		url = NULL;
		nautilus_desktop_file_get_string (df, NULL, "URL", &url);

		if (url != NULL) {
			gnome_url_show (url);
		}

		g_free (url);
	} else if (strcmp (type, "Application") == 0) {
		exec = NULL;
		nautilus_desktop_file_get_string (df, NULL, "Exec", &exec);

		if (exec != NULL) {
			gboolean in_terminal;
			
			subst = nautilus_desktop_file_sub_formats (df, exec);

			in_terminal = FALSE;
			nautilus_desktop_file_get_boolean (df, NULL, "Terminal", &in_terminal);

			nautilus_launch_application_from_command ("",
								  subst,
								  NULL,
								  in_terminal);
			g_free (subst);
		}

		g_free (exec);
	}

	g_free (type);
}


gboolean
nautilus_desktop_file_set_string (NautilusDesktopFile *df,
				  const char *section,
				  const char *keyname,
				  const char *value)
{
	NautilusDesktopFileSection *sect;
	NautilusDesktopFileAddition *addition;

	sect = get_section (df, section);
	if (sect == NULL) {
		return FALSE;
	}
	
	addition = addition_new (sect, keyname, value);
	df->addition_list = g_list_append (df->addition_list, addition);
	
	return TRUE; 
}
