
/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-complex-search-bar.h - Search object
   containing multiple choosable attributes

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Rebecca Schulman <rebecka@eazel.com>
*/

#ifndef NAUTILUS_COMPLEX_SEARCH_BAR_H
#define NAUTILUS_COMPLEX_SEARCH_BAR_H

#include "nautilus-search-bar.h"

#define NAUTILUS_TYPE_COMPLEX_SEARCH_BAR (nautilus_complex_search_bar_get_type ())
#define NAUTILUS_COMPLEX_SEARCH_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_COMPLEX_SEARCH_BAR, NautilusComplexSearchBar)
#define NAUTILUS_COMPLEX_SEARCH_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_COMPLEX_SEARCH_BAR, NautilusComplexSearchBarClass)
#define NAUTILUS_IS_COMPLEX_SEARCH_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_COMPLEX_SEARCH_BAR)

typedef struct NautilusComplexSearchBarDetails  NautilusComplexSearchBarDetails;

typedef struct {
	NautilusSearchBar parent_slot;
	NautilusComplexSearchBarDetails *details;
} NautilusComplexSearchBar;

typedef struct {
	NautilusSearchBarClass parent_slot;
} NautilusComplexSearchBarClass;

GtkType    nautilus_complex_search_bar_get_type (void);
GtkWidget *nautilus_complex_search_bar_new      (void);



#endif /* NAUTILUS_COMPLEX_SEARCH_BAR_H */










