/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view-private.h - Private functions for both the list and search list
   view to share

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Rebecca Schulman <rebecka@mit.edu>

*/

guint             fm_list_view_get_icon_size               (FMListView         *list_view);
void              fm_list_view_install_row_images          (FMListView         *list_view,
							    guint               row);
void		  fm_list_view_setup_list                  (FMListView         *list_view);
gboolean          fm_list_view_list_is_instantiated        (FMListView         *list_view);
void              fm_list_view_set_instantiated            (FMListView         *list_view);
