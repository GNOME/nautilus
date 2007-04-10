/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-users-groups-cache.h: cache of users' and groups' names.
 
   Copyright (C) 2006 Zbigniew Chyla
  
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
  
   Author: Zbigniew Chyla <mail@zbigniew.chyla.pl>
*/

#ifndef NAUTILUS_USERS_GROUPS_CACHE_H
#define NAUTILUS_USERS_GROUPS_CACHE_H

#include <sys/types.h>

char *nautilus_users_cache_get_name (uid_t uid);
char *nautilus_users_cache_get_gecos (uid_t uid);
char *nautilus_groups_cache_get_name (gid_t gid);

#endif /* NAUTILUS_USERS_GROUPS_CACHE_H */
