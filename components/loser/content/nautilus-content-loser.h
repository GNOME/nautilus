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

/* nautilus-content-loser.h - Nautilus content view component that
   fails on command. See the README in the `loser' directory to see
   how to control the type of failure. */

#ifndef NAUTILUS_CONTENT_LOSER_H
#define NAUTILUS_CONTENT_LOSER_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtklabel.h>

typedef struct NautilusContentLoser      NautilusContentLoser;
typedef struct NautilusContentLoserClass NautilusContentLoserClass;

#define NAUTILUS_TYPE_CONTENT_LOSER	      (nautilus_content_loser_get_type ())
#define NAUTILUS_CONTENT_LOSER(obj)	      (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CONTENT_LOSER, NautilusContentLoser))
#define NAUTILUS_CONTENT_LOSER_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONTENT_LOSER, NautilusContentLoserClass))
#define NAUTILUS_IS_CONTENT_LOSER(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CONTENT_LOSER))
#define NAUTILUS_IS_CONTENT_LOSER_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CONTENT_LOSER))

typedef struct NautilusContentLoserDetails NautilusContentLoserDetails;

struct NautilusContentLoser {
	GtkLabel parent;
	NautilusContentLoserDetails *details;
};

struct NautilusContentLoserClass {
	GtkLabelClass parent_class;
};

/* GtkObject support */
GtkType       nautilus_content_loser_get_type          (void);

/* Component embedding support */
NautilusView *nautilus_content_loser_get_nautilus_view (NautilusContentLoser *view);

/* URI handling */
void          nautilus_content_loser_load_uri          (NautilusContentLoser *view,
							const char           *uri);

/* Failing on command. */
void          nautilus_content_loser_maybe_fail        (const char           *location);

#endif /* NAUTILUS_CONTENT_LOSER_H */

