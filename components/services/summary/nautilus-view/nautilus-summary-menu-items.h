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
 * Authors: J Shane Culpepper
 */

#ifndef NAUTILUS_SUMMARY_MENU_ITEMS_H
#define NAUTILUS_SUMMARY_MENU_ITEMS_H

void	merge_bonobo_menu_items		(BonoboControl		*control,
					 gboolean		state,
					 gpointer		user_data);
void	update_menu_items		(NautilusSummaryView	*view,
					 gboolean		logged_in);

#endif /* NAUTILUS_SUMMARY_MENU_ITEMS_H */

