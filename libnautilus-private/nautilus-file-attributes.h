/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-file-attributes.h: #defines and other file-attribute-related info
 
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_FILE_ATTRIBUTES_H
#define NAUTILUS_FILE_ATTRIBUTES_H

/* Names for NautilusFile attributes. These are used when registering
 * interest in changes to the attributes or when waiting for them.
 */

#define NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI              "activation URI"
#define NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES                "capabilities"
#define NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON                 "custom icon"
#define NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS                 "deep counts"
#define NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT        "directory item count"
#define NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES   "directory item MIME types"
#define NAUTILUS_FILE_ATTRIBUTE_FILE_TYPE                   "file type"
#define NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY                "is directory"
#define NAUTILUS_FILE_ATTRIBUTE_METADATA                    "metadata"
#define NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE                   "MIME type"
#define NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT               "top left text"
#define NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME                "display name"

#endif /* NAUTILUS_FILE_ATTRIBUTES_H */
