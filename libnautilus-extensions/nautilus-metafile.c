/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-metafile.c - server side of Nautilus::Metafile
 *
 * Copyright (C) 2001 Eazel, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "nautilus-metafile.h"
#include "nautilus-metafile-server.h"

#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-directory.h>

#include "nautilus-string.h"
#include "nautilus-metadata.h"
#include "nautilus-thumbnails.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-directory-private.h"

#include <stdlib.h>
#include <gnome-xml/xmlmemory.h>

#define METAFILE_XML_VERSION "1.0"

struct NautilusMetafileDetails {
	NautilusDirectory *directory;
};

static void nautilus_metafile_initialize       (NautilusMetafile      *metafile);
static void nautilus_metafile_initialize_class (NautilusMetafileClass *klass);

static void destroy (GtkObject *metafile);

static CORBA_boolean corba_is_read (PortableServer_Servant  servant,
				    CORBA_Environment      *ev);

static CORBA_char *corba_get		      (PortableServer_Servant  servant,
					       const CORBA_char       *file_name,
					       const CORBA_char       *key,
					       const CORBA_char       *default_value,
					       CORBA_Environment      *ev);
static Nautilus_MetadataList *corba_get_list (PortableServer_Servant  servant,
					       const CORBA_char      *file_name,
					       const CORBA_char      *list_key,
					       const CORBA_char      *list_subkey,
					       CORBA_Environment     *ev);

static void corba_set      (PortableServer_Servant  servant,
			    const CORBA_char       *file_name,
			    const CORBA_char       *key,
			    const CORBA_char       *default_value,
			    const CORBA_char       *metadata,
			    CORBA_Environment      *ev);
static void corba_set_list (PortableServer_Servant       servant,
			    const CORBA_char            *file_name,
			    const CORBA_char            *list_key,
			    const CORBA_char            *list_subkey,
			    const Nautilus_MetadataList *list,
			    CORBA_Environment           *ev);
					       
static void corba_copy  (PortableServer_Servant   servant,
			 const CORBA_char        *source_file_name,
			 const Nautilus_URI       destination_directory_uri,
			 const CORBA_char        *destination_file_name,
			 CORBA_Environment       *ev);
static void corba_remove (PortableServer_Servant  servant,
			 const CORBA_char        *file_name,
			 CORBA_Environment       *ev);
static void corba_rename (PortableServer_Servant  servant,
			 const CORBA_char        *old_file_name,
			 const CORBA_char        *new_file_name,
			 CORBA_Environment       *ev);

static void corba_register_monitor   (PortableServer_Servant          servant,
				      const Nautilus_MetafileMonitor  monitor,
				      CORBA_Environment              *ev);
static void corba_unregister_monitor (PortableServer_Servant          servant,
				      const Nautilus_MetafileMonitor  monitor,
				      CORBA_Environment              *ev);

static char    *get_file_metadata      (NautilusDirectory *directory,
					const char *file_name,
					const char *key,
					const char *default_metadata);
static GList   *get_file_metadata_list (NautilusDirectory *directory,
					const char *file_name,
					const char *list_key,
					const char *list_subkey);
static gboolean set_file_metadata      (NautilusDirectory *directory,
					const char *file_name,
					const char *key,
					const char *default_metadata,
					const char *metadata);
static gboolean set_file_metadata_list (NautilusDirectory *directory,
					const char *file_name,
					const char *list_key,
					const char *list_subkey,
					GList *list);
					
static void rename_file_metadata (NautilusDirectory *directory,
				  const char *old_file_name,
				  const char *new_file_name);
static void copy_file_metadata   (NautilusDirectory *source_directory,
				  const char *source_file_name,
				  NautilusDirectory *destination_directory,
				  const char *destination_file_name);
static void remove_file_metadata (NautilusDirectory *directory,
				  const char *file_name);

static void call_metatfile_changed_for_one_file (NautilusDirectory *directory,
						 const CORBA_char  *file_name);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetafile, nautilus_metafile, BONOBO_OBJECT_TYPE)

static void
nautilus_metafile_initialize_class (NautilusMetafileClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
}

static POA_Nautilus_Metafile__epv *
nautilus_metafile_get_epv (void)
{
	static POA_Nautilus_Metafile__epv epv;
	
	epv.is_read            = corba_is_read;
	epv.get                = corba_get;
	epv.get_list           = corba_get_list;
	epv.set                = corba_set;
	epv.set_list           = corba_set_list;
	epv.copy               = corba_copy;
	epv.remove             = corba_remove;
	epv.rename             = corba_rename;
	epv.register_monitor   = corba_register_monitor;
	epv.unregister_monitor = corba_unregister_monitor;

	return &epv;
}

static POA_Nautilus_Metafile__vepv *
nautilus_metafile_get_vepv (void)
{
	static POA_Nautilus_Metafile__vepv vepv;

	vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();
	vepv.Nautilus_Metafile_epv = nautilus_metafile_get_epv ();

	return &vepv;
}

static POA_Nautilus_Metafile *
nautilus_metafile_create_servant (void)
{
	POA_Nautilus_Metafile *servant;
	CORBA_Environment ev;

	servant = (POA_Nautilus_Metafile *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = nautilus_metafile_get_vepv ();
	CORBA_exception_init (&ev);
	POA_Nautilus_Metafile__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_error ("can't initialize Nautilus metafile");
	}
	CORBA_exception_free (&ev);

	return servant;
}

static void
nautilus_metafile_initialize (NautilusMetafile *metafile)
{
	Nautilus_Metafile corba_metafile;

	metafile->details = g_new0 (NautilusMetafileDetails, 1);

	corba_metafile = bonobo_object_activate_servant
		(BONOBO_OBJECT (metafile), nautilus_metafile_create_servant ());
	bonobo_object_construct (BONOBO_OBJECT (metafile), corba_metafile);
}

static void
destroy (GtkObject *object)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	metafile  = NAUTILUS_METAFILE (object);
	directory = metafile->details->directory;
	
	nautilus_directory_unref (directory);
	g_free (metafile->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusMetafile *
nautilus_metafile_new (const char *directory_uri)
{
	NautilusMetafile *metafile;
	metafile = NAUTILUS_METAFILE (gtk_object_new (NAUTILUS_TYPE_METAFILE, NULL));
	metafile->details->directory = nautilus_directory_get (directory_uri);
	return metafile;
}

static CORBA_boolean
corba_is_read (PortableServer_Servant  servant,
	       CORBA_Environment      *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	return directory->details->metafile_read ? CORBA_TRUE : CORBA_FALSE;
}

static CORBA_char *
corba_get (PortableServer_Servant  servant,
	   const CORBA_char       *file_name,
	   const CORBA_char       *key,
	   const CORBA_char       *default_value,
	   CORBA_Environment      *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	char       *metadata;
	CORBA_char *result;

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	metadata = get_file_metadata (directory, file_name, key, default_value);

	result = CORBA_string_dup (metadata != NULL ? metadata : "");

	g_free (metadata);
	
	return result;
}

static Nautilus_MetadataList *
corba_get_list (PortableServer_Servant  servant,
	        const CORBA_char       *file_name,
	        const CORBA_char       *list_key,
	        const CORBA_char       *list_subkey,
	        CORBA_Environment      *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	GList *metadata_list;
	Nautilus_MetadataList *result;
	int	len;
	int	buf_pos;
	GList   *list_ptr;

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	metadata_list = get_file_metadata_list (directory, file_name, list_key, list_subkey);

	len = g_list_length (metadata_list);
	result = Nautilus_MetadataList__alloc ();
	result->_maximum = len;
	result->_length  = len;
	result->_buffer  = CORBA_sequence_CORBA_string_allocbuf (len);

	/* We allocate our buffer with CORBA calls, so the caller will clean it
	 * all up if we set release to TRUE.
	 */
	CORBA_sequence_set_release (result, CORBA_TRUE);

	buf_pos  = 0;
	list_ptr = metadata_list;
	while (list_ptr != NULL) {
		result->_buffer [buf_pos] = CORBA_string_dup (list_ptr->data);
		list_ptr = g_list_next (list_ptr);
		++buf_pos;
	}

	nautilus_g_list_free_deep (metadata_list);

	return result;
}

static void
corba_set (PortableServer_Servant  servant,
	   const CORBA_char       *file_name,
	   const CORBA_char       *key,
	   const CORBA_char       *default_value,
	   const CORBA_char       *metadata,
	   CORBA_Environment      *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	if (nautilus_str_is_empty (default_value)) {
		default_value = NULL;
	}
	if (nautilus_str_is_empty (metadata)) {
		metadata = NULL;
	}

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	if (set_file_metadata (directory, file_name, key, default_value, metadata)) {
		call_metatfile_changed_for_one_file (directory, file_name);
	}
}

static void
corba_set_list (PortableServer_Servant      servant,
		const CORBA_char            *file_name,
		const CORBA_char            *list_key,
		const CORBA_char            *list_subkey,
		const Nautilus_MetadataList *list,
		CORBA_Environment           *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	GList               *metadata_list;
	CORBA_unsigned_long  buf_pos;

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	metadata_list = NULL;
	for (buf_pos = 0; buf_pos < list->_length; ++buf_pos) {
		metadata_list = g_list_prepend (metadata_list, list->_buffer [buf_pos]);
	}
	metadata_list = g_list_reverse (metadata_list);
	
	if (set_file_metadata_list (directory, file_name, list_key, list_subkey, metadata_list)) {
		call_metatfile_changed_for_one_file (directory, file_name);
	}
	
	g_list_free (metadata_list);
}
					       
static void
corba_copy (PortableServer_Servant   servant,
	    const CORBA_char        *source_file_name,
	    const Nautilus_URI       destination_directory_uri,
	    const CORBA_char        *destination_file_name,
	    CORBA_Environment       *ev)
{
	NautilusMetafile  *source_metafile;
	NautilusDirectory *source_directory;
	NautilusDirectory *destination_directory;

	source_metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	source_directory = source_metafile->details->directory;
	
	destination_directory = nautilus_directory_get (destination_directory_uri);

	copy_file_metadata (source_directory, source_file_name,
			    destination_directory, destination_file_name);
			    
	nautilus_directory_unref (destination_directory);
}

static void
corba_remove (PortableServer_Servant  servant,
	      const CORBA_char       *file_name,
	      CORBA_Environment      *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	metafile =  NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;
	
	remove_file_metadata (directory, file_name);
}

static void
corba_rename (PortableServer_Servant  servant,
	      const CORBA_char       *old_file_name,
	      const CORBA_char       *new_file_name,
	      CORBA_Environment      *ev)
{
	NautilusMetafile  *metafile;
	NautilusDirectory *directory;

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	rename_file_metadata (directory, old_file_name, new_file_name);
}

typedef struct {
	GList             *monitors;
	NautilusDirectory *directory;
} DirectoryMonitorListEntry;

static GHashTable *directory_monitor_lists;

static DirectoryMonitorListEntry *
get_or_add_directory_monitor_list_entry (NautilusDirectory *directory)
{
	DirectoryMonitorListEntry *entry;
	
	if (directory_monitor_lists == NULL) {
		directory_monitor_lists = nautilus_g_hash_table_new_free_at_exit (g_direct_hash, g_direct_equal, __FILE__ ": metadata monitors");
	}

	entry = g_hash_table_lookup (directory_monitor_lists, directory);

	if (entry == NULL) {
		entry = g_new0 (DirectoryMonitorListEntry, 1);
		nautilus_directory_ref (directory);
		entry->directory = directory;
		g_hash_table_insert (directory_monitor_lists, directory, entry);
	}

	return entry;
}

static void
remove_directory_monitor_list_entry (NautilusDirectory *directory)
{
	DirectoryMonitorListEntry *entry;
	
	entry = g_hash_table_lookup (directory_monitor_lists, directory);

	if (entry != NULL) {
		/* This fn only handles removal when there are no monitors left.
		 * It makes no attempt to free the monitors.
		 */
		g_return_if_fail (entry->monitors == NULL);
		
		g_hash_table_remove (directory_monitor_lists, directory);
		nautilus_directory_unref (directory);
		g_free (entry);
	}
}

static GList *
find_monitor_node (GList *monitors, const Nautilus_MetafileMonitor monitor)
{
	GList                    *node;
	CORBA_Environment	  ev;
	Nautilus_MetafileMonitor  cur_monitor;		

	CORBA_exception_init (&ev);

	for (node = monitors; node != NULL; node = node->next) {
		cur_monitor = node->data;
		if (CORBA_Object_is_equivalent (cur_monitor, monitor, &ev)) {
			break;
		}
	}
	
	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	CORBA_exception_free (&ev);
	
	return node;
}

static void
corba_register_monitor (PortableServer_Servant          servant,
			const Nautilus_MetafileMonitor  monitor,
			CORBA_Environment              *ev)
{
	NautilusMetafile          *metafile;
	NautilusDirectory         *directory;
	DirectoryMonitorListEntry *monitor_list;
	
	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;

	monitor_list = get_or_add_directory_monitor_list_entry (directory);

	g_return_if_fail (find_monitor_node (monitor_list->monitors, monitor) == NULL);

	monitor_list->monitors = g_list_prepend (monitor_list->monitors, (gpointer) CORBA_Object_duplicate (monitor, ev));	

	/* cause metafile to be read */
	directory->details->load_metafile_for_server = TRUE;
	nautilus_directory_async_state_changed (directory);
}

static void
corba_unregister_monitor (PortableServer_Servant          servant,
			  const Nautilus_MetafileMonitor  monitor,
			  CORBA_Environment              *ev)
{
	NautilusMetafile          *metafile;
	NautilusDirectory         *directory;
	DirectoryMonitorListEntry *entry;
	GList                     *node;

	metafile  = NAUTILUS_METAFILE (bonobo_object_from_servant (servant));
	directory = metafile->details->directory;
	
	entry = g_hash_table_lookup (directory_monitor_lists, directory);

	g_return_if_fail (entry != NULL);

	node = find_monitor_node (entry->monitors, monitor);

	g_return_if_fail (node != NULL);

	entry->monitors = g_list_remove_link (entry->monitors, node);

	CORBA_Object_release (node->data, ev);
	g_list_free_1 (node);

	if (entry->monitors == NULL) {
		remove_directory_monitor_list_entry (directory);
	}
}

static void
call_metatfile_changed (NautilusDirectory *directory,
	                const Nautilus_FileNameList  *file_names)
{
	GList                     *node;
	CORBA_Environment          ev;
	Nautilus_MetafileMonitor   monitor;		
	DirectoryMonitorListEntry *entry;

	entry = g_hash_table_lookup (directory_monitor_lists, directory);
	
	if (entry != NULL) {
		CORBA_exception_init (&ev);
		
		for (node = entry->monitors; node != NULL; node = node->next) {
			monitor = node->data;
			Nautilus_MetafileMonitor_metafile_changed (monitor, file_names, &ev);
			/* FIXME bugzilla.eazel.com 6664: examine ev for errors */
		}
		
		CORBA_exception_free (&ev);
	}
}

static void
file_list_filler_ghfunc (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
	Nautilus_FileNameList *file_names;

	file_names = user_data;

	file_names->_buffer [file_names->_length] = key;
	
	++file_names->_length;
}

void
call_metafile_changed_for_all_files_mentioned_in_metafile (NautilusDirectory *directory)
{
	CORBA_unsigned_long   len;
	Nautilus_FileNameList file_names;

	len = g_hash_table_size (directory->details->metafile_node_hash);

	if (len > 0) {
		file_names._maximum =  len;
		file_names._length  =  0;
		file_names._buffer  =  g_new (CORBA_char *, len);

		g_hash_table_foreach (directory->details->metafile_node_hash,
				      file_list_filler_ghfunc,
				      &file_names);

		call_metatfile_changed (directory, &file_names);

		g_free (file_names._buffer);
	}
}

static void
call_metatfile_changed_for_one_file (NautilusDirectory *directory,
				     const CORBA_char  *file_name)
{
	Nautilus_FileNameList file_names;
	
	file_names._maximum =  1;
	file_names._length  =  1;
	file_names._buffer  =  (CORBA_char **) &file_name;

	call_metatfile_changed (directory, &file_names);
}

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
		nautilus_metafile_set_metafile_contents (directory, xmlNewDoc (METAFILE_XML_VERSION));
	}
	root = xmlDocGetRootElement (directory->details->metafile);
	if (root == NULL) {
		root = xmlNewDocNode (directory->details->metafile, NULL, "directory", NULL);
		xmlDocSetRootElement (directory->details->metafile, root);
	}

	return root;
}

static xmlNode *
get_file_node (NautilusDirectory *directory,
	       const char *file_name,
	       gboolean create)
{
	GHashTable *hash;
	xmlNode *root, *node;
	
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	hash = directory->details->metafile_node_hash;
	node = g_hash_table_lookup (hash, file_name);
	if (node != NULL) {
		return node;
	}
	
	if (create) {
		root = create_metafile_root (directory);
		node = xmlNewChild (root, NULL, "file", NULL);
		xmlSetProp (node, "name", file_name);
		g_hash_table_insert (hash, xmlMemStrdup (file_name), node);
		return node;
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

	node = get_file_node (directory, file_name, FALSE);
	return get_metadata_from_node (node, key, default_metadata);
}

static GList *
get_metadata_list_from_metafile (NautilusDirectory *directory,
				 const char *file_name,
				 const char *list_key,
				 const char *list_subkey)
{
	xmlNode *node;

	node = get_file_node (directory, file_name, FALSE);
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
	old_metadata = get_file_metadata
		(directory, file_name, key, default_metadata);

	old_metadata_matches = nautilus_strcmp (old_metadata, metadata) == 0;
	g_free (old_metadata);
	if (old_metadata_matches) {
		return FALSE;
	}

	/* Data that matches the default is represented in the tree by
	 * the lack of an attribute.
	 */
	if (nautilus_strcmp (default_metadata, metadata) == 0) {
		value = NULL;
	} else {
		value = metadata;
	}

	/* Get or create the node. */
	node = get_file_node (directory, file_name, value != NULL);

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
	node = get_file_node (directory, file_name, list != NULL);

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

static void
destroy_xml_string_key (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (key != NULL);
	g_assert (user_data == NULL);
	g_assert (value != NULL);

	xmlFree (key);
}

void
nautilus_metafile_destroy (NautilusDirectory *directory)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	g_hash_table_foreach (directory->details->metafile_node_hash,
			      destroy_xml_string_key, NULL);
	xmlFreeDoc (directory->details->metafile);
	destroy_metadata_changes_hash_table (directory->details->metadata_changes);
}

static char *
get_file_metadata (NautilusDirectory *directory,
		   const char *file_name,
		   const char *key,
		   const char *default_metadata)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (key), NULL);

	if (directory->details->metafile_read) {
		return get_metadata_string_from_metafile
			(directory, file_name, key, default_metadata);
	} else {
		return get_metadata_string_from_table
			(directory, file_name, key, default_metadata);
	}
}

static GList *
get_file_metadata_list (NautilusDirectory *directory,
			const char *file_name,
			const char *list_key,
			const char *list_subkey)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (list_key), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (list_subkey), NULL);

	if (directory->details->metafile_read) {
		return get_metadata_list_from_metafile
			(directory, file_name, list_key, list_subkey);
	} else {
		return get_metadata_list_from_table
			(directory, file_name, list_key, list_subkey);
	}
}

static gboolean
set_file_metadata (NautilusDirectory *directory,
		   const char *file_name,
		   const char *key,
		   const char *default_metadata,
		   const char *metadata)
{
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (key), FALSE);

	if (directory->details->metafile_read) {
		return set_metadata_string_in_metafile (directory, file_name, key,
							default_metadata, metadata);
	} else {
		value = metadata_value_new (default_metadata, metadata);
		return set_metadata_eat_value (directory, file_name,
					       key, NULL, value);
	}
}

static gboolean
set_file_metadata_list (NautilusDirectory *directory,
			const char *file_name,
			const char *list_key,
			const char *list_subkey,
			GList *list)
{
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (list_key), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (list_subkey), FALSE);

	if (directory->details->metafile_read) {
		return set_metadata_list_in_metafile (directory, file_name,
						      list_key, list_subkey, list);
	} else {
		value = metadata_value_new_list (list);
		return set_metadata_eat_value (directory, file_name,
					       list_key, list_subkey, value);
	}
}

static void
rename_file_metadata (NautilusDirectory *directory,
		      const char *old_file_name,
		      const char *new_file_name)
{
	gboolean found;
	gpointer key, value;
	xmlNode *file_node;
	GHashTable *hash;
	char *old_file_uri, *new_file_uri;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (old_file_name != NULL);
	g_return_if_fail (new_file_name != NULL);

	remove_file_metadata (directory, new_file_name);

	if (directory->details->metafile_read) {
		/* Move data in XML document if present. */
		hash = directory->details->metafile_node_hash;
		found = g_hash_table_lookup_extended
			(hash, old_file_name, &key, &value);
		if (found) {
			g_assert (strcmp ((const char *) key, old_file_name) == 0);
			file_node = value;
			g_hash_table_remove (hash,
					     old_file_name);
			xmlFree (key);
			g_hash_table_insert (hash,
					     xmlMemStrdup (new_file_name), value);
			xmlSetProp (file_node, "name", new_file_name);
			nautilus_directory_request_write_metafile (directory);
		}
	} else {
		/* Move data in hash table. */
		/* FIXME: If there's data for this file in the
		 * metafile on disk, this doesn't arrange for that
		 * data to be moved to the new name.
		 */
		hash = directory->details->metadata_changes;
		found = g_hash_table_lookup_extended
			(hash, old_file_name, &key, &value);
		if (found) {
			g_hash_table_remove (hash, old_file_name);
			g_free (key);
			g_hash_table_insert (hash, g_strdup (new_file_name), value);
		}
	}

	/* Rename the thumbnails for the file, if any. */
	old_file_uri = nautilus_directory_get_file_uri (directory, old_file_name);
	new_file_uri = nautilus_directory_get_file_uri (directory, new_file_name);
	nautilus_update_thumbnail_file_renamed (old_file_uri, new_file_uri);
	g_free (old_file_uri);
	g_free (new_file_uri);
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
apply_file_changes (NautilusDirectory *directory,
		    const char *file_name,
		    GHashTable *changes)
{
	ChangeContext context;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (file_name != NULL);
	g_assert (changes != NULL);

	context.directory = directory;
	context.file_name = file_name;

	g_hash_table_foreach (changes, apply_one_change, &context);
}

static void
apply_one_file_changes (gpointer key, gpointer value, gpointer callback_data)
{
	apply_file_changes (callback_data, key, value);
	g_hash_table_destroy (value);
}

void
nautilus_metafile_apply_pending_changes (NautilusDirectory *directory)
{
	if (directory->details->metadata_changes == NULL) {
		return;
	}
	g_hash_table_foreach (directory->details->metadata_changes,
			      apply_one_file_changes, directory);
	g_hash_table_destroy (directory->details->metadata_changes);
	directory->details->metadata_changes = NULL;
}

static void
copy_file_metadata (NautilusDirectory *source_directory,
				       const char *source_file_name,
				       NautilusDirectory *destination_directory,
				       const char *destination_file_name)
{
	xmlNodePtr source_node, node, root;
	GHashTable *hash, *changes;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (source_directory));
	g_return_if_fail (source_file_name != NULL);
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (destination_directory));
	g_return_if_fail (destination_file_name != NULL);

	/* FIXME bugzilla.eazel.com 3343: This does not properly
	 * handle the case where we don't have the source metadata yet
	 * since it's not read in.
	 */

	remove_file_metadata
		(destination_directory, destination_file_name);
	g_assert (get_file_node (destination_directory, destination_file_name, FALSE) == NULL);

	source_node = get_file_node (source_directory, source_file_name, FALSE);
	if (source_node != NULL) {
		if (destination_directory->details->metafile_read) {
			node = xmlCopyNode (source_node, TRUE);
			root = create_metafile_root (destination_directory);
			xmlAddChild (root, node);
			xmlSetProp (node, "name", destination_file_name);
			g_hash_table_insert (destination_directory->details->metafile_node_hash,
					     xmlMemStrdup (destination_file_name), node);
		} else {
			/* FIXME bugzilla.eazel.com 6526: Copying data into a destination
			 * where the metafile was not yet read is not implemented.
			 */
			g_warning ("not copying metadata");
		}
	}

	hash = source_directory->details->metadata_changes;
	if (hash != NULL) {
		changes = g_hash_table_lookup (hash, source_file_name);
		if (changes != NULL) {
			apply_file_changes (destination_directory,
					    destination_file_name,
					    changes);
		}
	}

	/* FIXME: Do we want to copy the thumbnail here like in the
	 * rename and remove cases?
	 */
}

static void
remove_file_metadata (NautilusDirectory *directory,
					 const char *file_name)
{
	gboolean found;
	gpointer key, value;
	xmlNode *file_node;
	GHashTable *hash;
	char *file_uri;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (file_name != NULL);

	if (directory->details->metafile_read) {
		/* Remove data in XML document if present. */
		hash = directory->details->metafile_node_hash;
		found = g_hash_table_lookup_extended
			(hash, file_name, &key, &value);
		if (found) {
			g_assert (strcmp ((const char *) key, file_name) == 0);
			file_node = value;
			g_hash_table_remove (hash,
					     file_name);
			xmlFree (key);
			nautilus_xml_remove_node (file_node);
			xmlFreeNode (file_node);
			nautilus_directory_request_write_metafile (directory);
		}
	} else {
		/* Remove data from hash table. */
		/* FIXME: If there's data for this file on the
		 * metafile on disk, this does not arrange for it to
		 * be removed when the metafile is later read.
		 */
		hash = directory->details->metadata_changes;
		if (hash != NULL) {
			found = g_hash_table_lookup_extended
				(hash, file_name, &key, &value);
			if (found) {
				g_hash_table_remove (hash, file_name);
				g_free (key);
				metadata_value_destroy (value);
			}
		}
	}

	/* Delete the thumbnails for the file, if any. */
	file_uri = nautilus_directory_get_file_uri (directory, file_name);
	nautilus_remove_thumbnail_for_file (file_uri);
	g_free (file_uri);
}

void
nautilus_metafile_set_metafile_contents (NautilusDirectory *directory,
					  xmlDocPtr metafile_contents)
{
	GHashTable *hash;
	xmlNodePtr node;
	xmlChar *name;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (directory->details->metafile == NULL);

	if (metafile_contents == NULL) {
		return;
	}

	directory->details->metafile = metafile_contents;
	
	/* Populate the node hash table. */
	hash = directory->details->metafile_node_hash;
	for (node = nautilus_xml_get_root_children (metafile_contents);
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "file") == 0) {
			name = xmlGetProp (node, "name");
			if (g_hash_table_lookup (hash, name) != NULL) {
				xmlFree (name);
				/* FIXME: Should we delete duplicate nodes as we discover them? */
			} else {
				g_hash_table_insert (hash, name, node);
			}
		}
	}
}
