/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-switchable-navigation-bar.h - Navigation bar for Nautilus
 * that allows switching between the location bar and the search bar
 */

#ifndef NAUTILUS_SWITCHABLE_NAVIGATION_BAR_H
#define NAUTILUS_SWITCHABLE_NAVIGATION_BAR_H

#include "nautilus-navigation-bar.h"
#include <gtk/gtkhbox.h>
#include "nautilus-location-bar.h"
#include "nautilus-search-bar.h"

#define NAUTILUS_TYPE_SWITCHABLE_NAVIGATION_BAR (nautilus_switchable_navigation_bar_get_type ())
#define NAUTILUS_SWITCHABLE_NAVIGATION_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_SWITCHABLE_NAVIGATION_BAR, NautilusSwitchableNavigationBar)
#define NAUTILUS_SWITCHABLE_NAVIGATION_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_SWITCHABLE_NAVIGATION_BAR, NautilusSwitchableNavigationBarClass)
#define NAUTILUS_IS_SWITCHABLE_NAVIGATION_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_SWITCHABLE_NAVIGATION_BAR)

typedef struct NautilusSwitchableNavigationBarDetails NautilusSwitchableNavigationBarDetails;

typedef enum {
	NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION,
	NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH
} NautilusSwitchableNavigationBarMode;


typedef struct NautilusSwitchableNavigationBar {
	NautilusNavigationBar parent;
	NautilusSwitchableNavigationBarDetails *details;
} NautilusSwitchableNavigationBar;

typedef struct {
	NautilusNavigationBarClass parent_class;

	void         (*mode_changed)     (NautilusSwitchableNavigationBar *switchable_navigation_bar,
					  NautilusSwitchableNavigationBarMode mode);
} NautilusSwitchableNavigationBarClass;

GtkType    			    nautilus_switchable_navigation_bar_get_type	(void);
GtkWidget* 			    nautilus_switchable_navigation_bar_new     	(NautilusWindow *window);
NautilusSwitchableNavigationBarMode nautilus_switchable_navigation_bar_get_mode (NautilusSwitchableNavigationBar     *switchable_navigation_bar);
void       			    nautilus_switchable_navigation_bar_set_mode (NautilusSwitchableNavigationBar     *switchable_navigation_bar,
								 		 NautilusSwitchableNavigationBarMode  mode);
void				    nautilus_switchable_navigation_bar_activate (NautilusSwitchableNavigationBar     *switchable_navigation_bar);

#endif /* NAUTILUS_SWITCHABLE_NAVIGATION_BAR_H */
