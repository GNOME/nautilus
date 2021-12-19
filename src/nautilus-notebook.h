/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *    (ephy-notebook.c)
 *
 *  Copyright © 2008 Free Software Foundation, Inc.
 *    (nautilus-notebook.c)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-window-slot.h"

G_BEGIN_DECLS

int		nautilus_notebook_add_tab	(GtkNotebook *nb,
						 NautilusWindowSlot *slot,
						 int position,
						 gboolean jump_to);
	
void		nautilus_notebook_sync_tab_label (GtkNotebook *nb,
						  NautilusWindowSlot *slot);
void		nautilus_notebook_sync_loading   (GtkNotebook *nb,
						  NautilusWindowSlot *slot);

void		nautilus_notebook_reorder_current_child_relative (GtkNotebook *notebook,
								  int offset);
gboolean        nautilus_notebook_can_reorder_current_child_relative (GtkNotebook *notebook,
								      int offset);
void            nautilus_notebook_prev_page (GtkNotebook *notebook);
void            nautilus_notebook_next_page (GtkNotebook *notebook);

gboolean        nautilus_notebook_contains_slot (GtkNotebook   *notebook,
                                                 NautilusWindowSlot *slot);

gboolean        nautilus_notebook_get_tab_clicked (GtkNotebook *notebook,
                                                   gint         x,
                                                   gint         y,
                                                   gint        *position);
void            nautilus_notebook_setup           (GtkNotebook *notebook);
G_END_DECLS
