/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-signaller.h: Class to manage file-manager-wide signals.
 
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

#ifndef FM_SIGNALLER_H
#define FM_SIGNALLER_H

#include <gtk/gtkobject.h>

/* FMSignaller is a class that manages signals between
   disconnected file manager code. File manager objects connect to these signals
   so that other objects can cause them to be emitted later, without
   the connecting and emit-causing objects needing to know about each
   other. It seems a shame to have to invent a subclass and a special
   object just for this purpose. Perhaps there's a better way to do 
   this kind of thing.
*/

typedef struct _FMSignaller FMSignaller;
typedef struct _FMSignallerClass FMSignallerClass;

#define FM_TYPE_SIGNALLER \
	(fm_signaller_get_type ())
#define FM_SIGNALLER(obj) \
	(GTK_CHECK_CAST ((obj), FM_TYPE_SIGNALLER, FMSignaller))
#define FM_SIGNALLER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_SIGNALLER, FMSignallerClass))
#define FM_IS_SIGNALLER(obj) \
	(GTK_CHECK_TYPE ((obj), FM_TYPE_SIGNALLER))
#define FM_IS_SIGNALLER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_SIGNALLER))

struct _FMSignaller
{
	GtkObject object;
};

struct _FMSignallerClass
{
	GtkObjectClass parent_class;

	void (* icon_text_changed) (FMSignaller *signaller);
};

/* Basic GtkObject requirements. */
GtkType            fm_signaller_get_type            (void);

/* Get the one and only FMSignaller to connect with or emit signals for */
FMSignaller *fm_signaller_get_current	  (void);

#endif /* FM_SIGNALLER_H */
