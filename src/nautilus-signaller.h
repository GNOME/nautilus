/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-signaller.h: Class to manage nautilus-wide signals that don't
   correspond to any particular object.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_SIGNALLER_H
#define NAUTILUS_SIGNALLER_H

#include <gtk/gtkobject.h>

/* NautilusSignaller is a class that manages signals between
   disconnected file manager code. File manager objects connect to these signals
   so that other objects can cause them to be emitted later, without
   the connecting and emit-causing objects needing to know about each
   other. It seems a shame to have to invent a subclass and a special
   object just for this purpose. Perhaps there's a better way to do 
   this kind of thing.
*/

typedef struct _NautilusSignaller NautilusSignaller;
typedef struct _NautilusSignallerClass NautilusSignallerClass;

#define NAUTILUS_TYPE_SIGNALLER \
	(nautilus_signaller_get_type ())
#define NAUTILUS_SIGNALLER(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SIGNALLER, NautilusSignaller))
#define NAUTILUS_SIGNALLER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SIGNALLER, NautilusSignallerClass))
#define NAUTILUS_IS_SIGNALLER(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SIGNALLER))
#define NAUTILUS_IS_SIGNALLER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SIGNALLER))

struct _NautilusSignaller
{
	GtkObject object;
};

struct _NautilusSignallerClass
{
	GtkObjectClass parent_class;

	void (* icon_text_changed) (NautilusSignaller *signaller);
};

/* Basic GtkObject requirements. */
GtkType            nautilus_signaller_get_type            (void);

/* Get the one and only NautilusSignaller to connect with or emit signals for */
NautilusSignaller *nautilus_signaller_get_current	  (void);

#endif /* NAUTILUS_SIGNALLER_H */
