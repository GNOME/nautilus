/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2001 Eazel, Inc
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <sys/utsname.h>

#include "eazel-clone-lib.h"
#include <eazel-package-system.h>
#include <eazel-install-xml-package-list.h>
#include <libtrilobite/libtrilobite.h>

#include <unistd.h>

typedef struct {
	PackageData *installed;
	PackageData *inventory;
	EazelSoftCatSense sense;
} PackagePair;

/* Ripped from eazel-install-public.h */

typedef enum {
	EAZEL_INSTALL_STATUS_NEW_PACKAGE,
	EAZEL_INSTALL_STATUS_UPGRADES,
	EAZEL_INSTALL_STATUS_DOWNGRADES,
	EAZEL_INSTALL_STATUS_QUO
} EazelInstallStatus;


GList*
eazel_install_clone_load_inventory (char *input_file)
{
	FILE *f;
	GList *result = NULL;

	if (input_file==NULL) {
		f = stdin;
	} else {
		f = fopen (input_file, "rt");
	}

	if (f==NULL) {
		trilobite_debug ("Could not access file %s", input_file==NULL ? "(stdin)":input_file);
	} else {
		char *tmp;
		size_t size, rsize;

		fseek (f, 0, SEEK_END);
		size = ftell (f);
		fseek (f, 0, SEEK_SET);
		tmp = g_new (char, size);
		rsize = fread (tmp, 1, size, f);
		if (rsize != size) {
			trilobite_debug ("read too little (%d, expected %d)", rsize, size);
		} else {
			result = parse_memory_xml_package_list (tmp, size);
		}
		g_free (tmp);
	}
	
	trilobite_debug ("%d category (%s), %d packages", 
			 g_list_length (result), 
			 ((CategoryData*)(result->data))->name,
			 g_list_length (((CategoryData*)(result->data))->packages));

	/* FIXME: I leak the glist and the one categorydata element here */
	return ((CategoryData*)(result->data))->packages;
}

/* Ripped and modified from eazel-install-logic2.c */

static EazelInstallStatus
eazel_install_clone_check_existing (EazelPackageSystem *packsys,
				    PackageData *pack,
				    PackagePair **pair)
{
	GList *existing_packages;
	GList *borked_packages = NULL;
	EazelInstallStatus result;

	g_assert (pair);
	g_assert ((*pair)==NULL);
	g_assert (pack);
	g_assert (IS_PACKAGEDATA (pack));

#if EC_DEBUG & 0x4
	trilobite_debug ("check_existing %p %s", pack, pack->name);
#endif
	result = EAZEL_INSTALL_STATUS_NEW_PACKAGE;
	/* query for existing package of same name */
	existing_packages = eazel_package_system_query (packsys,
							NULL,
							pack->name,
							EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
							PACKAGE_FILL_NO_DEPENDENCIES|
							PACKAGE_FILL_NO_TEXT|
							PACKAGE_FILL_NO_DIRS_IN_PROVIDES);

	/* If error, handle it */
	if (packsys->err) {
#if EC_DEBUG & 0x4
		switch (packsys->err->e) {
		case EazelPackageSystemError_DB_ACCESS:
			if (strcmp (packsys->err->u.db_access.path, "/")==0) {
				trilobite_debug ("some package dbs is locked by another process", 
						 packsys->err->u.db_access.pid);
				pack->status = PACKAGE_PACKSYS_FAILURE;
				g_free (packsys->err);
				return result;
				break;
			}
		}
#endif
	}
	if (existing_packages) {
		/* Get the existing package, set it's modify flag and add it */
		GList *iterator;
		PackageData *survivor = NULL;
		gboolean abort = FALSE;
		int res;

		if (g_list_length (existing_packages)>1) {
#if EC_DEBUG & 0x4
			trilobite_debug ("there are %d existing packages called %s",
					 g_list_length (existing_packages),
					 pack->name);
#endif
			/* Verify them all, to find one that is not damaged. Mark the rest
			   as invalid so the client can suggest the user should delete them */
			for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
				PackageData *existing_package = PACKAGEDATA (iterator->data);
				char *name = packagedata_get_readable_name (existing_package);
				GList *foo = NULL;
				
				foo = g_list_prepend (foo, existing_package);
				if (eazel_package_system_verify (packsys, NULL, foo)) {
					/* I18N note: "%is is ok" in the sense that %s=package name and the
					   package is intact */
					g_message (_("%s is ok"), name);
					if (survivor == NULL) {
						survivor = existing_package;
					} else {						
						abort = TRUE;
					}
				} else {
					/* I18N note: "%is is not ok" in the sense that %s=package name and the
					   package is not intact */
					g_message ("%s is not ok", name);
					existing_package->status = PACKAGE_INVALID;
					/* I add the borked packages later, so they'll show up
					   earlier in the tree */
					borked_packages = g_list_prepend (borked_packages, existing_package);
				}
				g_list_free (foo);
				g_free (name);
			}
		} else {
			survivor = PACKAGEDATA (g_list_first (existing_packages)->data);
		}
		
		if (abort) {
			trilobite_debug ("*********************************************************");
			trilobite_debug ("This is a bad bad case, see bug 3511");
			trilobite_debug ("To circumvent this problem, as root, execute this command");
			trilobite_debug ("(which is dangerous by the way....)");
			trilobite_debug ("rpm -e --nodeps `rpm -q %s`", pack->name);
			
			g_list_free (borked_packages);

			/* Cancel the package, mark all the existing as invalid */			   
			pack->status = PACKAGE_CANCELLED;
			for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
				PackageData *existing_package = PACKAGEDATA (iterator->data);
				existing_package->status = PACKAGE_INVALID;
				packagedata_add_pack_to_modifies (pack, existing_package);
				gtk_object_unref (GTK_OBJECT (existing_package));
			}
		}

		if (abort==FALSE && survivor) {
			g_assert (pack->version);
			g_assert (survivor->version);

			res = eazel_package_system_compare_version (packsys,
								    pack->version, 
								    survivor->version);

			if (survivor->epoch > 0) {
#if EC_DEBUG & 0x4
				g_warning ("modified package has epoch %d, new package has %d",
					   survivor->epoch,
					   pack->epoch);
#endif
				gtk_object_set_data (GTK_OBJECT (packsys),
						     "ignore-epochs", GINT_TO_POINTER (1));
			}
			
			/* check against minor version */
			if (res==0) {
#if EC_DEBUG & 0x4
				trilobite_debug ("versions are equal (%s), comparing minors", pack->version);
#endif
				if (pack->minor && survivor->minor) {
#if EC_DEBUG & 0x4
					trilobite_debug ("minors are %s for new and %s for installed)", 
							 pack->minor, survivor->minor);
#endif
					res = eazel_package_system_compare_version (packsys,
										    pack->minor, 
										    survivor->minor);
				} else if (!pack->minor && survivor->minor) {
					/* If the given packages does not have a minor,
					   but the installed has, assume we're fine */
					/* FIXME: bugzilla.eazel.com
					   This is a patch, it should be res=1, revert when
					   softcat is updated to have revisions for all packages 
					   (post PR3) */
					res = 1;
				} else {
					/* Eh, do nothing just to be safe */
					res = 0;
				}
			}

			(*pair) = g_new0 (PackagePair, 1);
			(*pair)->inventory = pack;
			(*pair)->installed = survivor;

			/* Set the modify_status flag */
			if (res == 0) {
				(*pair)->sense = EAZEL_SOFTCAT_SENSE_EQ;
				survivor->modify_status = PACKAGE_MOD_UNTOUCHED;
			} else if (res > 0) {
				(*pair)->sense = EAZEL_SOFTCAT_SENSE_GT;
				survivor->modify_status = PACKAGE_MOD_UPGRADED;
			} else {
				(*pair)->sense = EAZEL_SOFTCAT_SENSE_LT;
				survivor->modify_status = PACKAGE_MOD_DOWNGRADED;
			}

			/* Calc the result */
			if (res == 0) {
				result = EAZEL_INSTALL_STATUS_QUO;
			} else if (res > 0) {
				result = EAZEL_INSTALL_STATUS_UPGRADES;
			} else {
				result = EAZEL_INSTALL_STATUS_DOWNGRADES;
			}
		
			if (result != EAZEL_INSTALL_STATUS_QUO) {
				survivor->status = PACKAGE_RESOLVED;
			} 
			pack->status = PACKAGE_ALREADY_INSTALLED;
		}

		/* Now add the borked packages */
		for (iterator = borked_packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *existing_package = PACKAGEDATA (iterator->data);
			packagedata_add_pack_to_modifies (pack, existing_package);
			gtk_object_unref (GTK_OBJECT (existing_package));

		}
		g_list_free (borked_packages);

		/* Free the list structure from _simple_query */
		g_list_free (existing_packages);
	} else {
#if EC_DEBUG & 0x4
		if (pack->minor) {
			trilobite_debug (_("%s installs version %s-%s"), 
					 pack->name, 
					 pack->version,
					 pack->minor);
		} else {
			trilobite_debug (_("%s installs version %s"), 
					 pack->name, 
					 pack->version);
		}

#endif
	}

	return result;
}

void
eazel_install_clone_compare_inventory (GList *inventory,
				       GList **install,
				       GList **upgrade,
				       GList **downgrade)
{
	GList *iterator;
	EazelPackageSystem *packsys = eazel_package_system_new (NULL);
	
	g_assert (install);
	g_assert ((*install)==NULL);
	g_assert (upgrade);
	g_assert ((*upgrade)==NULL);
	g_assert (downgrade);
	g_assert ((*downgrade)==NULL);

	for (iterator = inventory; iterator; iterator = g_list_next (iterator)) {
		PackageData *package;
		PackagePair *pair = NULL;
		EazelInstallStatus res;

		package = PACKAGEDATA (iterator->data);
		res = eazel_install_clone_check_existing (packsys, package, &pair);
		switch (res) {
		case EAZEL_INSTALL_STATUS_QUO:
			break;
		case EAZEL_INSTALL_STATUS_NEW_PACKAGE:
			(*install) = g_list_prepend ((*install), package);
			break;
		case EAZEL_INSTALL_STATUS_UPGRADES:
			(*upgrade) = g_list_prepend ((*upgrade), package);
			break;
		case EAZEL_INSTALL_STATUS_DOWNGRADES:
			(*downgrade) = g_list_prepend ((*downgrade), package);
			break;
		}
		if (pair) {
			gtk_object_unref (GTK_OBJECT (pair->installed));
			g_free (pair);
		}
	}
	gtk_object_unref (GTK_OBJECT (packsys));
}

void 
eazel_install_clone_create_inventory (char **mem, long *size)
{
	GList *packages;
	EazelPackageSystem *packsys = eazel_package_system_new (NULL);
	xmlDocPtr doc;
	xmlNodePtr ptr, node;

	packages = eazel_package_system_query (packsys,
					       NULL,
					       "",
					       EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR,
 					       PACKAGE_FILL_MINIMAL);

	/* FIXME: must strip dupes here.
	   for p and q where p->name == q->name, 
	   verify both, and only list the ones that pass */

	ptr = eazel_install_packagelist_to_xml (packages, FALSE);
	doc = xmlNewDoc ("1.0");
	doc->root = xmlNewDocNode (doc, NULL, "CATEGORIES", NULL);
	node = xmlNewChild (doc->root, NULL, "CATEGORY", NULL);
	xmlSetProp (node, "name", "inventory");
	xmlAddChild (node, ptr);
	xmlDocDumpMemory (doc, (xmlChar**)mem, (int*)size);
	xmlFreeDoc (doc);

	gtk_object_unref (GTK_OBJECT (packsys));
}
