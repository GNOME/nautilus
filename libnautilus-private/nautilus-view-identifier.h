/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-view-identifier.h: Unique ID/Human-readable name pairs for views
 
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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Maciej Stachowiak <mjs@eazel.com>
*/

#ifndef NAUTILUS_VIEW_IDENTIFIER_H
#define NAUTILUS_VIEW_IDENTIFIER_H

#include <liboaf/liboaf.h>

typedef struct {
	char *iid;	      /* Unique ID */
	char *name;	      /* human-readable name */
	char *view_as_label;  /* "View as <name>" */
	char *viewer_label;   /* "<name> Viewer" */
} NautilusViewIdentifier;

NautilusViewIdentifier *nautilus_view_identifier_new_from_oaf_server_info (OAF_ServerInfo               *server,
									   char                         *name_attribute);
NautilusViewIdentifier *nautilus_view_identifier_new_from_content_view    (OAF_ServerInfo               *server);
NautilusViewIdentifier *nautilus_view_identifier_new_from_sidebar_panel   (OAF_ServerInfo               *server);
NautilusViewIdentifier *nautilus_view_identifier_new_from_property_page   (OAF_ServerInfo               *server);
NautilusViewIdentifier *nautilus_view_identifier_copy                     (const NautilusViewIdentifier *identifier);
void                    nautilus_view_identifier_free                     (NautilusViewIdentifier       *identifier);
int                     nautilus_view_identifier_compare                  (const NautilusViewIdentifier *a,
									   const NautilusViewIdentifier *b);
/* lists of NautilusViewIdentifier */
GList *                 nautilus_view_identifier_list_copy                (GList                        *list);
void                    nautilus_view_identifier_list_free                (GList                        *list);

#endif /* NAUTILUS_VIEW_IDENTIFIER */
