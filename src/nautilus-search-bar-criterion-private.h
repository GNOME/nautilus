/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-bar-criterion-private.h - Code to bring up
   the various kinds of criterion supported in the nautilus search
   bar 

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

#ifndef NAUTILUS_SEARCH_BAR_CRITERION_PRIVATE_H
#define NAUTILUS_SEARCH_BAR_CRITERION_PRIVATE_H

#include "nautilus-complex-search-bar.h"
#include "nautilus-search-bar-criterion.h"
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkoptionmenu.h>
#include <time.h>
#include <libgnomeui/gnome-dateedit.h>

struct NautilusSearchBarCriterionDetails {
	NautilusSearchBarCriterionType type;

	/* The container for the widgets */
	GtkWidget *box;

	/* various widgets held by the criterion */
	GtkOptionMenu *available_criteria;
	GtkOptionMenu *relation_menu;
	gboolean use_value_entry;
	NautilusEntry *value_entry;
	gboolean use_value_menu;
	GtkOptionMenu *value_menu;
	gboolean use_value_suffix;
	GtkLabel *value_suffix;
	GnomeDateEdit *date;

	/* Keep a pointer to the whole bar, so we know what other
	   criteria are currently chosen, and therefore what types are
	   valid to change to */
	NautilusComplexSearchBar *bar;

};

#endif /* NAUTILUS_SEARCH_BAR_CRITERION_PRIVATE_H */
