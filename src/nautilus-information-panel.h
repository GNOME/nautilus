/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 *  This is the header file for the index panel widget, which displays overview information
 *  in a vertical panel and hosts the meta-views.
 */

#ifndef NAUTILUS_INFORMATION_PANEL_H
#define NAUTILUS_INFORMATION_PANEL_H

#include <eel/eel-background-box.h>

#define NAUTILUS_TYPE_INFORMATION_PANEL \
	(nautilus_information_panel_get_type ())
#define NAUTILUS_INFORMATION_PANEL(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_INFORMATION_PANEL, NautilusInformationPanel))
#define NAUTILUS_INFORMATION_PANEL_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_INFORMATION_PANEL, NautilusInformationPanelClass))
#define NAUTILUS_IS_INFORMATION_PANEL(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_INFORMATION_PANEL))
#define NAUTILUS_IS_INFORMATION_PANEL_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_INFORMATION_PANEL))

typedef struct NautilusInformationPanelDetails NautilusInformationPanelDetails;

#define NAUTILUS_INFORMATION_PANEL_ID "NautilusInformationPanel"

typedef struct {
	EelBackgroundBox parent_slot;
	NautilusInformationPanelDetails *details;
} NautilusInformationPanel;

typedef struct {
	EelBackgroundBoxClass parent_slot;
	
	void (*location_changed) (NautilusInformationPanel *information_panel,
				  const char *location);
} NautilusInformationPanelClass;

GType            nautilus_information_panel_get_type     (void);
void             nautilus_information_panel_register     (void);

#endif /* NAUTILUS_INFORMATION_PANEL_H */
