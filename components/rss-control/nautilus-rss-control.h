/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Andy Hertzfeld
 */

/* header file for the rss control component */

#ifndef NAUTILUS_RSS_CONTROL_H
#define NAUTILUS_RSS_CONTROL_H

#include <bonobo.h>
#include <gtk/gtkeventbox.h> 
typedef struct _NautilusRSSControl      NautilusRSSControl;
typedef struct _NautilusRSSControlClass NautilusRSSControlClass;

#define NAUTILUS_TYPE_RSS_CONTROL	    (nautilus_rss_control_get_type ())
#define NAUTILUS_RSS_CONTROL(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_RSS_CONTROL, NautilusRSSControl))
#define NAUTILUS_RSS_CONTROL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_RSS_CONTROL, NautilusRSSControlClass))
#define NAUTILUS_IS_RSS_CONTROL(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_RSS_CONTROL))
#define NAUTILUS_IS_RSS_CONTROL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_RSS_CONTROL))

typedef struct _NautilusRSSControlDetails NautilusRSSControlDetails;

struct _NautilusRSSControl {
	GtkEventBox parent;
	NautilusRSSControlDetails *details;
};

struct _NautilusRSSControlClass {
	GtkEventBoxClass parent_class;
};

/* GtkObject support */
GtkType	nautilus_rss_control_get_type (void);
BonoboObject*	nautilus_rss_control_get_control (NautilusRSSControl *rss_control);
		
#endif /* NAUTILUS_RSS_CONTROL_H */
