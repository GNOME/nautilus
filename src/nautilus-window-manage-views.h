/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef NAUTILUS_WINDOW_MANAGE_VIEWS_H
#define NAUTILUS_WINDOW_MANAGE_VIEWS_H

#include "nautilus-window.h"
#include "nautilus-navigation-window.h"

void                    nautilus_window_manage_views_destroy         (NautilusWindow         *window);
void                    nautilus_window_manage_views_finalize        (NautilusWindow         *window);
void                    nautilus_window_open_location                (NautilusWindow         *window,
                                                                      const char             *location);
void                    nautilus_window_open_location_with_selection (NautilusWindow         *window,
                                                                      const char             *location,
                                                                      GList                  *selection);
void                    nautilus_window_stop_loading                 (NautilusWindow         *window);
void                    nautilus_window_set_content_view             (NautilusWindow         *window,
                                                                      NautilusViewIdentifier *id);

gboolean                nautilus_window_content_view_matches_iid     (NautilusWindow         *window,
                                                                      const char             *iid);
NautilusViewIdentifier *nautilus_window_get_content_view_id          (NautilusWindow         *window);
void                    nautilus_window_connect_extra_view           (NautilusWindow         *window, 
                                                                      NautilusViewFrame      *view,
                                                                      NautilusViewIdentifier *id);
void                    nautilus_window_disconnect_extra_view        (NautilusWindow         *window, 
                                                                      NautilusViewFrame      *view);
char                   *nautilus_window_get_view_frame_label         (NautilusViewFrame      *view);
                                                                      
void                    nautilus_navigation_window_set_sidebar_panels           (NautilusNavigationWindow         *window,
                                                                                 GList                  *view_identifier_list);
void                    nautilus_navigation_window_back_or_forward              (NautilusNavigationWindow         *window,
                                                                                 gboolean                back,
                                                                                 guint                   distance);

#endif /* NAUTILUS_WINDOW_MANAGE_VIEWS_H */
