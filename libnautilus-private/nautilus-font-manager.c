/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-font-manager.c - Functions for managing fonts.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-font-manager.h"

#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string-list.h"
#include "nautilus-string.h"
#include <libgnome/gnome-util.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <libgnomevfs/gnome-vfs.h>

#define XLFD_INDEX_FOUNDRY		1
#define XLFD_INDEX_FAMILY		2
#define XLFD_INDEX_WEIGHT		3
#define XLFD_INDEX_SLANT		4
#define XLFD_INDEX_SET_WIDTH		5
#define XLFD_INDEX_CHAR_SET_REGISTRY	13
#define XLFD_INDEX_CHAR_SET_ENCODING	14
#define XLFD_INDEX_MAX			XLFD_INDEX_CHAR_SET_ENCODING

#define FONTS_DIR_FILE_NAME		"fonts.dir"
#define FONTS_ALIAS_FILE_NAME		"fonts.alias"
#define FONTS_SCALE_FILE_NAME		"fonts.scale"

#define FONT_SERVER_CONFIG_FILE		"/etc/X11/fs/config"
#define DEFAULT_FONT_DIRECTORY		NAUTILUS_DATADIR "/fonts/urw"
#define USER_FONT_DIRECTORY_NAME	"fonts"

#define NAUTILUS_FONT_UNDEFINED		((NautilusFontType) 0)

#define POSTSCRIPT_FONT_MIME_TYPE	"application/x-font-type1"
#define TRUE_TYPE_FONT_MIME_TYPE	"application/x-font-ttf"

/*
 * FontDescription:
 *
 * A structure that describes a single font entry;
 *
 */
typedef struct {
	char *file_name;
	NautilusFontType font_type;
	char *foundry;
	char *family;
	char *weight;
	char *slant;
	char *set_width;
	char *char_set_registry;
	char *char_set_encoding;
} FontDescription;

/*
 * FontDescriptionTable:
 *
 * A table of 0 or more font descriptions.
 *
 */
typedef struct {
	char *directory;
	char *fonts_dir_file;
	char *fonts_alias_file;
	char *fonts_scale_file;
	GList *descriptions;
} FontDescriptionTable;

static gboolean                string_is_valid                          (const char                  *string);
static char *                  file_as_string                           (const char                  *file_name);
static gboolean                directory_contains_file                  (const char                  *directory,
									 const char                  *file_name);
static FontDescription *       font_description_new                     (const char                  *font_file_name,
									 NautilusFontType             font_type,
									 const char                  *xlfd_string);
static void                    font_description_free                    (FontDescription             *description);
static gboolean                font_description_table_for_each          (const FontDescriptionTable  *description_table,
									 NautilusFontManagerCallback  callback,
									 gpointer                     callback_data);
static char                   *font_description_get_file_name           (const FontDescription       *description);
static char                   *font_description_get_foundry             (const FontDescription       *description);
static char                   *font_description_get_family              (const FontDescription       *description);
static char                   *font_description_get_weight              (const FontDescription       *description);
static char                   *font_description_get_slant               (const FontDescription       *description);
static char                   *font_description_get_set_width           (const FontDescription       *description);
static char                   *font_description_get_char_set_registry   (const FontDescription       *description);
static char                   *font_description_get_char_set_encoding   (const FontDescription       *description);
static FontDescriptionTable *  font_description_table_new               (const char                  *font_directory,
									 const GList                 *postscript_font_list,
									 const GList                 *true_type_font_list);
static void                    font_description_table_add               (FontDescriptionTable        *description_table,
									 const char                  *line,
									 const GList                 *postscript_font_list,
									 const GList                 *true_type_font_list);
static NautilusFontType        font_get_font_type                       (const char                  *font_file_name,
									 const GList                 *postscript_font_list,
									 const GList                 *true_type_font_list);
static guint                   font_description_table_get_length        (const FontDescriptionTable  *description_table);
static const FontDescription * font_description_table_peek_nth          (const FontDescriptionTable  *description_table,
									 guint                        n);
static char *                  font_description_table_get_nth_file_name (const FontDescriptionTable  *table,
									 guint                        n);
static void                    font_description_table_free              (FontDescriptionTable        *table);
static void                    font_description_table_clear             (FontDescriptionTable        *table);
static const FontDescription * font_description_table_find              (const FontDescriptionTable  *description_table,
									 const char                  *file_name);

static gboolean
string_is_valid (const char *string)
{
	return string && string[0] != '\0';
}

static char *
file_as_string (const char *file_name)
{
	struct stat stat_info;
	FILE *stream;
	char *result;
	size_t num_read;

	g_return_val_if_fail (file_name != NULL, NULL);
	g_return_val_if_fail (g_file_exists (file_name), NULL);

	if (stat (file_name, &stat_info) != 0) {
		return NULL;
	}

	if (stat_info.st_size == 0) {
		return NULL;
	}
	
	stream = fopen (file_name, "r");
	
	if (!stream) {
		return NULL;
	}

	result = g_malloc (sizeof (char) * stat_info.st_size + 1);

	num_read = fread (result, sizeof (char), stat_info.st_size, stream);

	fclose (stream);

	if ((ssize_t)num_read != stat_info.st_size) {
		g_free (result);
		return NULL;
	}

	result[stat_info.st_size] = '\0';

	return result;
}

static FontDescription *
font_description_new (const char *font_file_name,
		      NautilusFontType font_type,
		      const char *xlfd_string)
{
	FontDescription *font_description = NULL;
	NautilusStringList *tokenized_xlfd;

	g_return_val_if_fail (string_is_valid (font_file_name), NULL);
	g_return_val_if_fail (string_is_valid (xlfd_string), NULL);
	g_return_val_if_fail (font_type == NAUTILUS_FONT_POSTSCRIPT
			      || font_type == NAUTILUS_FONT_TRUE_TYPE, NULL);

	tokenized_xlfd = nautilus_string_list_new_from_tokens (xlfd_string, "-", FALSE);

	if (nautilus_string_list_get_length (tokenized_xlfd) == (XLFD_INDEX_MAX + 1)) {
		font_description = g_new0 (FontDescription, 1);
 		font_description->file_name = g_strdup (font_file_name);
 		font_description->font_type = font_type;
		font_description->foundry = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_FOUNDRY);
		font_description->family = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_FAMILY);
		font_description->weight = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_WEIGHT);
		font_description->slant = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_SLANT);
		font_description->set_width = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_SET_WIDTH);
		font_description->char_set_registry = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_CHAR_SET_REGISTRY);
		font_description->char_set_encoding = nautilus_string_list_nth (tokenized_xlfd, XLFD_INDEX_CHAR_SET_ENCODING);
	} else {
		g_warning ("'%s' is not a valid XLFD string", xlfd_string);
	}

	nautilus_string_list_free (tokenized_xlfd);

	return font_description;
}

static void
font_description_free (FontDescription *font_description)
{
	g_return_if_fail (font_description != NULL);

	g_free (font_description->file_name);
	g_free (font_description->foundry);
	g_free (font_description->family);
	g_free (font_description->weight);
	g_free (font_description->slant);
	g_free (font_description->set_width);
	g_free (font_description->char_set_registry);
	g_free (font_description->char_set_encoding);
	g_free (font_description);
}

static char *
font_description_get_file_name (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->file_name);
}

static char *
font_description_get_foundry (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->foundry);
}

static char *
font_description_get_family (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->family);
}

static char *
font_description_get_weight (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->weight);
}

static char *
font_description_get_slant (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->slant);
}

static char *
font_description_get_set_width (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->set_width);
}

static char *
font_description_get_char_set_registry (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->char_set_registry);
}

static char *
font_description_get_char_set_encoding (const FontDescription *description)
{
	g_return_val_if_fail (description != NULL, NULL);

	return g_strdup (description->char_set_encoding);
}

static guint
font_lists_total_num_fonts (const GList *postscript_font_list,
			    const GList *true_type_font_list)
{
	return g_list_length ((GList *) postscript_font_list)
		+ g_list_length ((GList *) true_type_font_list);
}
	
static void
font_description_table_add (FontDescriptionTable *table,
			    const char *line,
			    const GList *postscript_font_list,
			    const GList *true_type_font_list)
{
	char *font_file_name = NULL;
	FontDescription *description;
	char *xlfd_delimeter;
	char *font_file_full_path;
	NautilusFontType font_type;
	
	g_return_if_fail (table != NULL);
	g_return_if_fail (string_is_valid (line));
	g_return_if_fail (font_lists_total_num_fonts (postscript_font_list, true_type_font_list) > 0);

	xlfd_delimeter = strstr (line, " ");

	if (xlfd_delimeter == NULL) {
		g_warning ("'%s' is not a valid font description line", line);
		return;
	}

	font_file_name = g_strndup (line, xlfd_delimeter - line);

	while (isspace ((guchar) *xlfd_delimeter)) {
		xlfd_delimeter++;
	}

	font_file_full_path = nautilus_make_path (table->directory, font_file_name);
	font_type = font_get_font_type (font_file_full_path,
					postscript_font_list,
					true_type_font_list);

	if (font_type != NAUTILUS_FONT_UNDEFINED) {
		description = font_description_new (font_file_full_path, font_type, xlfd_delimeter);
		if (description != NULL) {
			table->descriptions = g_list_append (table->descriptions, description);
		}
	}

	g_free (font_file_full_path);
	g_free (font_file_name);
}

static NautilusFontType
font_get_font_type (const char *font_file_name,
		    const GList *postscript_font_list,
		    const GList *true_type_font_list)
{
	const GList *node;

	g_return_val_if_fail (string_is_valid (font_file_name), NAUTILUS_FONT_UNDEFINED);
	g_return_val_if_fail (font_lists_total_num_fonts (postscript_font_list, true_type_font_list) > 0,
			      NAUTILUS_FONT_UNDEFINED);

	node = postscript_font_list;
	while (node != NULL) {
		if (nautilus_istr_is_equal (node->data, font_file_name)) {
			return NAUTILUS_FONT_POSTSCRIPT;
		}
		node = node->next;
	}

	node = true_type_font_list;
	while (node != NULL) {
		if (nautilus_istr_is_equal (node->data, font_file_name)) {
			return NAUTILUS_FONT_TRUE_TYPE;
		}
		node = node->next;
	}

	return NAUTILUS_FONT_UNDEFINED;
}

static guint
font_description_table_get_length (const FontDescriptionTable *table)
{
	g_return_val_if_fail (table != NULL, 0);
	
	return g_list_length (table->descriptions);
}

static const FontDescription *
font_description_table_peek_nth (const FontDescriptionTable *table,
				 guint n)
{
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (n < font_description_table_get_length (table), NULL);

	return g_list_nth_data (table->descriptions, n);
}

static char *
font_description_table_get_nth_file_name (const FontDescriptionTable *table,
					  guint n)
{
	const FontDescription *description;
	
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (n < font_description_table_get_length (table), NULL);

	description = font_description_table_peek_nth (table, n);
	return g_strdup (description->file_name);
}

static void
font_description_table_free (FontDescriptionTable *table)
{
	g_return_if_fail (table != NULL);

	font_description_table_clear (table);
	g_free (table);
}

static void
font_description_table_clear (FontDescriptionTable *table)
{
	GList *node;
		
	g_return_if_fail (table != NULL);

	node = table->descriptions;
	while (node != NULL) {
		font_description_free (node->data);
		node = node->next;
	}

	g_list_free (table->descriptions);
	table->descriptions = NULL;

	g_free (table->directory);
	table->directory = NULL;

	g_free (table->fonts_dir_file);
	table->fonts_dir_file = NULL;

	g_free (table->fonts_alias_file);
	table->fonts_alias_file = NULL;

	g_free (table->fonts_scale_file);
	table->fonts_scale_file = NULL;
}

static const FontDescription *
font_description_table_find (const FontDescriptionTable *table,
			     const char *file_name)
{
	GList *node;
	const FontDescription *description;
	
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (nautilus_strlen (file_name) > 0, NULL);
	
	for (node = table->descriptions; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		description = node->data;

		if (nautilus_str_is_equal (file_name, description->file_name)) {
			return description;
		}
	}

	return NULL;
}

static gboolean
font_description_table_for_each (const FontDescriptionTable *table,
				 NautilusFontManagerCallback callback,
				 gpointer callback_data)
{
	GList *node;
	const FontDescription *description;
	gboolean cont = TRUE;
		
	g_return_val_if_fail (table != NULL, TRUE);
	g_return_val_if_fail (callback != NULL, TRUE);

	node = table->descriptions;
	while (node != NULL) {
		g_assert (node->data != NULL);
		description = node->data;
		
		cont = (* callback) (description->file_name,
				     description->font_type,
				     description->foundry,
				     description->family,
				     description->weight,
				     description->slant,
				     description->set_width,
				     description->char_set_registry,
				     description->char_set_encoding,
				     callback_data);
		
		node = cont ? node->next : NULL;
	}

	return cont;
}

static FontDescriptionTable *
font_description_table_new (const char *font_directory,
			    const GList *postscript_font_list,
			    const GList *true_type_font_list)
{
	FontDescriptionTable *table;
	char *description_file;
	char *description_contents;
	NautilusStringList *tokenized_contents;
	int i;
	int count;
	char *line;

	g_return_val_if_fail (string_is_valid (font_directory), NULL);
	g_return_val_if_fail (g_file_test (font_directory, G_FILE_TEST_ISDIR), NULL);

	description_file = nautilus_make_path (font_directory, FONTS_DIR_FILE_NAME);
	description_contents = file_as_string (description_file);
	
	if (description_contents == NULL) {
		g_free (description_file);
		return NULL;
	}

	tokenized_contents = nautilus_string_list_new_from_tokens (description_contents, "\n", FALSE);

	/* Make sure there is at least one description.  Item 0 is the count */
	if (nautilus_string_list_get_length (tokenized_contents) <= 1) {
		g_free (description_file);
		g_free (description_contents);
		nautilus_string_list_free (tokenized_contents);
		return NULL;
	}

	/* Find out how many font entries are described in this file */
	if (!nautilus_string_list_nth_as_integer (tokenized_contents, 0, &count)) {
		g_free (description_file);
		g_free (description_contents);
		nautilus_string_list_free (tokenized_contents);
		return NULL;
	}
	    
	/* Create a new table */
	table = g_new0 (FontDescriptionTable, 1);

	/* Assign the directory and description file */
	table->directory = g_strdup (font_directory);
	table->fonts_dir_file = description_file;

	/* Iterate throught the description file contents */
	for (i = 0; i < count; i++) {
		line = nautilus_string_list_nth (tokenized_contents, i + 1);
		if (line != NULL) {
			font_description_table_add (table,
						    line,
						    postscript_font_list,
						    true_type_font_list);
		}
		g_free (line);
	}
	nautilus_string_list_free (tokenized_contents);

	/* Assign the alias file if found */
	if (directory_contains_file (font_directory, FONTS_ALIAS_FILE_NAME)) {
		table->fonts_alias_file = nautilus_make_path (font_directory, FONTS_ALIAS_FILE_NAME);
	}
	
	/* Assign the alias scale if found */
	if (directory_contains_file (font_directory, FONTS_SCALE_FILE_NAME)) {
		table->fonts_scale_file = nautilus_make_path (font_directory, FONTS_SCALE_FILE_NAME);
	}

	g_free (description_contents);


	return table;
}

static GnomeVFSResult
collect_fonts_from_directory (const char *font_directory,
			      GList **postscript_font_list,
			      GList **true_type_font_list)
{
	GnomeVFSDirectoryHandle *directory;
	GnomeVFSResult result;
	GnomeVFSFileInfo *info;
	char *directory_uri;

	g_return_val_if_fail (string_is_valid (font_directory), GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (postscript_font_list != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (true_type_font_list != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	
	directory_uri = g_strconcat ("file://", font_directory, NULL);

	*postscript_font_list = NULL;
	*true_type_font_list = NULL;
	
	result = gnome_vfs_directory_open (&directory,
					   directory_uri,
					   GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
					   NULL);
	g_free (directory_uri);
	
	if (result != GNOME_VFS_OK) {
		return result;
	}
	
	
	while (TRUE) {
		info = gnome_vfs_file_info_new ();
		result = gnome_vfs_directory_read_next (directory, info);
		if (result == GNOME_VFS_OK) {
			if (strcasecmp (info->mime_type, POSTSCRIPT_FONT_MIME_TYPE) == 0) {
				*postscript_font_list = g_list_prepend (*postscript_font_list,
					g_strconcat (font_directory,
						"/", info->name, NULL));
			} else if (strcasecmp (info->mime_type, TRUE_TYPE_FONT_MIME_TYPE) == 0) {
				*true_type_font_list = g_list_prepend (*true_type_font_list,
					g_strconcat (font_directory,
						"/", info->name, NULL));
			}
		}
		gnome_vfs_file_info_unref (info);
		
		if (result == GNOME_VFS_ERROR_EOF) {
			break;
		}
	}
	
	gnome_vfs_directory_close (directory);
	
	return GNOME_VFS_OK;
}


static void
chop_off_comments (char *line)
{

	/* Terminates a string right at the start of a comment, stripping it from the
	 * string.
	 */
	gboolean saw_escape;
	char *scanner;

	saw_escape = FALSE;
	for (scanner = line; *scanner != '\0'; scanner++) {
		if (saw_escape) {
			saw_escape = FALSE;
			continue;
		}		
		if (*scanner == '\\') {
	
	
			saw_escape = TRUE;
			continue;
		}
		if (*scanner == '#') {
			*scanner = '\0';
			break;
		}
	}
}

static void
next_token (const char *buffer, int from, int *token_start, int *token_end)
{
	gboolean saw_escape;
	const char *scanner;

	g_assert ((int) strlen (buffer) >= from);

	*token_start = -1;
	*token_end = -1;
	
	/* strip white space */
	saw_escape = FALSE;
	for (scanner = buffer + from; *scanner != '\0'; scanner++) {
		if (saw_escape) {
			saw_escape = FALSE;
			continue;
		}		
		if (*scanner == '\\') {
			saw_escape = TRUE;
			continue;
		}
		if (!isspace ((guchar) *scanner) && *scanner != '\n') {
			*token_start = scanner - buffer;
			break;
		}
	}	

	if (*scanner == ',') {
		*token_end = *token_start + 1;
		return;
	}
	
	/* go until token end */
	saw_escape = FALSE;
	for (; *scanner != '\0'; scanner++) {
		if (saw_escape) {
			saw_escape = FALSE;
			continue;
		}		
		if (*scanner == '\\') {
			saw_escape = TRUE;
			continue;
		}
		if (isspace ((guchar) *scanner) || *scanner == ',') {
			break;
		}
	}	
	
	if (*token_start >= 0) {
		*token_end = scanner - buffer;
	}
}

#define READ_BUFFER_SIZE 2048

static gboolean
token_matches (const char *buffer, int start, int end, const char *pattern)
{
	if (start < 0) {
		return FALSE;
	}

	return strncmp (buffer + start, pattern, end - start) == 0;
}

typedef enum {
	EXPECT_CATALOGUE,
	EXPECT_ASSIGNMENT,
	EXPECT_DIRECTORY,
	EXPECT_COMMA
} FontConfigParseState;

static void
font_server_for_each_font_directory_internal (void (* callback) (const char *font_directory, gpointer callback_data),
					      gpointer callback_data,
					      char *buffer,
					      FILE *file)
{
	FontConfigParseState state;
	int token_start, token_end;
	char *font_directory;

	g_return_if_fail (callback != NULL);
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (file != NULL);

	state = EXPECT_CATALOGUE;
	while (TRUE) {
		fgets (buffer, READ_BUFFER_SIZE, file);
		if (strlen (buffer) == 0) {
			if (state != EXPECT_COMMA) {
				g_warning ("unexpected file end.");
			}
			break;
		}
		
		chop_off_comments (buffer);
		
		token_start = 0;
		while (TRUE) {
			next_token (buffer, token_start, &token_start, &token_end);
	
			if (token_start < 0) {
				break;
			}

			switch (state) {
			case EXPECT_CATALOGUE:
				if (token_matches(buffer, token_start, token_end, "catalogue")) {
					state = EXPECT_ASSIGNMENT;
				}
				break;
				
			case EXPECT_ASSIGNMENT:
				if (!token_matches(buffer, token_start, token_end, "=")) {
					g_warning (" expected token \"=\" .");
					return;
				}
				state = EXPECT_DIRECTORY;
				break;
				
			case EXPECT_DIRECTORY:
				if (token_matches(buffer, token_start, token_end, ",")) {
					g_warning (" expected directory name.");
					return;
				}
				/* found a directory, call an each function on it */
				font_directory = g_strndup (buffer + token_start, token_end - token_start);
				(* callback) (font_directory, callback_data);
				g_free (font_directory);
				state = EXPECT_COMMA;
				break;

			case EXPECT_COMMA:
				if (!token_matches(buffer, token_start, token_end, ",")) {
					/* we are done, no more directories */
					return;
				}
				state = EXPECT_DIRECTORY;
				break;				
			}
			
			token_start = token_end;
		}
	}
}

static void
font_server_for_each_font_directory (const char *font_config_file_path,
				     void (* callback) (const char *font_directory, gpointer callback_data),
				     gpointer callback_data)
{
	/* scan the font config file, finding all the font directory paths */
	FILE *font_config_file;
	char *buffer;

	g_return_if_fail (string_is_valid (font_config_file_path));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (callback_data != NULL);
	
	font_config_file = fopen (font_config_file_path, "r");

	if (font_config_file == NULL) {
		return;
	}
	
	buffer = g_malloc (READ_BUFFER_SIZE);
	font_server_for_each_font_directory_internal (callback,
						      callback_data,
						      buffer,
						      font_config_file);
	
	g_free (buffer);
	fclose (font_config_file);
}

static gboolean
directory_contains_file (const char *directory,
			 const char *file_name)
{
	gboolean result;
	char *path;

	g_return_val_if_fail (string_is_valid (directory), FALSE);
	g_return_val_if_fail (string_is_valid (file_name), FALSE);

	path = nautilus_make_path (directory, file_name);
	result = g_file_exists (path);
	g_free (path);

	return result;
}

/* Iterating directories is slow cause of all the mime sniffing that
 * has to happen on each potential scalalble font.  By Ignoring
 * directories that arent interesting, we make things much faster.
 */
static gboolean
font_ignore_directory (const char *font_directory)
{
	guint i;

	static const char *ignored_font_dir_suffices[] = {
		"unscaled",
		"100dpi",
		"75dpi",
		"misc",
		"abisource/fonts",
		"fonts/Speedo",
		"fonts/cyrillic"
	};

	g_return_val_if_fail (string_is_valid (font_directory), TRUE);

	for (i = 0; i < NAUTILUS_N_ELEMENTS (ignored_font_dir_suffices); i++) {
		if (nautilus_str_has_suffix (font_directory, ignored_font_dir_suffices[i])) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
font_manager_collect_font_tables (const char *font_directory,
				  GList **collected_font_tables)
{
	GList *postscript_font_list = NULL;
	GList *true_type_font_list = NULL;
	FontDescriptionTable *table;

	g_return_if_fail (string_is_valid (font_directory));
	g_return_if_fail (collected_font_tables != NULL);

	if (font_ignore_directory (font_directory)) {
		return;
	}
	
	/* Collect postscript and true type font in this directory */
	collect_fonts_from_directory (font_directory, &postscript_font_list, &true_type_font_list);

	/* No scalable fonts found; we're done */
	if (g_list_length (postscript_font_list) == 0 
	    && g_list_length (true_type_font_list) == 0) {
		return;
	}

	/* If no "fonts.dir" exists, then the user has a missing description file (broken setup) */
	if (!directory_contains_file (font_directory, FONTS_DIR_FILE_NAME)) {
		nautilus_g_list_free_deep (postscript_font_list);
		nautilus_g_list_free_deep (true_type_font_list);
		g_warning ("Direcotry '%s' contains scalable fonts but no '%s' description file.",
			   font_directory,
			   FONTS_DIR_FILE_NAME);
		return;
	}
	
	table = font_description_table_new (font_directory, postscript_font_list, true_type_font_list);
	if (table == NULL) {
		nautilus_g_list_free_deep (postscript_font_list);
		nautilus_g_list_free_deep (true_type_font_list);
		g_warning ("Error trying to process font directory '%s'.", font_directory);
		return;
	}

	*collected_font_tables = g_list_append (*collected_font_tables, table);

	nautilus_g_list_free_deep (postscript_font_list);
	nautilus_g_list_free_deep (true_type_font_list);
}

static void
font_server_for_each_callback (const char *font_directory,
			       gpointer callback_data)
{
	g_return_if_fail (string_is_valid (font_directory));
	g_return_if_fail (callback_data != NULL);

	font_manager_collect_font_tables (font_directory, callback_data);
}

static GList *global_font_table = NULL;

static void
font_table_list_free (GList *font_table_list)
{
	GList *node;

	node = font_table_list;
	while (node != NULL) {
		g_assert (node->data != NULL);
		font_description_table_free (node->data);
		node = node->next;
	}
	g_list_free (font_table_list);
}

static const FontDescription *
font_table_list_find (const GList *font_table_list,
		      const char *file_name)
{
	const GList *node;
	const FontDescription *description;

	g_return_val_if_fail (file_name != NULL, NULL);

	for (node = font_table_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		description = font_description_table_find (node->data, file_name);
		if (description != NULL) {
			return description;
		}
	}

	return NULL;
}

static void
free_font_tables (void)
{
	font_table_list_free (global_font_table);
	global_font_table = NULL;
}

static void
ensure_global_font_table (void)
{
	char *user_directory;
	char *user_font_dir;

	if (global_font_table != NULL) {
		return;
	}

	/* Populate the default font table if needed */
	font_manager_collect_font_tables (DEFAULT_FONT_DIRECTORY, &global_font_table);

	/* Populate the user font table if needed */
	user_directory = nautilus_get_user_directory ();
	user_font_dir = nautilus_make_path (user_directory, USER_FONT_DIRECTORY_NAME);
	if (g_file_test (user_font_dir, G_FILE_TEST_ISDIR)) {
		font_manager_collect_font_tables (user_font_dir, &global_font_table);
	}
	g_free (user_directory);
	g_free (user_font_dir);

	/* Populate the system font table if needed - using the font server's configuration */
	if (g_file_exists (FONT_SERVER_CONFIG_FILE)) {
		font_server_for_each_font_directory (FONT_SERVER_CONFIG_FILE,
						     font_server_for_each_callback,
						     &global_font_table);
	}
	
	g_atexit (free_font_tables);
}

/* Public */

/**
 * nautilus_font_manager_for_each_font:
 * @callback: A callback to be called for each scalable font in the system.
 * @callback_data: User's data.
 *
 * Iterate all the scalable fonts available in the system.  The available
 * fonts are the sum of:
 *
 * 1) Fallback fonts installed by Nautilus
 * 2) User fonts found in ~/.nautilus/fonts
 * 3) Fonts listed in the font servers config file (/etc/X11/fs/config)
 *
 */
void
nautilus_font_manager_for_each_font (NautilusFontManagerCallback callback,
				     gpointer callback_data)
{
	GList *node;
	gboolean cont = TRUE;

 	g_return_if_fail (callback != NULL);

	/* Ensure that all the font tables exist */
	ensure_global_font_table ();

	for (node = global_font_table; node != NULL && cont; node = node->next) {
		g_assert (node->data != NULL);
		cont = font_description_table_for_each (node->data, callback, callback_data);
	}
}

char *
nautilus_font_manager_get_default_font (void)
{
	guint i;

	static const char *default_fonts[] = {
		DEFAULT_FONT_DIRECTORY "/n019003l.pfb",
		SOURCE_DATADIR "/fonts/urw/n019003l.pfb",
		"/usr/share/fonts/default/Type1/n019003l.pfb",
		"/usr/X11R6/lib/X11/fonts/Type1/lcdxsr.pfa"
	};

	for (i = 0; i < NAUTILUS_N_ELEMENTS (default_fonts); i++) {
		if (g_file_exists (default_fonts[i])) {
			return g_strdup (default_fonts[i]);
		}
	}

	return NULL;
}

char *
nautilus_font_manager_get_default_bold_font (void)
{
	guint i;

	static const char *default_bold_fonts[] = {
		DEFAULT_FONT_DIRECTORY "/n019004l.pfb",
		SOURCE_DATADIR "/fonts/urw/n019004l.pfb",
	};

	for (i = 0; i < NAUTILUS_N_ELEMENTS (default_bold_fonts); i++) {
		if (g_file_exists (default_bold_fonts[i])) {
			return g_strdup (default_bold_fonts[i]);
		}
	}

	return NULL;
}

gboolean
nautilus_font_manager_file_is_scalable_font (const char *file_name)
{
	gboolean is_scalable_font = FALSE;
	char *uri;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	
	g_return_val_if_fail (nautilus_strlen (file_name) > 0, FALSE);

	uri = g_strconcat ("file://", file_name, NULL);

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE);

	if (result == GNOME_VFS_OK) {
		is_scalable_font = 
			nautilus_istr_is_equal (info->mime_type, POSTSCRIPT_FONT_MIME_TYPE)
			|| nautilus_istr_is_equal (info->mime_type, TRUE_TYPE_FONT_MIME_TYPE);
	}

	gnome_vfs_file_info_unref (info);
	g_free (uri);

	return is_scalable_font;
}

typedef struct
{
	const FontDescription *description;
	char *found_file_name;
} FindData;

static gboolean
font_list_find_bold_callback (const char *font_file_name,
			      NautilusFontType font_type,
			      const char *foundry,
			      const char *family,
			      const char *weight,
			      const char *slant,
			      const char *set_width,
			      const char *char_set_registry,
			      const char *char_set_encoding,
			      gpointer callback_data)
{
	FindData *data;

	g_return_val_if_fail (font_file_name != NULL, FALSE);
	g_return_val_if_fail (foundry != NULL, FALSE);
	g_return_val_if_fail (family != NULL, FALSE);
	g_return_val_if_fail (weight != NULL, FALSE);
	g_return_val_if_fail (slant != NULL, FALSE);
	g_return_val_if_fail (set_width != NULL, FALSE);
	g_return_val_if_fail (char_set_registry != NULL, FALSE);
	g_return_val_if_fail (char_set_encoding != NULL, FALSE);
	g_return_val_if_fail (callback_data != NULL, FALSE);

	data = callback_data;

	g_return_val_if_fail (data->description != NULL, FALSE);
	g_return_val_if_fail (data->found_file_name == NULL, FALSE);
	
	if (nautilus_istr_is_equal (data->description->foundry, foundry)
	    && nautilus_istr_is_equal (data->description->family, family)
	    && nautilus_istr_is_equal (data->description->slant, slant)
	    && nautilus_istr_is_equal (data->description->set_width, set_width)
	    && nautilus_istr_is_equal (data->description->char_set_registry, char_set_registry)
	    && nautilus_istr_is_equal (data->description->char_set_encoding, char_set_encoding)
	    && nautilus_font_manager_weight_is_bold (weight)) {
		data->found_file_name = g_strdup (font_file_name);
	}

	return (data->found_file_name == NULL);
}

char *
nautilus_font_manager_get_bold (const char *plain_font)
{
	FindData data;

	g_return_val_if_fail (nautilus_strlen (plain_font) > 0, NULL);
	g_return_val_if_fail (nautilus_font_manager_file_is_scalable_font (plain_font), NULL);

	ensure_global_font_table ();

 	data.description = font_table_list_find (global_font_table, plain_font);

 	if (data.description == NULL) {
 		return g_strdup (plain_font);
 	}
	
 	data.found_file_name = NULL;
 	nautilus_font_manager_for_each_font (font_list_find_bold_callback, &data);
	
 	if (data.found_file_name != NULL) {
 		return data.found_file_name;
 	}
	
	return g_strdup (plain_font);
}

gboolean
nautilus_font_manager_weight_is_bold (const char *weight)
{
	g_return_val_if_fail (weight != NULL, FALSE);

	return (nautilus_istr_is_equal (weight, "bold")
		|| nautilus_istr_is_equal (weight, "demibold")
		|| nautilus_istr_is_equal (weight, "black"));
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static char *
call_chop_off_comments (const char *input)
{
	char *test_copy;
	test_copy = g_strdup (input);
	chop_off_comments (test_copy);
	return test_copy;
}

#define TEST_FONT_DIR "/usr/share/fonts/default/Type1"

void
nautilus_self_check_font_manager (void)
{
	FontDescriptionTable *table;
	const FontDescription *description;
	GList *font_table_list = NULL;

	/* chop_off_comments() */
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("foo bar"), "foo bar");
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("foo bar\n"), "foo bar\n");
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("#foo bar"), "");
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("foo bar#"), "foo bar");
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("\\foo bar"), "\\foo bar");
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("\\#foo bar"), "\\#foo bar");
	NAUTILUS_CHECK_STRING_RESULT (call_chop_off_comments ("\\##foo bar"), "\\#");

	if (!g_file_exists (TEST_FONT_DIR)) {
		return;
	}

	font_manager_collect_font_tables (TEST_FONT_DIR, &font_table_list);
	g_return_if_fail (font_table_list != NULL);

	g_return_if_fail (g_list_nth_data (font_table_list, 0) != NULL);
	table = g_list_nth_data (font_table_list, 0);

 	NAUTILUS_CHECK_INTEGER_RESULT (font_description_table_get_length (table), 35);
 	NAUTILUS_CHECK_STRING_RESULT (font_description_table_get_nth_file_name (table, 0), TEST_FONT_DIR "/a010013l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_table_get_nth_file_name (table, 1), TEST_FONT_DIR "/a010015l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_table_get_nth_file_name (table, 2), TEST_FONT_DIR "/a010033l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_table_get_nth_file_name (table, 3), TEST_FONT_DIR "/a010035l.pfb");

	description = font_description_table_peek_nth (table, 0);
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_file_name (description), TEST_FONT_DIR "/a010013l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_foundry (description), "URW");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_family (description), "Avantgarde");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_weight (description), "book");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_slant (description), "r");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_set_width (description), "normal");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_encoding (description), "1");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_registry (description), "iso8859");

	description = font_description_table_peek_nth (table, 1);
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_file_name (description), TEST_FONT_DIR "/a010015l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_foundry (description), "URW");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_family (description), "Avantgarde");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_weight (description), "demibold");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_slant (description), "r");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_set_width (description), "normal");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_encoding (description), "1");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_registry (description), "iso8859");

	description = font_description_table_peek_nth (table, 2);
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_file_name (description), TEST_FONT_DIR "/a010033l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_foundry (description), "URW");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_family (description), "Avantgarde");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_weight (description), "book");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_slant (description), "o");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_set_width (description), "normal");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_encoding (description), "1");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_registry (description), "iso8859");

	description = font_description_table_peek_nth (table, 3);
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_file_name (description), TEST_FONT_DIR "/a010035l.pfb");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_foundry (description), "URW");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_family (description), "Avantgarde");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_weight (description), "demibold");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_slant (description), "o");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_set_width (description), "normal");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_encoding (description), "1");
 	NAUTILUS_CHECK_STRING_RESULT (font_description_get_char_set_registry (description), "iso8859");

	font_table_list_free (font_table_list);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
