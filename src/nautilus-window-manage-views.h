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

void                    nautilus_window_manage_views_destroy          (NautilusWindow           *window);
void                    nautilus_window_manage_views_finalize         (NautilusWindow           *window);
void                    nautilus_window_open_location                 (NautilusWindow           *window,
                                                                       GFile                    *location,
                                                                       gboolean                  close_behind);
void                    nautilus_window_open_location_with_selection  (NautilusWindow           *window,
                                                                       GFile                    *location,
                                                                       GList                    *selection,
                                                                       gboolean                  close_behind);
void                    nautilus_window_open_location_full            (NautilusWindow           *window,
                                                                       GFile                    *location,
                                                                       NautilusWindowOpenMode    mode,
                                                                       NautilusWindowOpenFlags   flags,
                                                                       GList                    *new_selection);
void                    nautilus_window_stop_loading                  (NautilusWindow           *window);
void                    nautilus_window_set_content_view              (NautilusWindow           *window,
                                                                       const char               *id);
gboolean                nautilus_window_content_view_matches_iid      (NautilusWindow           *window,
                                                                       const char               *iid);
const char             *nautilus_window_get_content_view_id           (NautilusWindow           *window);
char                   *nautilus_window_get_view_error_label          (NautilusWindow           *window);
char                   *nautilus_window_get_view_startup_error_label  (NautilusWindow           *window);
void                    nautilus_navigation_window_set_sidebar_panels (NautilusNavigationWindow *window,
                                                                       GList                    *view_identifier_list);


/* NautilusWindowInfo implementation: */
void nautilus_window_report_load_underway     (NautilusWindow     *window,
                                               NautilusView       *view);
void nautilus_window_report_selection_changed (NautilusWindowInfo *window);
void nautilus_window_report_view_failed       (NautilusWindow     *window,
                                               NautilusView       *view);
void nautilus_window_report_load_complete     (NautilusWindow     *window,
                                               NautilusView       *view);

#endif /* NAUTILUS_WINDOW_MANAGE_VIEWS_H */
