/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-metafile.c: Nautilus directory model.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-directory-metafile.h"

#include <stdlib.h>
#include <xmlmemory.h>
#include "nautilus-string.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-directory-private.h"

#define METAFILE_XML_VERSION "1.0"

typedef struct {
	char *file_name;
	char *key;
	char *subkey;
} MetadataKey;

typedef struct {
	gboolean is_list;
	union {
		char *string;
		GList *string_list;
	} value;
	char *default_value;
} MetadataValue;

#if 0
static MetadataValue     *get_metadata                        (NautilusDirectory   *directory,
							       const MetadataKey   *key);
#endif
static char *             get_metadata_from_node              (xmlNode             *node,
							       const char          *key,
							       const char          *default_metadata);
static GList *            get_metadata_list_from_node         (xmlNode             *node,
							       const char          *list_key,
							       const char          *list_subkey);
static gboolean           set_metadata_eat_parameters         (NautilusDirectory   *directory,
							       MetadataKey         *key,
							       MetadataValue       *value);
static gboolean           set_metadata_in_metafile            (NautilusDirectory   *directory,
							       const MetadataKey   *key,
							       const MetadataValue *value);

static char *
get_metadata_from_node (xmlNode *node,
			const char *key,
			const char *default_metadata)
{
	xmlChar *property;
	char *result;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (key[0] != '\0', NULL);

	property = xmlGetProp (node, key);
	if (property == NULL) {
		result = g_strdup (default_metadata);
	} else {
		result = g_strdup (property);
	}
	xmlFree (property);

	return result;
}


static GList *
get_metadata_list_from_node (xmlNode *node,
			     const char *list_key,
			     const char *list_subkey)
{
	g_return_val_if_fail (list_key != NULL, NULL);
	g_return_val_if_fail (list_key[0] != '\0', NULL);
	g_return_val_if_fail (list_subkey != NULL, NULL);
	g_return_val_if_fail (list_subkey[0] != '\0', NULL);

	return nautilus_xml_get_property_for_children
		(node, list_key, list_subkey);
}

static xmlNode *
create_metafile_root (NautilusDirectory *directory)
{
	xmlNode *root;

	if (directory->details->metafile == NULL) {
		directory->details->metafile = xmlNewDoc (METAFILE_XML_VERSION);
	}
	root = xmlDocGetRootElement (directory->details->metafile);
	if (root == NULL) {
		root = xmlNewDocNode (directory->details->metafile, NULL, "DIRECTORY", NULL);
		xmlDocSetRootElement (directory->details->metafile, root);
	}

	return root;
}

char *
nautilus_directory_get_metadata (NautilusDirectory *directory,
				 const char *key,
				 const char *default_metadata)
{
	/* It's legal to call this on a NULL directory. */
	if (directory == NULL) {
		return g_strdup (default_metadata);
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	/* The root itself represents the directory. */
	return get_metadata_from_node
		(xmlDocGetRootElement (directory->details->metafile),
		 key, default_metadata);
}

GList *
nautilus_directory_get_metadata_list (NautilusDirectory *directory,
				      const char *list_key,
				      const char *list_subkey)
{
	/* It's legal to call this on a NULL directory. */
	if (directory == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	/* The root itself represents the directory. */
	return get_metadata_list_from_node
		(xmlDocGetRootElement (directory->details->metafile),
		 list_key, list_subkey);
}

gboolean 
nautilus_directory_get_boolean_metadata (NautilusDirectory *directory,
					 const char *key,
					 gboolean default_metadata)
{
	char *result_as_string;
	gboolean result;

	result_as_string = nautilus_directory_get_metadata
		(directory, key,
		 default_metadata ? "true" : "false");
	
	g_strdown (result_as_string);
	if (strcmp (result_as_string, "true") == 0) {
		result = TRUE;
	} else if (strcmp (result_as_string, "false") == 0) {
		result = FALSE;
	} else {
		if (result_as_string != NULL) {
			g_warning ("boolean metadata with value other than true or false");
		}
		result = default_metadata;
	}

	g_free (result_as_string);
	return result;
}

void               
nautilus_directory_set_boolean_metadata (NautilusDirectory *directory,
					 const char *key,
					 gboolean default_metadata,
					 gboolean metadata)
{
	nautilus_directory_set_metadata
		(directory, key,
		 default_metadata ? "true" : "false",
		 metadata ? "true" : "false");
}

int 
nautilus_directory_get_integer_metadata (NautilusDirectory *directory,
					 const char *key,
					 int default_metadata)
{
	char *result_as_string;
	char *default_as_string;
	int result;

	default_as_string = g_strdup_printf ("%d", default_metadata);
	result_as_string = nautilus_directory_get_metadata
		(directory, key, default_as_string);
	
	/* Handle oddball case of non-existent directory */
	if (result_as_string == NULL) {
		result = default_metadata;
	} else {
		result = atoi (result_as_string);
		g_free (result_as_string);
	}

	g_free (default_as_string);
	return result;

}

void               
nautilus_directory_set_integer_metadata (NautilusDirectory *directory,
					 const char *key,
					 int default_metadata,
					 int metadata)
{
	char *value_as_string;
	char *default_as_string;

	value_as_string = g_strdup_printf ("%d", metadata);
	default_as_string = g_strdup_printf ("%d", default_metadata);

	nautilus_directory_set_metadata
		(directory, key,
		 default_as_string, value_as_string);

	g_free (value_as_string);
	g_free (default_as_string);
}

xmlNode *
nautilus_directory_get_file_metadata_node (NautilusDirectory *directory,
					   const char *file_name,
					   gboolean create)
{
	xmlNode *root, *child;
	
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	
	/* The root itself represents the directory.
	 * The children represent the files.
	 * FIXME bugzilla.eazel.com 650: 
	 * This linear search may not be fast enough.
	 * Eventually, we could have a pointer from the NautilusFile right to
	 * the corresponding XML node, or maybe we won't have the XML tree
	 * in memory at all.
	 */
	child = nautilus_xml_get_root_child_by_name_and_property
		(directory->details->metafile,
		 "FILE", METADATA_NODE_NAME_FOR_FILE_NAME, file_name);
	if (child != NULL) {
		return child;
	}
	
	/* Create if necessary. */
	if (create) {
		root = create_metafile_root (directory);
		child = xmlNewChild (root, NULL, "FILE", NULL);
		xmlSetProp (child, METADATA_NODE_NAME_FOR_FILE_NAME, file_name);
		return child;
	}
	
	return NULL;
}

char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata)
{
	return get_metadata_from_node
		(nautilus_directory_get_file_metadata_node (directory, file_name, FALSE),
		 key, default_metadata);
}


GList *
nautilus_directory_get_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey)
{
	return get_metadata_list_from_node
		(nautilus_directory_get_file_metadata_node (directory, file_name, FALSE),
		 list_key, list_subkey);
}

static gboolean
real_set_metadata (NautilusDirectory *directory,
		   const char *file_name,
		   const char *key,
		   const char *default_metadata,
		   const char *metadata)
{
	char *old_metadata;
	gboolean old_metadata_matches;
	const char *value;
	xmlNode *node;
	xmlAttr *property_node;

	/* If the data in the metafile is already correct, do nothing. */
	if (file_name == NULL) {
		old_metadata = nautilus_directory_get_metadata
			(directory, key, default_metadata);		
	} else {
		old_metadata = nautilus_directory_get_file_metadata
			(directory, file_name, key, default_metadata);
	}
	old_metadata_matches = nautilus_strcmp (old_metadata, metadata) == 0;
	g_free (old_metadata);
	if (old_metadata_matches) {
		return FALSE;
	}

	/* Data that matches the default is represented in the tree by
	   the lack of an attribute.
	*/
	if (nautilus_strcmp (default_metadata, metadata) == 0) {
		value = NULL;
	} else {
		value = metadata;
	}

	/* Get or create the node. */
	if (file_name == NULL) {
		node = create_metafile_root (directory);
	} else {
		node = nautilus_directory_get_file_metadata_node
			(directory, file_name, value != NULL);
	}

	/* Add or remove a property node. */
	if (node != NULL) {
		property_node = xmlSetProp (node, key, value);
		if (value == NULL) {
			xmlRemoveProp (property_node);
		}
	}
	
	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);
	return TRUE;
}

static gboolean
real_set_metadata_list (NautilusDirectory *directory,
			const char *file_name,
			const char *list_key,
			const char *list_subkey,
			GList *list)
{
	xmlNode *node, *child, *next;
	gboolean changed;
	GList *p;
	xmlChar *property;

	/* Get or create the node. */
	if (file_name == NULL) {
		node = create_metafile_root (directory);
	} else {
		node = nautilus_directory_get_file_metadata_node
			(directory, file_name, list != NULL);
	}

	/* Work with the list. */
	changed = FALSE;
	if (node == NULL) {
		g_assert (list == NULL);
	} else {
		p = list;

		/* Remove any nodes except the ones we expect. */
		for (child = nautilus_xml_get_children (node);
		     child != NULL;
		     child = next) {

			next = child->next;
			if (strcmp (child->name, list_key) == 0) {
				property = xmlGetProp (child, list_subkey);
				if (property != NULL && p != NULL
				    && strcmp (property, (char *) p->data) == 0) {
					p = p->next;
				} else {
					xmlUnlinkNode (child);
					xmlFreeNode (child);
					changed = TRUE;
				}
				xmlFree (property);
			}
		}
		
		/* Add any additional nodes needed. */
		for (; p != NULL; p = p->next) {
			child = xmlNewChild (node, NULL, list_key, NULL);
			xmlSetProp (child, list_subkey, p->data);
			changed = TRUE;
		}
	}

	if (!changed) {
		return FALSE;
	}

	nautilus_directory_request_write_metafile (directory);
	return TRUE;
}

static MetadataKey *
metadata_key_new (const char *file_name,
		  const char *key_string,
		  const char *subkey)
{
	MetadataKey *key;

	key = g_new0 (MetadataKey, 1);
	key->file_name = g_strdup (file_name);
	key->key = g_strdup (key_string);
	key->subkey = g_strdup (subkey);

	return key;
}

static void
metadata_key_destroy (MetadataKey *key)
{
	if (key == NULL) {
		return;
	}

	g_free (key->file_name);
	g_free (key->key);
	g_free (key->subkey);
	g_free (key);
}

static guint
metadata_key_hash (gconstpointer key_pointer)
{
	const MetadataKey *key;
	guint hash;

	key = key_pointer;
	hash = 0;

	if (key->file_name != NULL) {
		hash = g_str_hash (key->file_name);
	}

	hash <<= 4;
	hash ^= g_str_hash (key->key);

	if (key->subkey != NULL) {
		hash <<= 4;
		hash ^= g_str_hash (key->subkey);
	}

	return hash;
}

static gboolean
metadata_key_hash_equal (gconstpointer key_pointer_a,
			 gconstpointer key_pointer_b)
{
	const MetadataKey *key_a, *key_b;

	key_a = key_pointer_a;
	key_b = key_pointer_b;

	return nautilus_strcmp (key_a->file_name, key_b->file_name) == 0
		&& nautilus_strcmp (key_a->key, key_b->key) == 0
		&& nautilus_strcmp (key_a->subkey, key_b->subkey) == 0;
}

static MetadataValue *
metadata_value_new (const char *default_metadata, const char *metadata)
{
	MetadataValue *value;

	value = g_new0 (MetadataValue, 1);

	value->default_value = g_strdup (default_metadata);
	value->value.string = g_strdup (metadata);

	return value;
}

static MetadataValue *
metadata_value_new_list (GList *metadata)
{
	MetadataValue *value;

	value = g_new0 (MetadataValue, 1);

	value->is_list = TRUE;
	value->value.string_list = nautilus_g_str_list_copy (metadata);

	return value;
}

static void
metadata_value_destroy (MetadataValue *value)
{
	if (value == NULL) {
		return;
	}

	if (!value->is_list) {
		g_free (value->value.string);
	} else {
		nautilus_g_list_free_deep (value->value.string_list);
	}
	g_free (value->default_value);
	g_free (value);
}

static gboolean
metadata_value_equal (const MetadataValue *value_a,
		      const MetadataValue *value_b)
{
	if (value_a->is_list != value_b->is_list) {
		return FALSE;
	}

	if (!value_a->is_list) {
		return nautilus_strcmp (value_a->value.string,
					value_b->value.string) == 0
			&& nautilus_strcmp (value_a->default_value,
					    value_b->default_value) == 0;
	} else {
		g_assert (value_a->default_value == NULL);
		g_assert (value_b->default_value == NULL);

		return nautilus_g_str_list_equal
			(value_a->value.string_list,
			 value_b->value.string_list);
	}
}

static gboolean
set_metadata_in_metafile (NautilusDirectory *directory,
			  const MetadataKey *key,
			  const MetadataValue *value)
{
	gboolean changed;

	if (!value->is_list) {
		g_assert (key->subkey == NULL);
		changed = real_set_metadata
			(directory,
			 key->file_name,
			 key->key,
			 value->default_value,
			 value->value.string);
	} else {
		g_assert (value->default_value == NULL);
		changed = real_set_metadata_list
			(directory,
			 key->file_name,
			 key->key,
			 key->subkey,
			 value->value.string_list);
	}

	return changed;
}

static gboolean
set_metadata_eat_parameters (NautilusDirectory *directory,
			     MetadataKey *key,
			     MetadataValue *value)
{
	gboolean changed;
	MetadataKey *destroy_key;
	MetadataValue *destroy_value;
	gpointer old_key, old_value;

	destroy_key = key;
	destroy_value = value;

	if (!directory->details->metafile_read) {
		/* Create hash table only when we need it.
		 * We'll destroy it when we finish reading the metafile.
		 */
		if (directory->details->metadata_changes == NULL) {
			directory->details->metadata_changes =
				g_hash_table_new (metadata_key_hash,
						  metadata_key_hash_equal);
		}

		/* Put the change into the hash.
		 * Delete the old change.
		 */
		if (g_hash_table_lookup_extended (directory->details->metadata_changes,
						  key,
						  &old_key,
						  &old_value)) {
			changed = !metadata_value_equal (old_value, value);
			if (changed) {
				destroy_key = old_key;
				destroy_value = old_value;
			}
		} else {
			changed = TRUE;
			destroy_key = NULL;
			destroy_value = NULL;
		}

		if (changed) {
			g_hash_table_insert (directory->details->metadata_changes,
					     key, value);
		}
	} else {
		changed = set_metadata_in_metafile (directory, key, value);
	}

	metadata_key_destroy (destroy_key);
	metadata_value_destroy (destroy_value);

	return changed;
}

static void
free_metadata_changes_hash_table_entry (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (user_data == NULL);
	metadata_key_destroy (key);
	metadata_value_destroy (value);
}

static void
destroy_metadata_changes_hash_table (GHashTable *table)
{
	if (table == NULL) {
		return;
	}

	g_hash_table_foreach (table,
			      free_metadata_changes_hash_table_entry,
			      NULL);
	g_hash_table_destroy (table);
}

void
nautilus_directory_metafile_destroy (NautilusDirectory *directory)
{
	xmlFreeDoc (directory->details->metafile);
	destroy_metadata_changes_hash_table (directory->details->metadata_changes);
}

void
nautilus_directory_set_metadata (NautilusDirectory *directory,
				 const char *key_string,
				 const char *default_metadata,
				 const char *metadata)
{
	MetadataKey *key;
	MetadataValue *value;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (key_string != NULL);
	g_return_if_fail (key_string[0] != '\0');

	key = metadata_key_new (NULL, key_string, NULL);
	value = metadata_value_new (default_metadata, metadata);
	if (set_metadata_eat_parameters (directory, key, value)) {
		nautilus_directory_emit_metadata_changed (directory);
	}
}

gboolean
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key_string,
				      const char *default_metadata,
				      const char *metadata)
{
	MetadataKey *key;
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (file_name[0] != '\0', FALSE);
	g_return_val_if_fail (key_string != NULL, FALSE);
	g_return_val_if_fail (key_string[0] != '\0', FALSE);

	key = metadata_key_new (file_name, key_string, NULL);
	value = metadata_value_new (default_metadata, metadata);
	return set_metadata_eat_parameters (directory, key, value);
}

gboolean
nautilus_directory_set_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey,
					   GList *list)
{
	MetadataKey *key;
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (file_name[0] != '\0', FALSE);
	g_return_val_if_fail (list_key != NULL, FALSE);
	g_return_val_if_fail (list_key[0] != '\0', FALSE);
	g_return_val_if_fail (list_subkey != NULL, FALSE);
	g_return_val_if_fail (list_subkey[0] != '\0', FALSE);

	key = metadata_key_new (file_name, list_key, list_subkey);
	value = metadata_value_new_list (list);
	return set_metadata_eat_parameters (directory, key, value);
}
