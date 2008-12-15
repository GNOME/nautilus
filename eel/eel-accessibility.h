/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* eel-accessibility.h - Utility functions for accessibility

   Copyright (C) 2002 Anders Carlsson

   The Eel Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Eel Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Eel Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#ifndef EEL_ACCESSIBILITY_H
#define EEL_ACCESSIBILITY_H

#include <glib-object.h>
#include <atk/atkobject.h>
#include <atk/atkregistry.h>
#include <atk/atkobjectfactory.h>
#include <gtk/gtk.h>
#include <libgail-util/gailtextutil.h>

void eel_accessibility_set_up_label_widget_relation (GtkWidget *label, GtkWidget *widget);

typedef void     (*EelAccessibilityClassInitFn)    (AtkObjectClass *klass);

AtkObject    *eel_accessibility_get_atk_object        (gpointer              object);
AtkObject    *eel_accessibility_for_object            (gpointer              object);
gpointer      eel_accessibility_get_gobject           (AtkObject            *object);
AtkObject    *eel_accessibility_set_atk_object_return (gpointer              object,
						       AtkObject            *atk_object);
GType         eel_accessibility_create_derived_type   (const char           *type_name,
						       GType                 existing_gobject_with_proxy,
						       EelAccessibilityClassInitFn class_init);
void          eel_accessibility_set_name              (gpointer              object,
						       const char           *name);
void          eel_accessibility_set_description       (gpointer              object,
						       const char           *description);

char*         eel_accessibility_text_get_text         (AtkText              *text,
                                                       gint                 start_pos,
                                                       gint                 end_pos);
gunichar      eel_accessibility_text_get_character_at_offset
                                                      (AtkText              *text,
                                                       gint                 offset);
char*         eel_accessibility_text_get_text_before_offset
                                                      (AtkText              *text,
                                                       gint                 offset,
                                                       AtkTextBoundary      boundary_type,
                                                       gint                 *start_offset,
                                                       gint                 *end_offset);
char*         eel_accessibility_text_get_text_at_offset
                                                      (AtkText              *text,
                                                       gint                 offset,
                                                       AtkTextBoundary      boundary_type,
                                                       gint                 *start_offset,
                                                       gint                 *end_offset);
char*         eel_accessibility_text_get_text_after_offset
                                                      (AtkText              *text,
                                                       gint                 offset,
                                                       AtkTextBoundary      boundary_type,
                                                       gint                 *start_offset,
                                                       gint                 *end_offset);
gint          eel_accessibility_text_get_character_count
                                                      (AtkText              *text);

                     
#define EEL_TYPE_ACCESSIBLE_TEXT           (eel_accessible_text_get_type ())
#define EEL_IS_ACCESSIBLE_TEXT(obj)        G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_ACCESSIBLE_TEXT)
#define EEL_ACCESSIBLE_TEXT(obj)           G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_ACCESSIBLE_TEXT, EelAccessibleText)
#define EEL_ACCESSIBLE_TEXT_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), EEL_TYPE_ACCESSIBLE_TEXT, EelAccessibleTextIface))

/* Instead of implementing the AtkText interface, implement this */
typedef struct _EelAccessibleText EelAccessibleText;

typedef struct {
	GTypeInterface parent;
	
	GailTextUtil *(*get_text)   (GObject *text);
	PangoLayout  *(*get_layout) (GObject *text);
} EelAccessibleTextIface;

GType eel_accessible_text_get_type      (void);
void  eel_accessibility_add_simple_text (GType type);

/* From gail - should be unneccessary when AtkObjectFactory is fixed */
#define EEL_ACCESSIBLE_FACTORY(type, factory_name, type_as_function, opt_create_accessible)	\
										\
static GType									\
type_as_function ## _factory_get_accessible_type (void)				\
{										\
  return type;									\
}										\
										\
static AtkObject*								\
type_as_function ## _factory_create_accessible (GObject *obj)			\
{										\
  AtkObject *accessible;							\
										\
  g_assert (G_IS_OBJECT (obj));  						\
										\
  accessible = opt_create_accessible (obj);					\
										\
  return accessible;								\
}										\
										\
static void									\
type_as_function ## _factory_class_init (AtkObjectFactoryClass *klass)		\
{										\
  klass->create_accessible   = type_as_function ## _factory_create_accessible;	\
  klass->get_accessible_type = type_as_function ## _factory_get_accessible_type;\
}										\
										\
static GType									\
type_as_function ## _factory_get_type (void)					\
{										\
  static GType t = 0;								\
										\
  if (!t)									\
  {										\
    static const GTypeInfo tinfo =						\
    {										\
      sizeof (AtkObjectFactoryClass),					\
      NULL, NULL, (GClassInitFunc) type_as_function ## _factory_class_init,			\
      NULL, NULL, sizeof (AtkObjectFactory), 0, NULL, NULL			\
    };										\
										\
    t = g_type_register_static (						\
	    ATK_TYPE_OBJECT_FACTORY, factory_name, &tinfo, 0);			\
  }										\
										\
  return t;									\
}

#define EEL_OBJECT_SET_FACTORY(object_type, type_as_function)			\
	atk_registry_set_factory_type (atk_get_default_registry (),		\
				       object_type,				\
				       type_as_function ## _factory_get_type ())


#endif /* EEL_ACCESSIBILITY_H */
