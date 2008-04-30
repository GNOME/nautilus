/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-*/

/* nautilus-metafile.c - server side of Nautilus::Metafile
 *
 * Copyright (C) 2001 Eazel, Inc.
 * Copyright (C) 2001-2005 Free Software Foundation, Inc.
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

#include "nautilus-directory.h"
#include "nautilus-directory.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-thumbnails.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <glib/gurifuncs.h>
#include <libxml/parser.h>
#include <gtk/gtkmain.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define METAFILE_XML_VERSION "1.0"
#define METAFILE_PERMISSIONS 0600
#define METAFILES_DIRECTORY_NAME "metafiles"

enum {
	CHANGED,
	READY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* TODO asynchronous copying/moving of metadata
 *
 * - potential metadata loss when a deletion is scheduled, and new metadata is copied to
 *   this location before the old deletion is consumed
 *
 * - if metafile read fails, and a file from that metafile is scheduled for a copy/move operation,
 *   its associated operations are not removed from pending_copies
 *
 * */

static char    *get_file_metadata                  (NautilusMetafile *metafile,
						    const char       *file_name,
						    const char       *key,
						    const char       *default_metadata);
static GList   *get_file_metadata_list             (NautilusMetafile *metafile,
						    const char       *file_name,
						    const char       *list_key,
						    const char       *list_subkey);
static gboolean set_file_metadata                  (NautilusMetafile *metafile,
						    const char       *file_name,
						    const char       *key,
						    const char       *default_metadata,
						    const char       *metadata);
static gboolean set_file_metadata_list             (NautilusMetafile *metafile,
						    const char       *file_name,
						    const char       *list_key,
						    const char       *list_subkey,
						    GList            *list);
static void     rename_file_metadata               (NautilusMetafile *metafile,
						    const char       *old_file_name,
						    const char       *new_file_name);
static void     copy_file_metadata                 (NautilusMetafile *source_metafile,
						    const char       *source_file_name,
						    NautilusMetafile *destination_metafile,
						    const char       *destination_file_name);
static void     real_copy_file_metadata            (NautilusMetafile *source_metafile,
						    const char       *source_file_name,
						    NautilusMetafile *destination_metafile,
						    const char       *destination_file_name);
static void     remove_file_metadata               (NautilusMetafile *metafile,
						    const char       *file_name);
static void     real_remove_file_metadata          (NautilusMetafile *metafile,
						    const char       *file_name);
static void     call_metafile_changed_for_one_file (NautilusMetafile *metafile,
						    const char       *file_name);
static void     metafile_read_restart              (NautilusMetafile *metafile);
static void     metafile_read_start                (NautilusMetafile *metafile);
static void     metafile_write_start               (NautilusMetafile *metafile);
static void     directory_request_write_metafile   (NautilusMetafile *metafile);
static void     metafile_free_metadata             (NautilusMetafile *metafile);
static void     metafile_read_cancel               (NautilusMetafile *metafile);
static void     async_read_cancel                  (NautilusMetafile *metafile);
static void     set_metafile_contents              (NautilusMetafile *metafile,
						    xmlDocPtr         metafile_contents);

typedef struct MetafileReadState {
	NautilusMetafile *metafile;
	GCancellable *cancellable;
} MetafileReadState;

typedef struct MetafileWriteState {
	xmlChar *buffer;
	goffset size;
	gboolean write_again;
} MetafileWriteState;

struct _NautilusMetafile {
	GObject parent_slot;

	gboolean is_read;
	
	xmlDoc *xml;
	GHashTable *node_hash;
	GHashTable *changes;

	/* State for reading and writing metadata. */
	MetafileReadState *read_state;
	guint write_idle_id;
	MetafileWriteState *write_state;
	
	GList *monitors;

	char *private_uri;
	char *directory_uri;
};

static GHashTable *metafiles;

G_DEFINE_TYPE (NautilusMetafile, nautilus_metafile, G_TYPE_OBJECT);

static void
nautilus_metafile_init (NautilusMetafile *metafile)
{
	metafile->node_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
}

static void
finalize (GObject *object)
{
	NautilusMetafile  *metafile;

	metafile = NAUTILUS_METAFILE (object);

	g_assert (metafile->write_state == NULL);
	async_read_cancel (metafile);
	g_assert (metafile->read_state == NULL);

	g_hash_table_remove (metafiles, metafile->directory_uri);
	
	metafile_free_metadata (metafile);
	g_hash_table_destroy (metafile->node_hash);

	g_assert (metafile->write_idle_id == 0);

	g_free (metafile->private_uri);
	g_free (metafile->directory_uri);

	G_OBJECT_CLASS (nautilus_metafile_parent_class)->finalize (object);
}

static char *
escape_slashes (const char *str)
{
	int n_reserved;
	const char *p;
	char *escaped, *e;

	n_reserved = 0;
	for (p = str; *p != 0; p++) {
		if (*p == '%' || *p == '/') {
			n_reserved++;
		}
	}

	escaped = g_malloc (strlen (str) + 2*n_reserved + 1);

	e = escaped;
	
	for (p = str; *p != 0; p++) {
		if (*p == '%') {
			*e++ = '%';
			*e++ = '2';
			*e++ = '5';
		} else if (*p == '/') {
			*e++ = '%';
			*e++ = '2';
			*e++ = 'F';
		} else {
			*e++ = *p;
		}
	}
	*e = 0;

	return escaped;
}
	

static char *
construct_private_metafile_uri (const char *uri)
{
	char *user_directory, *metafiles_directory;
	char *escaped_uri, *file_name;
	char *alternate_path, *alternate_uri;

	/* Ensure that the metafiles directory exists. */
	user_directory = nautilus_get_user_directory ();
	metafiles_directory = g_build_filename (user_directory, METAFILES_DIRECTORY_NAME, NULL);
	g_free (user_directory);
	mkdir (metafiles_directory, 0700);

	/* Construct a file name from the URI. */
	escaped_uri = escape_slashes (uri);
	file_name = g_strconcat (escaped_uri, ".xml", NULL);
	g_free (escaped_uri);

	/* Construct a URI for something in the "metafiles" directory. */
	alternate_path = g_build_filename (metafiles_directory, file_name, NULL);
	g_free (metafiles_directory);
	g_free (file_name);
	alternate_uri = g_filename_to_uri (alternate_path, NULL, NULL);
	g_free (alternate_path);

	return alternate_uri;
}

static void
nautilus_metafile_set_directory_uri (NautilusMetafile *metafile,
				     const char *directory_uri)
{
	if (eel_strcmp (metafile->directory_uri, directory_uri) == 0) {
		return;
	}

	g_free (metafile->directory_uri);
	metafile->directory_uri = g_strdup (directory_uri);

	g_free (metafile->private_uri);
	metafile->private_uri
		= construct_private_metafile_uri (directory_uri);
}

static NautilusMetafile *
nautilus_metafile_new (const char *directory_uri)
{
	NautilusMetafile *metafile;
	
	metafile = NAUTILUS_METAFILE (g_object_new (NAUTILUS_TYPE_METAFILE, NULL));

	nautilus_metafile_set_directory_uri (metafile, directory_uri);

	return metafile;
}

NautilusMetafile *
nautilus_metafile_get_for_uri (const char *directory_uri)
{
	NautilusMetafile *metafile;
	char *canonical_uri;
	GFile *file;
	
	g_return_val_if_fail (directory_uri != NULL, NULL);

	if (metafiles == NULL) {
		metafiles = eel_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal, __FILE__ ": metafiles");
	}


	file = g_file_new_for_uri (directory_uri);
	canonical_uri = g_file_get_uri (file);
	g_object_unref (file);
	
	metafile = g_hash_table_lookup (metafiles, canonical_uri);
	
	if (metafile != NULL) {
		g_object_ref (metafile);
	} else {
		metafile = nautilus_metafile_new (canonical_uri);
		
		g_assert (strcmp (metafile->directory_uri, canonical_uri) == 0);

		g_hash_table_insert (metafiles,
				     metafile->directory_uri,
				     metafile);
	}
	
	g_free (canonical_uri);

	return metafile;
}

static GList *pending_copies;

typedef struct {
	NautilusMetafile *source_metafile;
	char             *source_file_name;
	NautilusMetafile *destination_metafile;
	char             *destination_file_name;
} NautilusMetadataCopy;

static gboolean
nautilus_metadata_copy_equal (const NautilusMetadataCopy *a,
			      const NautilusMetadataCopy *b)
{
	return (b->source_metafile == a->source_metafile)
	       && (b->destination_metafile == a->destination_metafile)
	       && (strcmp (a->source_file_name, b->source_file_name) == 0)
	       && (strcmp (a->destination_file_name, b->destination_file_name) == 0);
}

static NautilusMetadataCopy *
nautilus_metadata_get_scheduled_copy (NautilusMetafile *source_metafile,
				      const char       *source_file_name,
				      NautilusMetafile *destination_metafile,
				      const char       *destination_file_name)
{
	NautilusMetadataCopy key, *copy;
	GList *l;

	key.source_metafile = source_metafile;
	key.source_file_name = (char *) source_file_name;
	key.destination_metafile = destination_metafile;
	key.destination_file_name = (char *) destination_file_name;

	for (l = pending_copies; l != NULL; l = l->next) {
		copy = l->data;

		if (nautilus_metadata_copy_equal (l->data, &key)) {
			return copy;
		}
	}

	return NULL;
}

static gboolean
nautilus_metadata_has_scheduled_copy (NautilusMetafile *source_metafile,
				      const char       *source_file_name)
{
	NautilusMetadataCopy *copy;
	GList *l;

	for (l = pending_copies; l != NULL; l = l->next) {
		copy = l->data;

		if ((copy->source_metafile == source_metafile) &&
		    (strcmp (copy->source_file_name, source_file_name) == 0)) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
nautilus_metadata_schedule_copy (NautilusMetafile *source_metafile,
				 const char       *source_file_name,
				 NautilusMetafile *destination_metafile,
				 const char       *destination_file_name)
{
	NautilusMetadataCopy *copy;

	g_assert (!source_metafile->is_read || !destination_metafile->is_read);

	copy = nautilus_metadata_get_scheduled_copy (source_metafile,
						     source_file_name,
						     destination_metafile,
						     destination_file_name);
	if (copy == NULL) {
		copy = g_malloc (sizeof (NautilusMetadataCopy));
		copy->source_metafile = g_object_ref (source_metafile);
		copy->source_file_name = g_strdup (source_file_name);
		copy->destination_metafile = g_object_ref (destination_metafile);
		copy->destination_file_name = g_strdup (destination_file_name);

		pending_copies = g_list_prepend (pending_copies, copy);

		metafile_read_start (source_metafile);
		metafile_read_start (destination_metafile);
	}
}

static void
nautilus_metadata_process_ready_copies (void)
{
	NautilusMetadataCopy *copy;
	GList *l, *next;

	l = pending_copies;
	while (l != NULL) {
		copy = l->data;

		next = l->next;

		if (copy->source_metafile->is_read &&
		    copy->destination_metafile->is_read) {
			real_copy_file_metadata (copy->source_metafile, copy->source_file_name,
						 copy->destination_metafile, copy->destination_file_name);
			
			g_object_unref (copy->source_metafile);
			g_free (copy->source_file_name);
			g_object_unref (copy->destination_metafile);
			g_free (copy->destination_file_name);
			g_free (copy);

			pending_copies = g_list_delete_link (pending_copies, l);
		}

		l = next;
	}
}

static GList *pending_removals;

typedef struct {
	NautilusMetafile *metafile;
	char             *file_name;
} NautilusMetadataRemoval;

static gboolean
nautilus_metadata_removal_equal (const NautilusMetadataRemoval *a,
				 const NautilusMetadataRemoval *b)
{
	return ((b->metafile == a->metafile) &&
		(strcmp (a->file_name, b->file_name) == 0));
}

static NautilusMetadataRemoval *
nautilus_metadata_get_scheduled_removal (NautilusMetafile *metafile,
					 const char       *file_name)
{
	NautilusMetadataRemoval key, *removal;
	GList *l;

	key.metafile = metafile;
	key.file_name = (char *) file_name;

	for (l = pending_removals; l != NULL; l = l->next) {
		removal = l->data;

		if (nautilus_metadata_removal_equal (l->data, &key)) {
			return removal;
		}
	}

	return NULL;
}

static void
nautilus_metadata_schedule_removal (NautilusMetafile *metafile,
				    const char       *file_name)
{
	NautilusMetadataRemoval *removal;

	g_assert (nautilus_metadata_has_scheduled_copy (metafile, file_name));

	removal = nautilus_metadata_get_scheduled_removal (metafile, file_name);
	if (removal == NULL) {
		removal = g_malloc (sizeof (NautilusMetadataRemoval));
		removal->metafile = g_object_ref (metafile);
		removal->file_name = g_strdup (file_name);

		pending_removals = g_list_prepend (pending_removals, removal);
	}
}

static void
nautilus_metadata_process_ready_removals (void)
{
	NautilusMetadataRemoval *removal;
	GList *l, *next;

	l = pending_removals;
	while (l != NULL) {
		removal = l->data;

		next = l->next;

		if (!nautilus_metadata_has_scheduled_copy (removal->metafile, removal->file_name)) {
			real_remove_file_metadata (removal->metafile, removal->file_name);

			pending_removals = g_list_delete_link (pending_removals, l);

			g_object_unref (removal->metafile);
			g_free (removal->file_name);
		}

		l = next;
	}
}

/* FIXME
 * Right now we only limit the number of conccurrent reads.
 * We may want to consider limiting writes as well.
 */

static int num_reads_in_progress;
static GList *pending_reads;

#if 0
#define DEBUG_METADATA_IO
#endif

static void
schedule_next_read (void)
{	
	const int kMaxAsyncReads = 10;
	
	GList* node;
	
#ifdef DEBUG_METADATA_IO
		g_message ("schedule_next_read: %d pending reads, %d reads in progress",
			   g_list_length (pending_reads), num_reads_in_progress);
#endif

	if (pending_reads != NULL && num_reads_in_progress <= kMaxAsyncReads) {
		node = pending_reads;
		pending_reads = g_list_remove_link (pending_reads, node);
#ifdef DEBUG_METADATA_IO
		g_message ("schedule_next_read: %s", NAUTILUS_METAFILE (node->data)->details->directory_uri);
#endif
		metafile_read_start (node->data);
		g_list_free_1 (node);
		++num_reads_in_progress;
	}
}

static void
async_read_start (NautilusMetafile *metafile)
{
	if (metafile->is_read ||
	    metafile->read_state != NULL) {
	    return;
	}
#ifdef DEBUG_METADATA_IO
	g_message ("async_read_start: %s", metafile->directory_uri);
#endif
	pending_reads = g_list_prepend (pending_reads, metafile);
	schedule_next_read ();
}

static void
async_read_done (NautilusMetafile *metafile)
{
#ifdef DEBUG_METADATA_IO
	g_message ("async_read_done: %s", metafile->directory_uri);
#endif
	--num_reads_in_progress;
	schedule_next_read ();
}

static void
async_read_cancel (NautilusMetafile *metafile)
{
	GList* node;

#ifdef DEBUG_METADATA_IO
	g_message ("async_read_cancel: %s", metafile->directory_uri);
#endif
	node = g_list_find (pending_reads, metafile);

	if (node != NULL) {
		pending_reads = g_list_remove_link (pending_reads, node);
		g_list_free_1 (node);
	}
	
	if (metafile->read_state != NULL) {
		metafile_read_cancel (metafile);
		async_read_done (metafile);
	}

}

gboolean
nautilus_metafile_is_read (NautilusMetafile *metafile)
{
	return metafile->is_read;
}

char *
nautilus_metafile_get (NautilusMetafile               *metafile,
		       const char                     *file_name,
		       const char                     *key,
		       const char                     *default_value)
{
	return get_file_metadata (metafile, file_name, key, default_value);
}

GList *
nautilus_metafile_get_list (NautilusMetafile               *metafile,
			    const char                     *file_name,
			    const char                     *list_key,
			    const char                     *list_subkey)
{
	return get_file_metadata_list (metafile, file_name, list_key, list_subkey);
}


void
nautilus_metafile_set (NautilusMetafile               *metafile,
		       const char                     *file_name,
		       const char                     *key,
		       const char                     *default_value,
		       const char                     *metadata)
{
	if (set_file_metadata (metafile, file_name, key, default_value, metadata)) {
		call_metafile_changed_for_one_file (metafile, file_name);
	}
}

void
nautilus_metafile_set_list (NautilusMetafile               *metafile,
			    const char                     *file_name,
			    const char                     *list_key,
			    const char                     *list_subkey,
			    GList                          *list)
{
	if (set_file_metadata_list (metafile, file_name, list_key, list_subkey, list)) {
		call_metafile_changed_for_one_file (metafile, file_name);
	}
}

void
nautilus_metafile_copy (NautilusMetafile               *source_metafile,
			const char                     *source_file_name,
			const char                     *destination_directory_uri,
			const char                     *destination_file_name)
{
	NautilusMetafile *destination_metafile;

	destination_metafile = nautilus_metafile_get_for_uri (destination_directory_uri);

	copy_file_metadata (source_metafile, source_file_name,
			    destination_metafile, destination_file_name);
			    
	g_object_unref (destination_metafile);
}


void
nautilus_metafile_remove (NautilusMetafile               *metafile,
			  const char                     *file_name)
{
	remove_file_metadata (metafile, file_name);
}

void
nautilus_metafile_rename (NautilusMetafile               *metafile,
			  const char                     *old_file_name,
			  const char                     *new_file_name)
{
	rename_file_metadata (metafile, old_file_name, new_file_name);
}

void
nautilus_metafile_rename_directory (NautilusMetafile               *metafile,
				    const char                     *new_directory_uri)
{
	nautilus_metafile_set_directory_uri (metafile, new_directory_uri);
}

void
nautilus_metafile_load (NautilusMetafile *metafile)
{
	async_read_start (metafile);
}

static gboolean
notify_metafile_ready_idle (gpointer user_data)
{
	NautilusMetafile *metafile;
	metafile = user_data;
	
	g_signal_emit (metafile, signals[READY], 0);
	g_object_unref (metafile);
	return FALSE;
}

static void
nautilus_metafile_notify_metafile_ready (NautilusMetafile *metafile, gboolean in_idle)
{
	if (in_idle) {
		g_idle_add (notify_metafile_ready_idle, g_object_ref (metafile));
	} else {
		g_signal_emit (metafile, signals[READY], 0);
	}
}

typedef struct {
	NautilusMetafile *metafile;
	GList *file_names;
} ChangedData;

static gboolean
metafile_changed_idle (gpointer user_data)
{
	ChangedData *data;

	data = user_data;

	g_signal_emit (data->metafile, signals[CHANGED], 0, data->file_names);
	
	g_object_unref (data->metafile);
	eel_g_list_free_deep (data->file_names);
	g_free (data);

	return FALSE;
}

static void
call_metafile_changed (NautilusMetafile *metafile,
		       GList *file_names)
{
	ChangedData *data;

	data = g_new (ChangedData, 1);
	data->metafile = g_object_ref (metafile);
	data->file_names = eel_g_str_list_copy (file_names);
	
	g_idle_add (metafile_changed_idle, data);
}

#if 0
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
call_metafile_changed_for_all_files_mentioned_in_metafile (NautilusMetafile *metafile)
{
	CORBA_unsigned_long   len;
	Nautilus_FileNameList file_names;

	len = g_hash_table_size (metafile->node_hash);

	if (len > 0) {
		file_names._maximum =  len;
		file_names._length  =  0;
		file_names._buffer  =  g_new (CORBA_char *, len);

		g_hash_table_foreach (metafile->node_hash,
				      file_list_filler_ghfunc,
				      &file_names);

		call_metafile_changed (metafile, &file_names);

		g_free (file_names._buffer);
	}
}
#endif

static void
call_metafile_changed_for_one_file (NautilusMetafile *metafile,
				    const char  *file_name)
{
	GList l;

	l.next = NULL;
	l.prev = NULL;
	l.data = (void *)file_name;
	
	call_metafile_changed (metafile, &l);
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
	return eel_xml_get_property_for_children
		(node, list_key, list_subkey);
}

static xmlNode *
create_metafile_root (NautilusMetafile *metafile)
{
	xmlNode *root;

	if (metafile->xml == NULL) {
		set_metafile_contents (metafile, xmlNewDoc (METAFILE_XML_VERSION));
	}
	root = xmlDocGetRootElement (metafile->xml);
	if (root == NULL) {
		root = xmlNewDocNode (metafile->xml, NULL, "directory", NULL);
		xmlDocSetRootElement (metafile->xml, root);
	}

	return root;
}

static xmlNode *
get_file_node (NautilusMetafile *metafile,
	       const char *file_name,
	       gboolean create)
{
	GHashTable *hash;
	xmlNode *root, *node;
	char *escaped_file_name;
	
	g_assert (NAUTILUS_IS_METAFILE (metafile));

	hash = metafile->node_hash;
	node = g_hash_table_lookup (hash, file_name);
	if (node != NULL) {
		return node;
	}
	
	if (create) {
		root = create_metafile_root (metafile);
		node = xmlNewChild (root, NULL, "file", NULL);
		escaped_file_name = g_uri_escape_string (file_name, NULL, 0);
		xmlSetProp (node, "name", escaped_file_name);
		g_free (escaped_file_name);
		g_hash_table_insert (hash, xmlMemStrdup (file_name), node);
		return node;
	}
	
	return NULL;
}

static void
set_file_node_timestamp (xmlNode *node)
{
	char time_str[21];

	/* 2^64 turns out to be 20 characters */
	snprintf (time_str, 20, "%ld", time (NULL));
	time_str [20] = '\0';
	xmlSetProp (node, "timestamp", time_str);
}

static char *
get_metadata_string_from_metafile (NautilusMetafile *metafile,
				   const char *file_name,
				   const char *key,
				   const char *default_metadata)
{
	xmlNode *node;

	node = get_file_node (metafile, file_name, FALSE);
	return get_metadata_from_node (node, key, default_metadata);
}

static GList *
get_metadata_list_from_metafile (NautilusMetafile *metafile,
				 const char *file_name,
				 const char *list_key,
				 const char *list_subkey)
{
	xmlNode *node;

	node = get_file_node (metafile, file_name, FALSE);
	return get_metadata_list_from_node (node, list_key, list_subkey);
}

static gboolean
set_metadata_string_in_metafile (NautilusMetafile *metafile,
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
		(metafile, file_name, key, default_metadata);

	old_metadata_matches = eel_strcmp (old_metadata, metadata) == 0;
	g_free (old_metadata);
	if (old_metadata_matches) {
		return FALSE;
	}

	/* Data that matches the default is represented in the tree by
	 * the lack of an attribute.
	 */
	if (eel_strcmp (default_metadata, metadata) == 0) {
		value = NULL;
	} else {
		value = metadata;
	}

	/* Get or create the node. */
	node = get_file_node (metafile, file_name, value != NULL);

	if (node != NULL) {
		/* Set the timestamp */
		set_file_node_timestamp (node);
		
		/* Add or remove a property node. */
		property_node = xmlSetProp (node, key, value);
		if (value == NULL) {
			xmlRemoveProp (property_node);
		}
	}
	
	/* Since we changed the tree, arrange for it to be written. */
	directory_request_write_metafile (metafile);
	return TRUE;
}

static gboolean
set_metadata_list_in_metafile (NautilusMetafile *metafile,
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
	node = get_file_node (metafile, file_name, list != NULL);

	/* Work with the list. */
	changed = FALSE;
	if (node == NULL) {
		g_assert (list == NULL);
	} else {
		p = list;

		/* Remove any nodes except the ones we expect. */
		for (child = eel_xml_get_children (node);
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

		/* Set the timestamp */
		set_file_node_timestamp (node);
	}

	if (!changed) {
		return FALSE;
	}

	directory_request_write_metafile (metafile);
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
	value->value.string_list = eel_g_str_list_copy (metadata);

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
		eel_g_list_free_deep (value->value.string_list);
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
		return eel_strcmp (value_a->value.string,
					value_b->value.string) == 0
			&& eel_strcmp (value_a->default_value,
					    value_b->default_value) == 0;
	} else {
		g_assert (value_a->default_value == NULL);
		g_assert (value_b->default_value == NULL);

		return eel_g_str_list_equal
			(value_a->value.string_list,
			 value_b->value.string_list);
	}
}

static gboolean
set_metadata_in_metafile (NautilusMetafile *metafile,
			  const char *file_name,
			  const char *key,
			  const char *subkey,
			  const MetadataValue *value)
{
	gboolean changed;

	if (!value->is_list) {
		g_assert (subkey == NULL);
		changed = set_metadata_string_in_metafile
			(metafile, file_name, key,
			 value->default_value,
			 value->value.string);
	} else {
		g_assert (value->default_value == NULL);
		changed = set_metadata_list_in_metafile
			(metafile, file_name, key, subkey,
			 value->value.string_list);
	}

	return changed;
}

static char *
get_metadata_string_from_table (NautilusMetafile *metafile,
				const char *file_name,
				const char *key,
				const char *default_metadata)
{
	GHashTable *directory_table, *file_table;
	MetadataValue *value;

	/* Get the value from the hash table. */
	directory_table = metafile->changes;
        file_table = directory_table == NULL ? NULL
		: g_hash_table_lookup (directory_table, file_name);
	value = file_table == NULL ? NULL
		: g_hash_table_lookup (file_table, key);
	if (value == NULL) {
		return g_strdup (default_metadata);
	}
	
	/* Convert it to a string. */
	g_assert (!value->is_list);
	if (eel_strcmp (value->value.string, value->default_value) == 0) {
		return g_strdup (default_metadata);
	}
	return g_strdup (value->value.string);
}

static GList *
get_metadata_list_from_table (NautilusMetafile *metafile,
			      const char *file_name,
			      const char *key,
			      const char *subkey)
{
	GHashTable *directory_table, *file_table;
	char *combined_key;
	MetadataValue *value;

	/* Get the value from the hash table. */
	directory_table = metafile->changes;
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
	return eel_g_str_list_copy (value->value.string_list);
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
set_metadata_eat_value (NautilusMetafile *metafile,
			const char *file_name,
			const char *key,
			const char *subkey,
			MetadataValue *value)
{
	GHashTable *directory_table, *file_table;
	gboolean changed;
	char *combined_key;
	MetadataValue *old_value;

	if (metafile->is_read) {
		changed = set_metadata_in_metafile
			(metafile, file_name, key, subkey, value);
		metadata_value_destroy (value);
	} else {
		/* Create hash table only when we need it.
		 * We'll destroy it when we finish reading the metafile.
		 */
		directory_table = metafile->changes;
		if (directory_table == NULL) {
			directory_table = g_hash_table_new
				(str_or_null_hash, str_or_null_equal);
			metafile->changes = directory_table;
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

static void
metafile_free_metadata (NautilusMetafile *metafile)
{
	g_return_if_fail (NAUTILUS_IS_METAFILE (metafile));

	g_hash_table_foreach (metafile->node_hash,
			      destroy_xml_string_key, NULL);
	xmlFreeDoc (metafile->xml);
	destroy_metadata_changes_hash_table (metafile->changes);
}

static char *
get_file_metadata (NautilusMetafile *metafile,
		   const char *file_name,
		   const char *key,
		   const char *default_metadata)
{
	g_return_val_if_fail (NAUTILUS_IS_METAFILE (metafile), NULL);
	g_return_val_if_fail (!eel_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!eel_str_is_empty (key), NULL);

	if (metafile->is_read) {
		return get_metadata_string_from_metafile
			(metafile, file_name, key, default_metadata);
	} else {
		return get_metadata_string_from_table
			(metafile, file_name, key, default_metadata);
	}
}

static GList *
get_file_metadata_list (NautilusMetafile *metafile,
			const char *file_name,
			const char *list_key,
			const char *list_subkey)
{
	g_return_val_if_fail (NAUTILUS_IS_METAFILE (metafile), NULL);
	g_return_val_if_fail (!eel_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!eel_str_is_empty (list_key), NULL);
	g_return_val_if_fail (!eel_str_is_empty (list_subkey), NULL);

	if (metafile->is_read) {
		return get_metadata_list_from_metafile
			(metafile, file_name, list_key, list_subkey);
	} else {
		return get_metadata_list_from_table
			(metafile, file_name, list_key, list_subkey);
	}
}

static gboolean
set_file_metadata (NautilusMetafile *metafile,
		   const char *file_name,
		   const char *key,
		   const char *default_metadata,
		   const char *metadata)
{
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_METAFILE (metafile), FALSE);
	g_return_val_if_fail (!eel_str_is_empty (file_name), FALSE);
	g_return_val_if_fail (!eel_str_is_empty (key), FALSE);

	if (metafile->is_read) {
		return set_metadata_string_in_metafile (metafile, file_name, key,
							default_metadata, metadata);
	} else {
		value = metadata_value_new (default_metadata, metadata);
		return set_metadata_eat_value (metafile, file_name,
					       key, NULL, value);
	}
}

static gboolean
set_file_metadata_list (NautilusMetafile *metafile,
			const char *file_name,
			const char *list_key,
			const char *list_subkey,
			GList *list)
{
	MetadataValue *value;

	g_return_val_if_fail (NAUTILUS_IS_METAFILE (metafile), FALSE);
	g_return_val_if_fail (!eel_str_is_empty (file_name), FALSE);
	g_return_val_if_fail (!eel_str_is_empty (list_key), FALSE);
	g_return_val_if_fail (!eel_str_is_empty (list_subkey), FALSE);

	if (metafile->is_read) {
		return set_metadata_list_in_metafile (metafile, file_name,
						      list_key, list_subkey, list);
	} else {
		value = metadata_value_new_list (list);
		return set_metadata_eat_value (metafile, file_name,
					       list_key, list_subkey, value);
	}
}

static char *
metafile_get_file_uri (NautilusMetafile *metafile,
		       const char *file_name)
{
	char *escaped_file_name, *uri;
	
	g_assert (NAUTILUS_IS_METAFILE (metafile));
	g_assert (file_name != NULL);
	
	escaped_file_name = g_uri_escape_string (file_name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
	
	uri = g_build_filename (metafile->directory_uri, escaped_file_name, NULL);
	g_free (escaped_file_name);
	return uri;
}

static void
rename_file_metadata (NautilusMetafile *metafile,
		      const char *old_file_name,
		      const char *new_file_name)
{
	gboolean found;
	gpointer key, value;
	xmlNode *file_node;
	GHashTable *hash;
	char *old_file_uri, *new_file_uri;
	char *escaped;

	g_assert (NAUTILUS_IS_METAFILE (metafile));
	g_assert (old_file_name != NULL);
	g_assert (new_file_name != NULL);

	remove_file_metadata (metafile, new_file_name);

	if (metafile->is_read) {
		/* Move data in XML document if present. */
		hash = metafile->node_hash;
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
			escaped = g_uri_escape_string (new_file_name, NULL, FALSE);
			xmlSetProp (file_node, "name", escaped);
			g_free (escaped);
			directory_request_write_metafile (metafile);
		}
	} else {
		/* Move data in hash table. */
		/* FIXME: If there's data for this file in the
		 * metafile on disk, this doesn't arrange for that
		 * data to be moved to the new name.
		 */
		hash = metafile->changes;
		found = g_hash_table_lookup_extended
			(hash, old_file_name, &key, &value);
		if (found) {
			g_hash_table_remove (hash, old_file_name);
			g_free (key);
			g_hash_table_insert (hash, g_strdup (new_file_name), value);
		}
	}

	/* Rename the thumbnails for the file, if any. */
	old_file_uri = metafile_get_file_uri (metafile, old_file_name);
	new_file_uri = metafile_get_file_uri (metafile, new_file_name);
	nautilus_update_thumbnail_file_renamed (old_file_uri, new_file_uri);
	g_free (old_file_uri);
	g_free (new_file_uri);
}

typedef struct {
	NautilusMetafile *metafile;
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
	set_metadata_in_metafile (context->metafile, context->file_name,
				  metadata_key, subkey, value);
	g_free (key_prefix);
}

static void
apply_file_changes (NautilusMetafile *metafile,
		    const char *file_name,
		    GHashTable *changes)
{
	ChangeContext context;

	g_assert (NAUTILUS_IS_METAFILE (metafile));
	g_assert (file_name != NULL);
	g_assert (changes != NULL);

	context.metafile = metafile;
	context.file_name = file_name;

	g_hash_table_foreach (changes, apply_one_change, &context);
}

static void
apply_one_file_changes (gpointer key, gpointer value, gpointer callback_data)
{
	apply_file_changes (callback_data, key, value);
	g_hash_table_destroy (value);
}

static void
nautilus_metafile_apply_pending_changes (NautilusMetafile *metafile)
{
	if (metafile->changes == NULL) {
		return;
	}
	g_hash_table_foreach (metafile->changes,
			      apply_one_file_changes, metafile);
	g_hash_table_destroy (metafile->changes);
	metafile->changes = NULL;
}

static void
real_copy_file_metadata (NautilusMetafile *source_metafile,
			 const char *source_file_name,
			 NautilusMetafile *destination_metafile,
			 const char *destination_file_name)
{
	xmlNodePtr source_node, node, root;
	GHashTable *hash, *changes;
	char *escaped;

	real_remove_file_metadata (destination_metafile, destination_file_name);
	g_assert (get_file_node (destination_metafile, destination_file_name, FALSE) == NULL);

	source_node = get_file_node (source_metafile, source_file_name, FALSE);
	if (source_node != NULL) {
		node = xmlCopyNode (source_node, TRUE);
		root = create_metafile_root (destination_metafile);
		xmlAddChild (root, node);
		escaped = g_uri_escape_string (destination_file_name, NULL, FALSE);
		xmlSetProp (node, "name", escaped);
		g_free (escaped);
		set_file_node_timestamp (node);
		g_hash_table_insert (destination_metafile->node_hash,
				     xmlMemStrdup (destination_file_name), node);
		directory_request_write_metafile (destination_metafile);
	}

	hash = source_metafile->changes;
	if (hash != NULL) {
		changes = g_hash_table_lookup (hash, source_file_name);
		if (changes != NULL) {
			apply_file_changes (destination_metafile,
					    destination_file_name,
					    changes);
		}
	}
}

static void
copy_file_metadata (NautilusMetafile *source_metafile,
		    const char *source_file_name,
		    NautilusMetafile *destination_metafile,
		    const char *destination_file_name)
{
	char *source_file_uri;
	char *destination_file_uri;

	g_assert (NAUTILUS_IS_METAFILE (source_metafile));
	g_assert (source_file_name != NULL);
	g_assert (NAUTILUS_IS_METAFILE (destination_metafile));
	g_assert (destination_file_name != NULL);

	if (source_metafile->is_read
	    && destination_metafile->is_read) {
		real_copy_file_metadata (source_metafile,
					 source_file_name,
					 destination_metafile,
					 destination_file_name);
	} else {
		nautilus_metadata_schedule_copy (source_metafile,
						source_file_name,
						destination_metafile,
						destination_file_name);
        }

	/* Copy the thumbnail for the file, if any. */
	source_file_uri = metafile_get_file_uri (source_metafile, source_file_name);
	destination_file_uri = metafile_get_file_uri (destination_metafile, destination_file_name);
	nautilus_update_thumbnail_file_copied (source_file_uri, destination_file_uri);
	g_free (source_file_uri);
	g_free (destination_file_uri);
}

static void
real_remove_file_metadata (NautilusMetafile *metafile,
			   const char *file_name)
{
	gboolean found;
	gpointer key, value;
	xmlNode *file_node;
	GHashTable *hash;

	g_return_if_fail (NAUTILUS_IS_METAFILE (metafile));
	g_return_if_fail (file_name != NULL);

	if (metafile->is_read) {
		/* Remove data in XML document if present. */
		hash = metafile->node_hash;
		found = g_hash_table_lookup_extended
			(hash, file_name, &key, &value);
		if (found) {
			g_assert (strcmp ((const char *) key, file_name) == 0);
			file_node = value;
			g_hash_table_remove (hash,
					     file_name);
			xmlFree (key);
			xmlUnlinkNode (file_node);
			xmlFreeNode (file_node);
			directory_request_write_metafile (metafile);
		}
	} else {
		/* Remove data from hash table. */
		/* FIXME: If there's data for this file on the
		 * metafile on disk, this does not arrange for it to
		 * be removed when the metafile is later read.
		 */
		hash = metafile->changes;
		if (hash != NULL) {
			found = g_hash_table_lookup_extended
				(hash, file_name, &key, &value);
			if (found) {
				g_hash_table_remove (hash, file_name);
				g_free (key);
				destroy_metadata_changes_hash_table (value);
			}
		}
	}
}

static void
remove_file_metadata (NautilusMetafile *metafile,
		      const char *file_name)
{
	char *file_uri;

	g_assert (NAUTILUS_IS_METAFILE (metafile));
	g_assert (file_name != NULL);

	if (nautilus_metadata_has_scheduled_copy (metafile, file_name)) {
		nautilus_metadata_schedule_removal (metafile, file_name);
	} else {
		real_remove_file_metadata (metafile, file_name);
	}

	/* Delete the thumbnails for the file, if any. */
	file_uri = metafile_get_file_uri (metafile, file_name);
	nautilus_remove_thumbnail_for_file (file_uri);
	g_free (file_uri);
}

static void
set_metafile_contents (NautilusMetafile *metafile,
		       xmlDocPtr metafile_contents)
{
	GHashTable *hash;
	xmlNodePtr node;
	xmlChar *name;
	char *unescaped_name;

	g_assert (NAUTILUS_IS_METAFILE (metafile));
	g_assert (metafile->xml == NULL);

	if (metafile_contents == NULL) {
		return;
	}

	metafile->xml = metafile_contents;
	
	/* Populate the node hash table. */
	hash = metafile->node_hash;
	for (node = eel_xml_get_root_children (metafile_contents);
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "file") == 0) {
			name = xmlGetProp (node, "name");
			unescaped_name = g_uri_unescape_string (name, "/");
			xmlFree (name);
			if (unescaped_name == NULL ||
			    g_hash_table_lookup (hash, unescaped_name) != NULL) {
				/* FIXME: Should we delete duplicate nodes as we discover them? */
				g_free (unescaped_name);
			} else {
				g_hash_table_insert (hash, unescaped_name, node);
			}
		}
	}
}

static void
metafile_read_cancel (NautilusMetafile *metafile)
{
	if (metafile->read_state != NULL) {
		g_cancellable_cancel (metafile->read_state->cancellable);
		metafile->read_state->metafile = NULL;
		metafile->read_state = NULL;
	}
}

static void
metafile_read_state_free (MetafileReadState *state)
{
	if (state == NULL) {
		return;
	}

	g_object_unref (state->cancellable);
	g_free (state);
}

static void
metafile_read_mark_done (NautilusMetafile *metafile, gboolean callback_in_idle)
{
	metafile_read_state_free (metafile->read_state);
	metafile->read_state = NULL;	

	metafile->is_read = TRUE;

	/* Move over the changes to the metafile that were in the hash table. */
	nautilus_metafile_apply_pending_changes (metafile);

	/* Tell change-watchers that we have update information. */
	nautilus_metafile_notify_metafile_ready (metafile, callback_in_idle);

	async_read_done (metafile);
}

static void
metafile_read_done_callback (GObject *source_object,
			     GAsyncResult *res,
			     gpointer user_data)
{
	MetafileReadState *state;
	NautilusMetafile *metafile;
	gsize file_size;
	char *file_contents;

	state = user_data;
 
	if (state->metafile == NULL) {
		/* Operation was cancelled. Bail out */
		metafile_read_state_free (state);
		return;
	}
	
	metafile = state->metafile;
	g_assert (metafile->xml == NULL);

	if (g_file_load_contents_finish (G_FILE (source_object),
					 res,
					 &file_contents, &file_size,
					 NULL, NULL)) {
		set_metafile_contents (metafile, xmlParseMemory (file_contents, file_size));
		g_free (file_contents);
	}

	metafile_read_mark_done (metafile, FALSE);

	nautilus_metadata_process_ready_copies ();
	nautilus_metadata_process_ready_removals ();
}

static void
metafile_read_restart (NautilusMetafile *metafile)
{
	GFile *location;
	MetafileReadState *state;

	state = g_new0 (MetafileReadState, 1);
	state->metafile = metafile;
	state->cancellable = g_cancellable_new ();

	metafile->read_state = state;

	location = g_file_new_for_uri (metafile->private_uri);

	g_file_load_contents_async (location, state->cancellable,
				    metafile_read_done_callback, state);
	
	g_object_unref (location);
}

static gboolean
allow_metafile (NautilusMetafile *metafile)
{
	const char *uri;

	g_assert (NAUTILUS_IS_METAFILE (metafile));

	/* Note that this inhibits both reading and writing metadata
	 * completely. In the future we may want to inhibit writing to
	 * the real directory while allowing parallel-directory
	 * metadata.
	 */

	/* For now, hard-code these schemes. Perhaps we should
	 * hardcode the schemes that are good for metadata instead of
	 * the schemes that are bad for it.
	 */
	/* FIXME bugzilla.gnome.org 42434: 
	 * We need to handle this in a better way. Perhaps a
	 * better way can wait until we have support for metadata
	 * access inside gnome-vfs.
	 */
	uri = metafile->directory_uri;
	if (eel_uri_is_search (uri) ||
	    eel_istr_has_prefix (uri, "gnome-help:") ||
	    eel_istr_has_prefix (uri, "help:")
	    ) {
		return FALSE;
	}
	
	return TRUE;
}

static void
metafile_read_start (NautilusMetafile *metafile)
{
	g_assert (NAUTILUS_IS_METAFILE (metafile));

	if (metafile->is_read ||
	    metafile->read_state != NULL) {
		return;
	}

	if (!allow_metafile (metafile)) {
		metafile_read_mark_done (metafile, TRUE);
	} else {
		metafile_read_restart (metafile);
	}
}

static void
metafile_write_done (NautilusMetafile *metafile)
{
	if (metafile->write_state->write_again) {
		metafile_write_start (metafile);
		return;
	}

	xmlFree (metafile->write_state->buffer);
	g_free (metafile->write_state);
	metafile->write_state = NULL;
	g_object_unref (metafile);
}

static void
metafile_write_failed (NautilusMetafile *metafile)
{
	metafile_write_done (metafile);
}

static void
metafile_write_succeeded (NautilusMetafile *metafile)
{
	metafile_write_done (metafile);
}

static int
write_all (int fd, const char *buffer, int size)
{
	int size_remaining;
	const char *p;
	ssize_t result;

	p = buffer;
	size_remaining = size;
	while (size_remaining != 0) {
		result = write (fd, p, size_remaining);
		if (result <= 0 || result > size_remaining) {
			return -1;
		}
		p += result;
		size_remaining -= result;
	}

	return size;
}

static void
metafile_write_local (NautilusMetafile *metafile,
		      const char *metafile_path)
{
	char *temp_path;
	int fd;
	gboolean failed;

	/* Do this synchronously, since it's likely to be local. Use
	 * mkstemp to prevent security exploits by making symbolic
	 * links named .nautilus-metafile.xml.
	 */

	temp_path = g_strconcat (metafile_path, "XXXXXX", NULL);
	failed = FALSE;

	fd = mkstemp (temp_path);
	if (fd == -1) {
		failed = TRUE;
	}
	if (!failed && fchmod (fd, METAFILE_PERMISSIONS) == -1) {
		failed = TRUE;
	}
	if (!failed && write_all (fd,
				  metafile->write_state->buffer,
				  metafile->write_state->size) == -1) {
		failed = TRUE;
	}
	if (fd != -1 && close (fd) == -1) {
		failed = TRUE;
	}
	if (failed && fd != -1) {
		unlink (temp_path);
	}
	if (!failed && rename (temp_path, metafile_path) == -1) {
		failed = TRUE;
	}
	g_free (temp_path);

	if (failed) {
		metafile_write_failed (metafile);
	} else {
		metafile_write_succeeded (metafile);
	}
}

static void
metafile_write_start (NautilusMetafile *metafile)
{
	const char *metafile_uri;
	char *metafile_path;

	g_assert (NAUTILUS_IS_METAFILE (metafile));

	metafile->write_state->write_again = FALSE;

	metafile_uri = metafile->private_uri;

	metafile_path = g_filename_from_uri (metafile_uri, NULL, NULL);
	g_assert (metafile_path != NULL);
	
	metafile_write_local (metafile, metafile_path);
	g_free (metafile_path);
}

static void
metafile_write (NautilusMetafile *metafile)
{
	int xml_doc_size;
	
	g_assert (NAUTILUS_IS_METAFILE (metafile));

	g_object_ref (metafile);

	/* If we are already writing, then just remember to do it again. */
	if (metafile->write_state != NULL) {
		g_object_unref (metafile);
		metafile->write_state->write_again = TRUE;
		return;
	}

	/* Don't write anything if there's nothing to write.
	 * At some point, we might want to change this to actually delete
	 * the metafile in this case.
	 */
	if (metafile->xml == NULL) {
		g_object_unref (metafile);
		return;
	}

	/* Create the write state. */
	metafile->write_state = g_new0 (MetafileWriteState, 1);
	xmlDocDumpMemory (metafile->xml,
			  &metafile->write_state->buffer,
			  &xml_doc_size);
	metafile->write_state->size = xml_doc_size;
	metafile_write_start (metafile);
}

static gboolean
metafile_write_idle_callback (gpointer callback_data)
{
	NautilusMetafile *metafile;

	metafile = NAUTILUS_METAFILE (callback_data);

	metafile->write_idle_id = 0;
	metafile_write (metafile);

	g_object_unref (metafile);

	return FALSE;
}

static void
directory_request_write_metafile (NautilusMetafile *metafile)
{
	g_assert (NAUTILUS_IS_METAFILE (metafile));

	if (!allow_metafile (metafile)) {
		return;
	}

	/* Set up an idle task that will write the metafile. */
	if (metafile->write_idle_id == 0) {
		g_object_ref (metafile);
		metafile->write_idle_id =
			g_idle_add (metafile_write_idle_callback, metafile);
	}
}

static void
nautilus_metafile_class_init (NautilusMetafileClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = finalize;

	signals[CHANGED] = g_signal_new ("changed",
					 NAUTILUS_TYPE_METAFILE,
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[READY] = g_signal_new ("ready",
				       NAUTILUS_TYPE_METAFILE,
				       G_SIGNAL_RUN_LAST,
				       0,
				       NULL, NULL,
				       g_cclosure_marshal_VOID__VOID,
				       G_TYPE_NONE, 0);
}
