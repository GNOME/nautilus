/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-directory-protected.h: GNOME file manager directory model,
   details for child classes only.
 
   Copyright (C) 1999 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#ifndef FM_DIRECTORY_PROTECTED_H
#define FM_DIRECTORY_PROTECTED_H

#include "fm-directory.h"

/* The word "protected" in the name of this file means that these are details of the
   FMDirectory class that need to be known to concrete child classes of FMDirectory,
   but not to clients of FMDirectory.

   The terminology is stolen from C++.
*/

struct _FMDirectoryDetails
{
	char *hash_table_key; /* Could change this "URI" if we want to use it that way. */
};

struct _FMFile
{
};

#endif /* FM_DIRECTORY_PROTECTED_H */
