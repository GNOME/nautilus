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

#ifndef EAZEL_SUMMARY_SHARED_H
#define EAZEL_SUAMMRY_SHARED_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtk.h>

gboolean http_fetch_remote_file (char *uri, const char *target_file);

gboolean parse_summary_configuration (const char *summary_xml_file);

#endif /* EAZEL_SUMMARY_SHARED_H */

