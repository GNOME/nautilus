/*
 * Copyright (c) 2011 Red Hat, Inc.
 * Copyright (c) 2013 Ignacio Casal Quinteiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __GD_TAGGED_ENTRY_H__
#define __GD_TAGGED_ENTRY_H__

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GD_TYPE_TAGGED_ENTRY gd_tagged_entry_get_type()
#define GD_TAGGED_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GD_TYPE_TAGGED_ENTRY, GdTaggedEntry))
#define GD_TAGGED_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GD_TYPE_TAGGED_ENTRY, GdTaggedEntryClass))
#define GD_IS_TAGGED_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GD_TYPE_TAGGED_ENTRY))
#define GD_IS_TAGGED_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GD_TYPE_TAGGED_ENTRY))
#define GD_TAGGED_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GD_TYPE_TAGGED_ENTRY, GdTaggedEntryClass))

typedef struct _GdTaggedEntry GdTaggedEntry;
typedef struct _GdTaggedEntryClass GdTaggedEntryClass;
typedef struct _GdTaggedEntryPrivate GdTaggedEntryPrivate;

typedef struct _GdTaggedEntryTag GdTaggedEntryTag;
typedef struct _GdTaggedEntryTagClass GdTaggedEntryTagClass;
typedef struct _GdTaggedEntryTagPrivate GdTaggedEntryTagPrivate;

struct _GdTaggedEntry
{
  GtkSearchEntry parent;

  GdTaggedEntryPrivate *priv;
};

struct _GdTaggedEntryClass
{
  GtkSearchEntryClass parent_class;
};

#define GD_TYPE_TAGGED_ENTRY_TAG gd_tagged_entry_tag_get_type()
#define GD_TAGGED_ENTRY_TAG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GD_TYPE_TAGGED_ENTRY_TAG, GdTaggedEntryTag))
#define GD_TAGGED_ENTRY_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GD_TYPE_TAGGED_ENTRY_TAG, GdTaggedEntryTagClass))
#define GD_IS_TAGGED_ENTRY_TAG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GD_TYPE_TAGGED_ENTRY_TAG))
#define GD_IS_TAGGED_ENTRY_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GD_TYPE_TAGGED_ENTRY_TAG))
#define GD_TAGGED_ENTRY_TAG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GD_TYPE_TAGGED_ENTRY_TAG, GdTaggedEntryTagClass))

struct _GdTaggedEntryTag
{
  GObject parent;

  GdTaggedEntryTagPrivate *priv;
};

struct _GdTaggedEntryTagClass
{
  GObjectClass parent_class;
};

GType gd_tagged_entry_get_type (void) G_GNUC_CONST;

GdTaggedEntry *gd_tagged_entry_new (void);

void     gd_tagged_entry_set_tag_button_visible (GdTaggedEntry *self,
                                                 gboolean       visible);
gboolean gd_tagged_entry_get_tag_button_visible (GdTaggedEntry *self);

gboolean gd_tagged_entry_insert_tag (GdTaggedEntry    *self,
                                     GdTaggedEntryTag *tag,
                                     gint              position);

gboolean gd_tagged_entry_add_tag (GdTaggedEntry    *self,
                                  GdTaggedEntryTag *tag);

gboolean gd_tagged_entry_remove_tag (GdTaggedEntry *self,
                                     GdTaggedEntryTag *tag);

GType gd_tagged_entry_tag_get_type (void) G_GNUC_CONST;

GdTaggedEntryTag *gd_tagged_entry_tag_new (const gchar *label);

void gd_tagged_entry_tag_set_label (GdTaggedEntryTag *tag,
                                    const gchar *label);
const gchar *gd_tagged_entry_tag_get_label (GdTaggedEntryTag *tag);

void gd_tagged_entry_tag_set_has_close_button (GdTaggedEntryTag *tag,
                                               gboolean has_close_button);
gboolean gd_tagged_entry_tag_get_has_close_button (GdTaggedEntryTag *tag);

void gd_tagged_entry_tag_set_style (GdTaggedEntryTag *tag,
                                    const gchar *style);
const gchar *gd_tagged_entry_tag_get_style (GdTaggedEntryTag *tag);

gboolean gd_tagged_entry_tag_get_area (GdTaggedEntryTag      *tag,
                                       cairo_rectangle_int_t *rect);

G_END_DECLS

#endif /* __GD_TAGGED_ENTRY_H__ */
