/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-metafile.h - server side of Nautilus::Metafile
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

#ifndef NAUTILUS_METAFILE_H
#define NAUTILUS_METAFILE_H

#include <glib-object.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libxml/tree.h>

#define NAUTILUS_TYPE_METAFILE	          (nautilus_metafile_get_type ())
#define NAUTILUS_METAFILE(obj)	          (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_METAFILE, NautilusMetafile))
#define NAUTILUS_METAFILE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_METAFILE, NautilusMetafileClass))
#define NAUTILUS_IS_METAFILE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_METAFILE))
#define NAUTILUS_IS_METAFILE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_METAFILE))

typedef struct _NautilusMetafile NautilusMetafile;

typedef struct {
	GObjectClass parent_slot;

	void *(*changed) (NautilusMetafile *metafile,
			  GList *files);
	void *(*ready)   (NautilusMetafile *metafile);
} NautilusMetafileClass;

GType   nautilus_metafile_get_type (void);

NautilusMetafile *nautilus_metafile_get_for_uri (const char *directory_uri);

gboolean nautilus_metafile_is_read            (NautilusMetafile               *metafile);
char *   nautilus_metafile_get                (NautilusMetafile               *metafile,
					       const char                     *file_name,
					       const char                     *key,
					       const char                     *default_value);
GList *  nautilus_metafile_get_list           (NautilusMetafile               *metafile,
					       const char                     *file_name,
					       const char                     *list_key,
					       const char                     *list_subkey);
void     nautilus_metafile_set                (NautilusMetafile               *metafile,
					       const char                     *file_name,
					       const char                     *key,
					       const char                     *default_value,
					       const char                     *metadata);
void     nautilus_metafile_set_list           (NautilusMetafile               *metafile,
					       const char                     *file_name,
					       const char                     *list_key,
					       const char                     *list_subkey,
					       GList                          *list);
void     nautilus_metafile_copy               (NautilusMetafile               *metafile,
					       const char                     *source_file_name,
					       const char                     *destination_directory_uri,
					       const char                     *destination_file_name);
void     nautilus_metafile_remove             (NautilusMetafile               *metafile,
					       const char                     *file_name);
void     nautilus_metafile_rename             (NautilusMetafile               *metafile,
					       const char                     *old_file_name,
					       const char                     *new_file_name);
void     nautilus_metafile_rename_directory   (NautilusMetafile               *metafile,
					       const char                     *new_directory_uri);
void     nautilus_metafile_load               (NautilusMetafile               *metafile);

#endif /* NAUTILUS_METAFILE_H */
