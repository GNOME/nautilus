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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* main.c - Main function and object activation function for sample
 * view component.
 */

/* WHAT YOU NEED TO CHANGE: You need to change include
 * component-specific header below the include of config.h. Then look
 * for the CHANGE comments below and change the #defines.
 */

#include <config.h>

#include "nautilus-sample-content-view.h"
#include <libnautilus/nautilus-view-standard-main.h>

/* CHANGE: Replace these OAFIIDs with the new ones you put in the
 * .oafinfo file. 
 */
#define FACTORY_IID     "OAFIID:nautilus_sample_content_view_factory:3df6b028-be44-4a18-95c3-7720f50ca0c5"
#define VIEW_IID        "OAFIID:nautilus_sample_content_view:45c746bc-7d64-4346-90d5-6410463b43ae"

/* CHANGE: Change to your executable name */
#define EXECUTABLE_NAME "nautilus-sample-content-view"

/* CHANGE: Change to the get_type function for your view class */
#define GET_TYPE_FUNCTION nautilus_sample_content_view_get_type

int
main (int argc, char *argv[])
{
	return nautilus_view_standard_main (EXECUTABLE_NAME,
					    VERSION,
					    NULL,	/* Could be PACKAGE */
					    NULL,	/* Could be GNOMELOCALEDIR */
					    argc,
					    argv,
					    FACTORY_IID,
					    VIEW_IID,
					    nautilus_view_create_from_get_type_function,
					    NULL,
					    GET_TYPE_FUNCTION);
}
