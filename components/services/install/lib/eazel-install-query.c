/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 1998-1999 James Henstridge
 * Copyright (C) 1998 Red Hat Software, Inc.
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
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 */

#include <eazel-install-query.h>
#include <eazel-install-public.h>
#include <eazel-install-private.h>
#include <ctype.h>
#include <stdarg.h>

extern gboolean eazel_install_prepare_package_system (EazelInstall *service);
extern gboolean eazel_install_free_package_system (EazelInstall *service);
extern int eazel_install_package_name_compare (PackageData *pack, char *name);

/*****************************************************************************/
/* Query mechanisms                                                          */
/*****************************************************************************/

/*

  query syntax :
  " '<package>'.[<attr><op>'<arg>']*

  attr and op pairs :
  -------------------
  version = < > <= >=
  arch    =
  
  query examples:
  "'gnome-core'" check for package gnome-core
  "'nautilus'.version>='0.1'.arch=i386" Check for nautilus, i386 binary version >= 0.1
  "'popt'.version='1.5'" check for popt version 1.5 (no more, no less)

*/  

typedef enum {
	EI_Q_VERSION,
	EI_Q_ARCH
} AttributeEnum;

typedef enum {
	EI_OP_EQ = 0x01,
	EI_OP_LT = 0x02,
	EI_OP_GT = 0x04,
	EI_OP_NEG = 0x08,
} AttributeOpEnum;

typedef struct {
	AttributeEnum attrib;
	AttributeOpEnum op;
	char *arg;
} AttributeOpArg;


static void
eazel_install_simple_rpm_query (EazelInstall *service, 
				const char *input, 
				EazelInstallSimpleQueryEnum flag,
				const char *root,
				GList **result)
{
	dbiIndexSet matches;
	gboolean close_db;
	rpmdb db;
	int rc;
	int i;
	
	close_db = FALSE;

	/* If db is not open, this will be false, therefore, 
	   open and close at the end. That way, this
	   func can be used in both various enviroments */
	if (g_hash_table_size (service->private->packsys.rpm.dbs) == 0) {
		eazel_install_prepare_package_system (service);
		close_db = TRUE;
	} 

	g_message ("Querying for %s in %s", input, root);

	rc = -1;
	db = (rpmdb)g_hash_table_lookup (service->private->packsys.rpm.dbs, root);	
	g_assert (db);

	switch (flag) {
	case EI_SIMPLE_QUERY_OWNS:		
		rc = rpmdbFindByFile (db, input, &matches);
		break;
	case EI_SIMPLE_QUERY_PROVIDES:		
		rc = rpmdbFindByProvides (db, input, &matches);
		break;
	case EI_SIMPLE_QUERY_REQUIRES:
		rc = rpmdbFindByRequiredBy (db, input, &matches);
		break;
	case EI_SIMPLE_QUERY_MATCHES:
		rc = rpmdbFindPackage (db, input, &matches);
		break;
	default:
		g_warning ("Unknown query");
	}
	
	if (rc != 0) {
		return;
	} 
	
	for (i = 0; i < dbiIndexSetCount (matches); i++) {
		unsigned int offset;
		Header *hd;
		PackageData *pack;
		
		offset = dbiIndexRecordOffset (matches, i);
		hd = g_new0 (Header,1);
		(*hd) = rpmdbGetRecord (db, offset);
		pack = packagedata_new_from_rpm_header (hd);
		if (g_list_find_custom (*result, pack->name, (GCompareFunc)eazel_install_package_name_compare)!=NULL) {
			packagedata_destroy (pack);
		} else {
			(*result) = g_list_prepend (*result, pack);
		}
	}
	

	if (close_db) {
		eazel_install_free_package_system (service);
	}
}


GList* 
eazel_install_simple_query (EazelInstall *service, 
			    const char *input, 
			    EazelInstallSimpleQueryEnum flag, 
			    int neglist_count, 
			    ...)
{
	GList *result;
	GList *remove;
	GList *iterator;
	GList *root_dirs;
	GHashTable *names_to_ignore;

	if (neglist_count) {
		int i;
		va_list va;

		va_start (va, neglist_count);
		names_to_ignore = g_hash_table_new (g_str_hash, g_str_equal);

		/* for all neglists, collect the package->names */
		for (i = 0; i<neglist_count; i++) {
			GList *neglist;
			neglist = va_arg (va, GList*);
			for (iterator = neglist; iterator; iterator = iterator->next) {
				PackageData *pack;
				pack = (PackageData*)iterator->data;
				g_hash_table_insert (names_to_ignore, pack->name, pack);
			}
		}
	}

	result = NULL;

	/* query in one root ? */
	if (service->private->cur_root) {
		root_dirs = g_list_prepend (NULL, service->private->cur_root);
	} else {
		root_dirs = g_list_copy (eazel_install_get_root_dirs (service));
	}

	{
		/* Now query all the set roots */
		GList *iterator;
		for (iterator = root_dirs; iterator; iterator=iterator->next) {
			char *root_dir = (char*)iterator->data;
			/* Do the query depending on package system */
			switch (eazel_install_get_package_system (service)) {
			case EAZEL_INSTALL_USE_RPM:
				eazel_install_simple_rpm_query (service, input, flag, root_dir, &result);
				break;
			}
			
		}
	}
	g_list_free (root_dirs);

	/* Now strip the packages to ignore */
	if (neglist_count) {
		remove = NULL;

		/* Collect the packages in "remove" */
		for (iterator = result; iterator; iterator = iterator->next) {
			PackageData *pack1, *pack2;
			pack1 = (PackageData*)iterator->data;
			pack2 = g_hash_table_lookup (names_to_ignore, pack1->name);
			if (pack2) {
				remove = g_list_prepend (remove, pack1);
			}
		}

		/* Strip them from "result" */
		if (remove) {
			for (iterator = remove; iterator; iterator = iterator->next) {
				PackageData *pack;
				pack = (PackageData*)iterator->data;
				result = g_list_remove (result, pack);
				packagedata_destroy (pack);
			}
			g_list_free (remove);
		}

		g_hash_table_destroy (names_to_ignore);
	}
		
	return result;
}
