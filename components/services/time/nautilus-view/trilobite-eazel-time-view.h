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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* trilobite-eazel-time-view.h - sample time service nautilus view.*/

#ifndef TRILOBITE_EAZEL_TIME_VIEW_H
#define TRILOBITE_EAZEL_TIME_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtkeventbox.h>

#define OAFIID_TRILOBITE_EAZEL_TIME_VIEW "OAFIID:trilobite_eazel_time_view:de32d812-af19-4359-9902-42318e0089b3"
#define OAFIID_TRILOBITE_EAZEL_TIME_VIEW_FACTORY "OAFIID:trilobite_eazel_time_view_factory:9797487c-08f7-4ef1-9981-0c4b36df220b"

#define TRILOBITE_TYPE_EAZEL_TIME_VIEW	          (trilobite_eazel_time_view_get_type ())
#define TRILOBITE_EAZEL_TIME_VIEW(obj)	          (GTK_CHECK_CAST ((obj), TRILOBITE_TYPE_EAZEL_TIME_VIEW, TrilobiteEazelTimeView))
#define TRILOBITE_EAZEL_TIME_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TRILOBITE_TYPE_EAZEL_TIME_VIEW, TrilobiteEazelTimeViewClass))
#define TRILOBITE_IS_EAZEL_TIME_VIEW(obj)	  (GTK_CHECK_TYPE ((obj), TRILOBITE_TYPE_EAZEL_TIME_VIEW))
#define TRILOBITE_IS_EAZEL_TIME_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TRILOBITE_TYPE_EAZEL_TIME_VIEW))

typedef struct TrilobiteEazelTimeViewDetails TrilobiteEazelTimeViewDetails;

typedef struct {
	GtkEventBox parent;
	TrilobiteEazelTimeViewDetails *details;
} TrilobiteEazelTimeView;

typedef struct {
	GtkEventBoxClass parent_class;
} TrilobiteEazelTimeViewClass;

/* GtkObject support */
GtkType       trilobite_eazel_time_view_get_type          (void);

/* Component embedding support */
NautilusView *trilobite_eazel_time_view_get_nautilus_view (TrilobiteEazelTimeView *view);


/* URI handling */
void          trilobite_eazel_time_view_load_uri          (TrilobiteEazelTimeView *view,
							      const char                *uri);

#endif /* TRILOBITE_EAZEL_TIME_VIEW_H */
