/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-scroll-positionable.c - public interface for objects that implement
 *                                  scroll positioning
 *
 * Copyright (C) 2003 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "nautilus-scroll-positionable.h"

#include <config.h>
#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <eel/eel-marshal.h>

enum {
	GET_FIRST_VISIBLE_FILE,
	SCROLL_TO_FILE,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

BONOBO_CLASS_BOILERPLATE_FULL (NautilusScrollPositionable, nautilus_scroll_positionable, Nautilus_ScrollPositionable,
			       BonoboObject, BONOBO_OBJECT_TYPE)


static Nautilus_URI
impl_Nautilus_Scroll_Positionable_get_first_visible_file (PortableServer_Servant  servant,
							  CORBA_Environment      *ev)
{
	NautilusScrollPositionable *positionable;
	char *uri;
	char *ret;

	positionable = NAUTILUS_SCROLL_POSITIONABLE (bonobo_object_from_servant (servant));

	uri = NULL;
	g_signal_emit (G_OBJECT (positionable), signals [GET_FIRST_VISIBLE_FILE],
		       0, &uri);
	if (uri) {
		ret = CORBA_string_dup (uri);
		g_free (uri);
	} else {
		ret = CORBA_string_dup ("");
	}
	
	return ret;
}

static void 
impl_Nautilus_Scroll_Positionable_scroll_to_file (PortableServer_Servant  servant,
						  const CORBA_char       *uri,
						  CORBA_Environment      *ev)
{
	NautilusScrollPositionable *positionable;

	positionable = NAUTILUS_SCROLL_POSITIONABLE (bonobo_object_from_servant (servant));

	g_signal_emit (G_OBJECT (positionable),
		       signals [SCROLL_TO_FILE], 0,
		       uri);
}



static char *
nautilus_scroll_positionable_get_first_visible_file  (NautilusScrollPositionable *positionable)
{
  return NULL;
}

static void
nautilus_scroll_positionable_instance_init (NautilusScrollPositionable *positionable)
{
}

static void
nautilus_scroll_positionable_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
single_string_accumulator (GSignalInvocationHint *ihint,
                           GValue                *return_accu,
                           const GValue          *handler_return,
                           gpointer               dummy)
{
  gboolean continue_emission;
  const gchar *str;
  
  str = g_value_get_string (handler_return);
  g_value_set_string (return_accu, str);
  continue_emission = str == NULL;
  
  return continue_emission;
}

static void
nautilus_scroll_positionable_class_init (NautilusScrollPositionableClass *klass)
{
	POA_Nautilus_ScrollPositionable__epv *epv = &klass->epv;
	GObjectClass *object_class;
	
	object_class = (GObjectClass *) klass;
	
	klass->get_first_visible_file = nautilus_scroll_positionable_get_first_visible_file;
	
	object_class->finalize = nautilus_scroll_positionable_finalize;
				    
	signals [GET_FIRST_VISIBLE_FILE] =
		g_signal_new ("get_first_visible_file",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusScrollPositionableClass, get_first_visible_file),
			      single_string_accumulator, NULL,
			      eel_marshal_STRING__VOID,
			      G_TYPE_STRING, 0);
	signals [SCROLL_TO_FILE] =
		g_signal_new ("scroll_to_file",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusScrollPositionableClass, scroll_to_file),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	epv->scroll_to_file = impl_Nautilus_Scroll_Positionable_scroll_to_file;
	epv->get_first_visible_file = impl_Nautilus_Scroll_Positionable_get_first_visible_file;
}

NautilusScrollPositionable *
nautilus_scroll_positionable_new (void)
{
	return g_object_new (nautilus_scroll_positionable_get_type (), NULL);
}
