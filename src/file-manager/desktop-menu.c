/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Desktop component of GNOME file manager
 * 
 * Copyright (C) 1999, 2000 Red Hat Inc., Free Software Foundation
 * (based on Midnight Commander code by Federico Mena Quintero and Miguel de Icaza)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "desktop-menu.h"

#include <libgnomeui/gnome-popup-menu.h>

static void
exit_cb(GtkWidget *w, gpointer data)
{
  gtk_main_quit();
}

static GnomeUIInfo desktop_popup_items[] = {
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE ("Exit (debug only)", NULL, exit_cb),
	GNOMEUIINFO_END
};

GtkWidget*
desktop_menu_new (void)
{
        GtkWidget *popup;

  	popup = gnome_popup_menu_new (desktop_popup_items);
        
        return popup;
}

