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
	gboolean is_list;
	union {
		char *string;
		GList *string_list;
	} value;
	char *default_value;
} MetadataValue;

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

static xmlNode *
get_file_node (NautilusDirectory *directory,
	       const char *file_name,
	       gboolean create)
{
	xmlNode *root, *child;
	
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	
	/* The root itself represents the directory.
	 * The children represent the files.
	 * FIXME bugzilla.eazel.com 650: 
	 * This linear search may not be fast enough.
	 * Eventually, we could have a pointer from the NautilusFile right to
	 * the corresponding XML node, or maybe we won't have the XML tree
	 * in memory at all.
	 */
	child = nautilus_xml_get_root_child_by_name_and_property
		(directory->details->metafile, "FILE", "NAME", file_name);
	if (child != NULL) {
		return child;
 	}
	
	/* Create if necessary. */
	if (create) {
		root = create_metafile_root (directory);
		child = xmlNewChild (root, NULL, "FILE", NULL);
		xmlSetProp (child, "NAME", file_name);
		return child;
	}
	
	return NULL;
}

static char *
get_metadata_string_from_metafile (NautilusDirectory *directory,
				   const char *file_name,
				   const char *key,
				   const char *default_metadata)
{
	xmlNode *node;

	if (file_name == NULL) {
		node = xmlDocGetRootElement (directory->details->metafile);
	} else {
		node = get_file_node (directory, file_name, FALSE);
	}
	return get_metadata_from_node (node, key, default_metadata);
}

static GList *
get_metadata_list_from_metafile (NautilusDirectory *directory,
				 const char *file_name,
				 const char *list_key,
				 const char *list_subkey)
{
	xmlNode *node;

	if (file_name == NULL) {
		node = xmlDocGetRootElement (directory->details->metafile);
	} else {
		node = get_file_node (directory, file_name, FALSE);
	}
	return get_metadata_list_from_node (node, list_key, list_subkey);
}

static gboolean
set_metadata_string_in_metafile (NautilusDirectory *directory,
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
		node = get_file_node (directory, file_name, value != NULL);
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
set_metadata_list_in_metafile (NautilusDirectory *directory,
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
		node = get_file_node (directory, file_name, list != NULL);
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
			  const char *file_name,
			  const char *key,
			  const char *subkey,
			  const MetadataValue *value)
{
	gboolean changed;

	if (!value->is_list) {
		g_assert (subkey == NULL);
		changed = set_metadata_string_in_metafile
			(directory, file_name, key,
			 value->default_value,
			 value->value.string);
	} else {
		g_assert (value->default_value == NULL);
		changed = set_metadata_list_in_metafile
			(directory, file_name, key, subkey,
			 value->value.string_list);
	}

	return changed;
}

static char *
get_metadata_string_from_table (NautilusDirectory *directory,
				const char *file_name,
				const char *key,
				const char *default_metadata)
{
	GHashTable *directory_table, *file_table;
	MetadataValue *value;

	/* Get the value from the hash table. */
	directory_table = directory->details->metadata_changes;
        file_table = directory_table == NULL ? NULL
		: g_hash_table_lookup (directory_table, file_name);
	value = file_table == NULL ? NULL
		: g_hash_table_lookup (file_table, key);
	if (value == NULL) {
		return g_strdup (default_metadata);
	}
	
	/* Convert it to a string. */
	g_assert (!value->is_list);
	if (nautilus_strcmp (value->value.string, value->default_value) == 0) {
		return g_strdup (default_metadata);
	}
	return g_strdup (value->value.string);
}

static GList *
get_metadata_list_from_table (NautilusDirectory *directory,
			      const char *file_name,
			      const char *key,
			      const char *subkey)
{
	GHashTable *directory_table, *file_table;
	char *combined_key;
	MetadataValue *value;

	/* Get the value from the hash table. */
	directory_table = directory->details->metadata_changes;
        file_table = directory_table == NULL ? NULL
		: g_hash_table_lookup (directory_table, file_name);
	if (file_table == NULL) {
		return NULL;
	}
	combined_key = g_strconcat (key, "/", subkey, NULL);
	value = g_hash_table_lookup (file_table, combined_key);
	g_free (combined_key);
	if (value == NULL) {
		return NULL;
	}

	/* Copy the list and return it. */
	g_assert (value->is_list);
	return nautilus_g_str_list_copy (value->value.string_list);
}

static guint
str_or_null_hash (gconstpointer str)
{
	return str == NULL ? 0 : g_str_hash (str);
}

static gboolean
str_or_null_equal (gconstpointer str_a, gconstpointer str_b)
{
	if (str_a == NULL) {
		return str_b == NULL;
	}
	if (str_b == NULL) {
		return FALSE;
	}
	return g_str_equal (str_a, str_b);
}

static gboolean
set_metadata_eat_value (NautilusDirectory *directory,
			const char *file_name,
			const char *key,
			const char *subkey,
			MetadataValue *value)
{
	GHashTable *directory_table, *file_table;
	gboolean changed;
	char *combined_key;
	MetadataValue *old_value;

	if (directory->details->metafile_read) {
		changed = set_metadata_in_metafile
			(directory, file_name, key, subkey, value);
		metadata_value_destroy (value);
	} else {
		/* Create hash table only when we need it.
		 * We'll destroy it when we finish reading the metafile.
		 */
		directory_table = directory->details->metadata_changes;
		if (directory_table == NULL) {
			directory_table = g_hash_table_new
				(str_or_null_hash, str_or_null_equal);
			directory->details->metadata_changes = directory_table;
		}
		file_table = g_hash_table_lookup (directory_table, file_name);
		if (file_table == NULL) {
			file_table = g_hash_table_new (g_str_hash, g_str_equal);
			g_hash_table_insert (directory_table,
					     g_strdup (file_name), file_table);
		}

		/* Find the entry in the hash table. */
		if (subkey == NULL) {
			combined_key = g_strdup (key);
		} else {
			combined_key = g_strconcat (key, "/", subkey, NULL);
		}
		old_value = g_hash_table_lookup (file_table, combined_key);

		/* Put the change into the hash. Delete the old change. */
		changed = old_value == NULL || !metadata_value_equal (old_value, value);
		if (changed) {
			g_hash_table_insert (file_table, combined_key, value);
			if (old_value != NULL) {
				/* The hash table keeps the old key. */
				g_free (combined_key);
				metadata_value_destroy (old_value);
			}
		} else {
			g_free (combined_key);
			metadata_value_destroy (value);
		}
	}

	return changed;
}

static void
free_file_table_entry (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (user_data == NULL);

	g_free (key);
	metadata_value_destroy (value);
}

static void
free_directory_table_entry (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (user_data == NULL);
	g_assert (value != NULL);

	g_free (key);
	g_hash_table_foreach (value, free_file_table_entry, NULL);
	g_hash_table_destroy (value);
}

static void
destroy_metadata_changes_hash_table (GHashTable *directory_table)
{
	if (directory_table == NULL) {
		return;
	}
	g_hash_table_foreach (directory_table, free_directory_table_entry, NULL);
	g_hash_table_destroy (directory_table);
}

void
nautilus_directory_metafile_destroy (NautilusDirectory *directory)
{
	xmlFreeDoc (directory->details->metafile);
	destroy_metadata_changes_hash_table (directory->details->metadata_changes);
}

char *
nautilus_directory_get_metadata (NautilusDirectory *directory,
				 const char *key,
				 const char *default_metadata)
{
	g_return_val_if_fail (key != NULL, g_strdup (default_metadata));
	g_return_val_if_fail (key[0] != '\0', g_strdup (default_metadata));

	/* It's legal to call this on a NULL directory. */
	if (directory == NULL) {
		return g_strdup (default_metadata);
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), g_strdup (default_metadata));

	if (directory->details->metafile_read) {
		return get_metadata_string_from_metafile
			(directory, NULL, key, default_metadata);
	} else {
		return get_metadata_string_from_table
			(directory, NULL, key, default_metadata);
	}
}

char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (file_name[0] != '\0', FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (key[0] != '\0', FALSE);

	if (directory->details->metafile_read) {
		return get_metadata_string_from_metafile
			(directory, file_name, key, default_metadata);
	} else {
		return get_metadata_string_from_table
			(directory, file_name, key, default_metadata);
	}
}


GList *
nautilus_directory_get_metadata_list (NautilusDirectory *directory,
				      const char *list_key,
				      const char *list_subkey)
{
	g_return_val_if_fail (list_key != NULL, NULL);
	g_return_val_if_fail (list_key[0] != '\0', NULL);
	g_return_val_if_fail (list_subkey != NULL, NULL);
	g_return_val_if_fail (list_subkey[0] != '\0', NULL);

	/* It's legal to call this on a NULL directory. */
	if (directory == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	if (directory->details->metafile_read) {
		return get_metadata_list_from_metafile
			(directory, NULL, list_key, list_subkey);
	} else {
		return get_metadata_list_from_table
			(directory, NULL, list_key, list_subkey);
	}
}

GList *
nautilus_directory_get_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (file_name != NULL, NULL);
	g_return_val_if_fail (file_name[0] != '\0', NULL);
	g_return_val_if_fail (list_key != NULL, NULL);
	g_return_val_if_fail (list_key[0] != '\0', NULL);
	g_return_val_if_fail (list_subkey != NULL, NULL);
	g_return_val_if_fail (list_subkey[0] != '\0', NULL);

	if (directory->details->metafile_read) {
		return get_metadata_list_from_metafile
			(directory, file_name, list_key, list_subkey);
	} else {
		return get_metadata_list_from_table
			(directory, file_name, list_key, list_subkey);
	}
}

void
nautilus_directory_set_metadata (NautilusDirectory *directory,
				 const char *key,
				 const char *default_metadata,
				 const char *metadata)
{
	MetadataValue *value;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	if (directory->details->metafile_read) {
		if (set_metadata_string_in_metafile (directory, NULL, key,
						     default_metadata, metadata)) {
			nautilus_directory_emit_metadata_changed (directory);
		}
	} else {
		value = metadata_value_new (default_metadata, metadata);
		if (set_metadata_eat_value (directory, NULL, key, NULL, value)) {
			nautilus_directory_emit_metadata_changed (directory);
		}
	}
}

void
nautilus_directory_set_metadata_list (NautilusDirectory *directory,
				      const char *list_key,
				      const char *list_subkey,
				      GList *list)
{
	MetadataValue *value;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (list_key != NULL);
	g_return_if_fail (list_key[0] != '\0');
	g_return_if_fail (list_subkey != NULL);
	g_return_if_fail (list_subkey[0] != '\0');

	if (directory->details->metafile_read) {
		if (set_metadata_list_in_metafile (directory, NULL,
						   list_key, list_subkey, list)) {
			nautilus_directory_emit_metadata_changed (directory);
		}
	} else {
		value = metadata_value_new_list (list);
		if (set_metadata_eat_value (directory, NULL,
					    list_key, list_subkey, value)) {
			nautilus_directory_emit_metadata_changed (directory);
		}
	}
}

gboolean
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata,
				      const char *metadata)
{
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (file_name[0] != '\0', FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (key[0] != '\0', FALSE);

	if (directory->details->metafile_read) {
		return set_metadata_string_in_metafile (directory, file_name, key,
							default_metadata, metadata);
	} else {
		value = metadata_value_new (default_metadata, metadata);
		return set_metadata_eat_value (directory, file_name,
					       key, NULL, value);
	}
}

gboolean
nautilus_directory_set_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey,
					   GList *list)
{
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (file_name[0] != '\0', FALSE);
	g_return_val_if_fail (list_key != NULL, FALSE);
	g_return_val_if_fail (list_key[0] != '\0', FALSE);
	g_return_val_if_fail (list_subkey != NULL, FALSE);
	g_return_val_if_fail (list_subkey[0] != '\0', FALSE);

	if (directory->details->metafile_read) {
		return set_metadata_list_in_metafile (directory, file_name,
						      list_key, list_subkey, list);
	} else {
		value = metadata_value_new_list (list);
		return set_metadata_eat_value (directory, file_name,
					       list_key, list_subkey, value);
	}
}

void
nautilus_directory_update_file_metadata (NautilusDirectory *directory,
					 const char *old_file_name,
					 const char *new_file_name)
{
	xmlNode *file_node;
	GHashTable *directory_table;
	gboolean found;
	gpointer key, value;

	if (directory->details->metafile_read) {
		/* Move data in XML document if present. */
		file_node = get_file_node (directory, old_file_name, FALSE);
		if (file_node != NULL) {
			xmlSetProp (file_node, "NAME", new_file_name);
			nautilus_directory_request_write_metafile (directory);
		}
	} else {
		/* Move data in hash table. */
		directory_table = directory->details->metadata_changes;
		found = g_hash_table_lookup_extended
			(directory_table, old_file_name, &key, &value);
		if (found) {
			g_hash_table_remove (directory_table, old_file_name);
			g_free (key);
			g_hash_table_insert (directory_table,
					     g_strdup (new_file_name), value);
		}
	}
}

typedef struct {
	NautilusDirectory *directory;
	const char *file_name;
} ChangeContext;

static void
apply_one_change (gpointer key, gpointer value, gpointer callback_data)
{
	ChangeContext *context;
	const char *hash_table_key, *separator, *metadata_key, *subkey;
	char *key_prefix;

	g_assert (key != NULL);
	g_assert (value != NULL);
	g_assert (callback_data != NULL);

	context = callback_data;

	/* Break the key in half. */
	hash_table_key = key;
	separator = strchr (hash_table_key, '/');
	if (separator == NULL) {
		key_prefix = NULL;
		metadata_key = hash_table_key;
		subkey = NULL;
	} else {
		key_prefix = g_strndup (hash_table_key, separator - hash_table_key);
		metadata_key = key_prefix;
		subkey = separator + 1;
	}

	/* Set the metadata. */
	set_metadata_in_metafile (context->directory, context->file_name,
				  metadata_key, subkey, value);
	g_free (key_prefix);
}

static void
apply_file_changes (gpointer key, gpointer value, gpointer callback_data)
{
	ChangeContext context;

	g_assert (value != NULL);
	g_assert (NAUTILUS_IS_DIRECTORY (callback_data));

	context.directory = callback_data;
	context.file_name = key;

	g_hash_table_foreach (value, apply_one_change, &context);
	g_hash_table_destroy (value);
}

void
nautilus_directory_metafile_apply_pending_changes (NautilusDirectory *directory)
{
	if (directory->details->metadata_changes == NULL) {
		return;
	}
	g_hash_table_foreach (directory->details->metadata_changes,
			      apply_file_changes, directory);
	g_hash_table_destroy (directory->details->metadata_changes);
	directory->details->metadata_changes = NULL;
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
