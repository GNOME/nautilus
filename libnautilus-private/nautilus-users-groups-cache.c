/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-users-groups-cache.c: cache of users' and groups' names.
 
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

#include <config.h>
#include "nautilus-users-groups-cache.h"

#include <glib.h>
#include <grp.h>
#include <pwd.h>


typedef struct _ExpiringCache ExpiringCache;

/* times in milliseconds */
#define USERS_CACHE_EXPIRE_TIME   (60 * 1000)
#define GROUPS_CACHE_EXPIRE_TIME  (60 * 1000)

/* cache of users' names */
static ExpiringCache *users_cache = NULL;

/* cache of groups' names */
static ExpiringCache *groups_cache = NULL;


/**
 * Generic implementation of cache with guint keys and values which expire
 * after specified amount of time.
 */

typedef gpointer (*ExpiringCacheGetValFunc) (guint key);


struct _ExpiringCache
{
	/* Expiration time of cached value */
	time_t expire_time;

	/* Called to obtain a value by a key */
	ExpiringCacheGetValFunc get_value_func;

	/* Called to destroy a value */
	GDestroyNotify value_destroy_func;

	/* Stores cached values */
	GHashTable *cached_values;
};


typedef struct _ExpiringCacheEntry ExpiringCacheEntry;

struct _ExpiringCacheEntry
{
	ExpiringCache *cache;
	guint key;
	gpointer value;
};


static ExpiringCache *
expiring_cache_new (time_t expire_time, ExpiringCacheGetValFunc get_value_func,
                    GDestroyNotify value_destroy_func)
{
	ExpiringCache *cache;

	g_assert (get_value_func != NULL);

	cache = g_new (ExpiringCache, 1);
	cache->expire_time = expire_time;
	cache->get_value_func = get_value_func;
	cache->value_destroy_func = value_destroy_func;
	cache->cached_values = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

	return cache;
}

static ExpiringCacheEntry *
expiring_cache_entry_new (ExpiringCache *cache, guint key, gpointer value)
{
	ExpiringCacheEntry *entry;

	entry = g_slice_new (ExpiringCacheEntry);
	entry->cache = cache;
	entry->key = key;
	entry->value = value;

	return entry;
}

static void
expiring_cache_entry_destroy (ExpiringCacheEntry *entry)
{
	if (entry->cache->value_destroy_func != NULL) {
		entry->cache->value_destroy_func (entry->value);
	}
	g_slice_free (ExpiringCacheEntry, entry);
}

static gboolean
cb_cache_entry_expired (ExpiringCacheEntry *entry)
{
	g_hash_table_remove (entry->cache->cached_values, GSIZE_TO_POINTER (entry->key));
	expiring_cache_entry_destroy (entry);

	return FALSE;
}

static gpointer
expiring_cache_get_value (ExpiringCache *cache, guint key)
{
	ExpiringCacheEntry *entry;

	g_assert (cache != NULL);

	if (!g_hash_table_lookup_extended (cache->cached_values, GSIZE_TO_POINTER (key),
	                                   NULL, (gpointer) &entry)) {
		entry = expiring_cache_entry_new (cache, key, cache->get_value_func (key));
		g_hash_table_insert (cache->cached_values, GSIZE_TO_POINTER (key), entry);
		g_timeout_add (cache->expire_time, (GSourceFunc) cb_cache_entry_expired, entry);
	}

	return entry->value;
}


/*
 * Cache of users' names based on ExpiringCache.
 */

typedef struct _UserInfo UserInfo;

struct _UserInfo {
	char *name;
	char *gecos;
};


static UserInfo *
user_info_new (struct passwd *password_info)
{
	UserInfo *uinfo;

	if (password_info != NULL) {
		uinfo = g_slice_new (UserInfo);
		uinfo->name = g_strdup (password_info->pw_name);
		uinfo->gecos = g_strdup (password_info->pw_gecos);
	} else {
		uinfo = NULL;
	}

	return uinfo;
}

static void
user_info_free (UserInfo *uinfo)
{
	if (uinfo != NULL) {
		g_free (uinfo->name);
		g_free (uinfo->gecos);
		g_slice_free (UserInfo, uinfo);
	}
}

static gpointer
users_cache_get_value (guint key)
{
	return user_info_new (getpwuid (key));
}

static UserInfo *
get_cached_user_info(guint uid)
{
	if (users_cache == NULL) {
		users_cache = expiring_cache_new (USERS_CACHE_EXPIRE_TIME, users_cache_get_value,
		                                  (GDestroyNotify) user_info_free);
	}

	return expiring_cache_get_value (users_cache, uid);
}

/**
 * nautilus_users_cache_get_name:
 *
 * Returns name of user with given uid (using cached data if possible) or
 * NULL in case a user with given uid can't be found.
 *
 * Returns: Newly allocated string or NULL.
 */
char *
nautilus_users_cache_get_name (uid_t uid)
{
	UserInfo *uinfo;

	uinfo = get_cached_user_info (uid);
	if (uinfo != NULL) {
		return g_strdup (uinfo->name);
	} else {
		return NULL;
	}
}

/**
 * nautilus_users_cache_get_gecos:
 *
 * Returns gecos of user with given uid (using cached data if possible) or
 * NULL in case a user with given uid can't be found.
 *
 * Returns: Newly allocated string or NULL.
 */
char *
nautilus_users_cache_get_gecos (uid_t uid)
{
	UserInfo *uinfo;

	uinfo = get_cached_user_info (uid);
	if (uinfo != NULL) {
		return g_strdup (uinfo->gecos);
	} else {
		return NULL;
	}
}

/*
 * Cache of groups' names based on ExpiringCache.
 */

static gpointer
groups_cache_get_value (guint key)
{
	struct group *group_info;

	group_info = getgrgid (GPOINTER_TO_SIZE (key));
	if (group_info != NULL) {
		return g_strdup (group_info->gr_name);
	} else {
		return NULL;
	}
}


/**
 * nautilus_groups_cache_get_name:
 *
 * Returns name of group with given gid (using cached data if possible) or
 * NULL in case a group with given gid can't be found.
 *
 * Returns: Newly allocated string or NULL.
 */
char *
nautilus_groups_cache_get_name (gid_t gid)
{
	if (groups_cache == NULL) {
		groups_cache = expiring_cache_new (GROUPS_CACHE_EXPIRE_TIME, groups_cache_get_value, g_free);
	}

	return g_strdup (expiring_cache_get_value (groups_cache, gid));
}
