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

#ifndef NTL_WINDOW_MSGS_H
#define NTL_WINDOW_MSGS_H

#include "ntl-window.h"
#include "ntl-content-view.h"

void nautilus_window_request_location_change  (NautilusWindow                 *window,
                                               Nautilus_NavigationRequestInfo *loc,
                                               NautilusViewFrame              *requesting_view);
void nautilus_window_request_selection_change (NautilusWindow                 *window,
                                               Nautilus_SelectionRequestInfo  *loc,
                                               NautilusViewFrame              *requesting_view);
void nautilus_window_request_status_change    (NautilusWindow                 *window,
                                               Nautilus_StatusRequestInfo     *loc,
                                               NautilusViewFrame              *requesting_view);
void nautilus_window_request_progress_change  (NautilusWindow                 *window,
                                               Nautilus_ProgressRequestInfo   *loc,
                                               NautilusViewFrame              *requesting_view);
void nautilus_window_request_title_change     (NautilusWindow                 *window,
                                               const char                     *new_title,
                                               NautilusContentViewFrame       *requesting_view);

#endif /* NTL_WINDOW_MSGS_H */
