/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

#ifndef NAUTILUS_WINDOW_MANAGE_VIEWS_H
#define NAUTILUS_WINDOW_MANAGE_VIEWS_H

#include "nautilus-window.h"

void               nautilus_window_open_location               (NautilusWindow          *window,
                                                                const char              *location,
                                                                NautilusViewFrame       *view);
void               nautilus_window_open_location_in_new_window (NautilusWindow          *window,
                                                                const char              *location,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_location_change      (NautilusWindow          *window,
                                                                const char              *location,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_selection_change     (NautilusWindow          *window,
                                                                GList                   *selection,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_status               (NautilusWindow          *window,
                                                                const char              *status,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_load_underway        (NautilusWindow          *window,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_load_progress        (NautilusWindow          *window,
                                                                double                   fraction_done,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_load_complete        (NautilusWindow          *window,
                                                                NautilusViewFrame       *view);
void               nautilus_window_report_load_failed          (NautilusWindow          *window,
                                                                NautilusViewFrame       *view);
void               nautilus_window_set_title                   (NautilusWindow          *window,
                                                                const char              *new_title,
                                                                NautilusViewFrame       *view);
NautilusViewFrame *nautilus_window_load_content_view           (NautilusWindow          *window,
                                                                NautilusViewIdentifier  *id,
                                                                NautilusViewFrame      **requesting_view);

#endif /* NAUTILUS_WINDOW_MANAGE_VIEWS_H */
