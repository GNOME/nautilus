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
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 */

/* CVS version 1.41 contains the big chunks of old logic code
   that got evicted in the last phases of logic2 work */

#include "eazel-install-logic.h"
#include "eazel-install-xml-package-list.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"
#include "eazel-install-rpm-glue.h"
#include "eazel-install-query.h"
#include "eazel-install-logic2.h"

/* We use rpmvercmp to compare versions... */
#include <rpm/rpmlib.h>
#include <rpm/misc.h>

#ifndef EAZEL_INSTALL_NO_CORBA
#include <libtrilobite/libtrilobite.h>
#else
#include <libtrilobite/libtrilobite-service.h>
#include <libtrilobite/trilobite-root-helper.h>
#endif

#include <libtrilobite/trilobite-core-utils.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#ifdef EAZEL_INSTALL_SLIM
#include <sys/wait.h>
#endif

/* A GHRFunc to clean
   out the name_to_package hash table 

static gboolean
eazel_install_clean_name_to_package_hash (char *key,
					  PackageData *pack,
					  EazelInstall *service)
{
	g_free (key);
	return TRUE;
}
*/

/* This begins the package transaction.
   Return value is number of failed packages 
*/

#if 0
static int
eazel_install_start_transaction (EazelInstall *service,
				 GList* packages) 
{
	TrilobiteRootHelper *root_helper;
	int res;
	int flag = 0;

	if (g_list_length (packages) == 0) {
		return -1;
	}

	res = 0;

	if (service->private->downloaded_files) {
		/* I need to get the length here, because all_files_check can alter the list */
		int l  = g_list_length (packages);
		/* Unless we're force installing, check file conflicts */
		if (eazel_install_get_force (service) == FALSE) {
			if(!check_md5_on_files (service, packages)) {
				res = l;
			}
		}
	}

	if (eazel_install_get_test (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_TEST;
	}
	if (eazel_install_get_force (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE;
	}
	if (eazel_install_get_update (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE;
	}
	if (eazel_install_get_downgrade (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE;
	}

	root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
	gtk_object_set_data (GTK_OBJECT (service->private->package_system), 
			     "trilobite-root-helper", root_helper);	

	/* Init the hack var to emit the old style progress signals */
	service->private->infoblock [0] = 0;
	service->private->infoblock [1] = 0;
	service->private->infoblock [2] = 0;
	service->private->infoblock [3] = g_list_length (packages);
	service->private->infoblock [4] = 0;
	service->private->infoblock [5] = 0;

	if (eazel_install_emit_preflight_check (service, packages)) {
		/* this makes the primary packages appear before their dependencies.  very useful for installs
		 * via the install-view, where only toplevel packages cause the package detail info to update.
		 */
		packages = g_list_reverse (packages);

		if (eazel_install_get_uninstall (service)) {
			eazel_package_system_uninstall (service->private->package_system,
							service->private->cur_root,
							packages,
							flag);
		} else {
			eazel_package_system_install (service->private->package_system,
						      service->private->cur_root,
						      packages,
						      flag);
		}

		eazel_install_save_transaction_report (service);
	}
	
	g_list_free (service->private->transaction);
	service->private->transaction = NULL;

	g_hash_table_foreach_remove (service->private->name_to_package_hash,
				     (GHRFunc)eazel_install_clean_name_to_package_hash,
				     service);

	return res;
} /* end start_transaction */
#endif 

/*
  This function tests wheter "package" and "dep"
  seems to be related in some way.
  This is done by checking the package->modifies list for
  elements that have same version as dep->version.
  I then compare these elements against dep->name,
  and if one matches the x-y-z vs dep->name=x-y scheme,
  I declare that "package" and "dep" are related
*/
gboolean
eazel_install_check_if_related_package (EazelInstall *service,
					PackageData *package,
					PackageData *dep)
{
	/* Pessimisn mode = HIGH */
	gboolean result = FALSE;
	GList *potiental_mates;
	GList *iterator;
	char **dep_name_elements;
	
	dep_name_elements = g_strsplit (dep->name, "-", 80);

	/* First check, if package modifies a package with the same version
	   number as dep->version */
	potiental_mates = g_list_find_custom (package->modifies, 
					      dep->version, 
					      (GCompareFunc)eazel_install_package_version_compare);
	for (iterator = potiental_mates; iterator; glist_step (iterator)) {
		PackageData *modpack = (PackageData*)iterator->data;
		
		if ((modpack->modify_status == PACKAGE_MOD_UPGRADED) ||
		    (modpack->modify_status == PACKAGE_MOD_DOWNGRADED)) {			
			char **mod_name_elements;
			char *dep_name_iterator;
			char *mod_name_iterator;
			int cnt = 0;

			mod_name_elements = g_strsplit (modpack->name, "-", 80);
			
			for (cnt=0; TRUE;cnt++) {
				dep_name_iterator = dep_name_elements[cnt];
				mod_name_iterator = mod_name_elements[cnt];
#if 0
				trilobite_debug ("dep name iterator = \"%s\"", dep_name_iterator);
				trilobite_debug ("mod name iterator = \"%s\"", mod_name_iterator);
#endif
				if ((dep_name_iterator==NULL) ||
				    (mod_name_iterator==NULL)) {
					break;
				}
				if ((strlen (dep_name_iterator) == strlen (mod_name_iterator)) &&
				    (strcmp (dep_name_iterator, mod_name_iterator)==0)) {
					continue;
				}
				break;
			}
			if (cnt >= 1) {
				trilobite_debug ("%s-%s seems to be a child package of %s-%s, which %s-%s updates",
						 dep->name, dep->version, 
						 modpack->name, modpack->version,
						 package->name, package->version);
				if (!result) {
					result = TRUE;
				} else {
					trilobite_debug ("but what blows is, the previous also did!!");
					g_assert_not_reached ();
				}				
			} else {
				trilobite_debug ("%s-%s is not related to %s-%s", 
						 dep->name, dep->version, 
						 package->name, package->version);
			}
			g_strfreev (dep_name_elements);
			g_strfreev (mod_name_elements);			
		}
	}
	return result;
}

static void
dump_one_package (GtkObject *foo, char *prefix)
{
	char *softprefix, *modprefix, *breakprefix;
	char *packname;
	PackageData *pack = NULL;

	if (IS_PACKAGEDATA (foo)) {
		pack = PACKAGEDATA (foo);
	} else if (IS_PACKAGEBREAKS (foo)) {
		pack = packagebreaks_get_package (PACKAGEBREAKS (foo));
	} else if (IS_PACKAGEDEPENDENCY (foo)) {
		pack = PACKAGEDEPENDENCY (foo)->package;
	} else {
		g_assert_not_reached ();
	}

	if (pack->name == NULL) {
		if (pack->provides && pack->provides->data) {
			packname = g_strdup_printf ("[provider of %s]", (char *)(pack->provides->data));
		} else {
			packname = g_strdup ("[mystery package]");
		}
	} else {
		packname = packagedata_get_readable_name (pack);
	}

	trilobite_debug ("%s%s (stat %s/%s), %p", 
			 prefix, packname,
			 packagedata_status_enum_to_str (pack->status),
			 packagedata_modstatus_enum_to_str (pack->modify_status),
			 pack);
	g_free (packname);

	softprefix = g_strdup_printf ("%s (s) ", prefix);
	breakprefix = g_strdup_printf ("%s (b) ", prefix);
	modprefix = g_strdup_printf ("%s (m) ", prefix);
	g_list_foreach (pack->depends, (GFunc)dump_one_package, softprefix);
	g_list_foreach (pack->modifies, (GFunc)dump_one_package, modprefix);
	g_list_foreach (pack->breaks, (GFunc)dump_one_package, breakprefix);
	g_free (softprefix);
	g_free (modprefix);
	g_free (breakprefix);
}

static void
print_package_list (char *str, GList *packages, gboolean show_deps)
{
	GList *iterator;
	PackageData *pack;
	char *tmp = NULL;
	char *dep = " depends on ";
	/*	char *breaks = " breaks ";*/

	trilobite_debug ("---------------------------");
	trilobite_debug ("%s", str);
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		pack = (PackageData*)iterator->data;
		if (show_deps) {
			GList *it2;
			tmp = g_strdup (dep);
			it2 = pack->soft_depends;
			while (it2) { 
				char *tmp2;
				tmp2 = g_strdup_printf ("%s%s ", tmp ,
							((PackageData*)it2->data)->name);
				g_free (tmp);
				tmp = tmp2;
				it2 = g_list_next (it2);
			}
/*
			tmp = g_strdup (breaks);
			it2 = pack->breaks;
			while (it2) { 
				char *tmp2;
				PackageData *p2;
				p2 = (PackageData*)it2->data;
				tmp2 = g_strdup_printf ("%s%s(%db%dd) ", tmp , rpmfilename_from_packagedata (p2), 
							g_list_length (p2->breaks),
							g_list_length (p2->soft_depends));
				g_free (tmp);
				tmp = tmp2;
				it2 = g_list_next (it2);
			}
*/
		}
		trilobite_debug ("* %s (%s) %s", 
			   pack->name, 
			   pack->toplevel ? "true" : "", 
			   (tmp && strlen (tmp) > strlen (dep)) ? tmp : "");
		g_free (tmp);
		tmp = NULL;
	}
}

static int
compare_break_to_package_by_name (PackageBreaks *breakage, PackageData *pack)
{
	PackageData *broken_package = packagebreaks_get_package (breakage);

	return eazel_install_package_compare (broken_package, pack);
}

/* This traverses upwards in the deptree from the initial list, and adds
   all packages that will break to "breaks" */
static void
eazel_uninstall_upward_traverse (EazelInstall *service,
				 GList **packages,
				 GList **failed,
				 GList **breaks)
{
	GList *iterator;
	/*
	  Create set
	  add all packs from packages to set
	  dep check
	  for all break, add to packages and recurse
	 */

	trilobite_debug ("in eazel_uninstall_upward_traverse");

	g_assert (packages!=NULL);
	g_assert (*packages!=NULL);
	g_assert (breaks!=NULL);
	g_assert (*breaks==NULL);

	/* Open the package system */

	/* Add all packages to the set */

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *matches = NULL;
		GList *match_iterator;
		GList *tmp_breaks = NULL;
		GList *b_iterator = NULL;

		/* Get the packages required by pack */
		trilobite_debug ("checking reqs by %p %s", pack, rpmname_from_packagedata (pack));
		matches = eazel_package_system_query (service->private->package_system,
						      service->private->cur_root,
						      pack,
						      EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
						      PACKAGE_FILL_NO_DIRS_IN_PROVIDES | PACKAGE_FILL_NO_DEPENDENCIES);
		
		/* For all of them, mark as a break conflict */
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *requiredby = (PackageData*)match_iterator->data;;
			
			requiredby->status = PACKAGE_DEPENDENCY_FAIL;
			pack->status = PACKAGE_BREAKS_DEPENDENCY;
			trilobite_debug ("logic.c: %p %s requires %p %s", 
					 requiredby, requiredby->name, 
					 pack, pack->name);

			/* If the broken package is in packages, just continue */
			if (g_list_find_custom (*packages, requiredby,
						(GCompareFunc)eazel_install_package_compare)) {
				trilobite_debug ("skip %p %s", requiredby, requiredby->name);
				continue;
			}

			/* only add to breaks if it's a new breakage */
			if (g_list_find_custom (*breaks, (gpointer)requiredby, 
						(GCompareFunc)compare_break_to_package_by_name)) {
				(*breaks) = g_list_prepend ((*breaks), requiredby);
			}
			
			/* Create a FeatureMissing breakage */
			{
				PackageFeatureMissing *breakage = packagefeaturemissing_new ();
				packagebreaks_set_package (PACKAGEBREAKS (breakage), requiredby);
				packagedata_add_to_breaks (pack, PACKAGEBREAKS (breakage));
				gtk_object_unref (GTK_OBJECT (breakage));
			}

			/* If the pac has not been failed yet (and is a toplevel),
			   fail it */
			if (!g_list_find_custom (*failed, (gpointer)pack->name, 
						 (GCompareFunc)eazel_install_package_name_compare) &&
			    pack->toplevel) {
				(*failed) = g_list_prepend (*failed, pack);
			}
		}
		g_list_foreach (matches, (GFunc)gtk_object_unref, NULL);
		g_list_free (matches);

		/* Now check the packages that broke, this is where eg. uninstalling
		   glib begins to take forever */
		if (*breaks) {
			eazel_uninstall_upward_traverse (service, breaks, failed, &tmp_breaks);
		}
		
		/* Add the result from the recursion */
		for (b_iterator = tmp_breaks; b_iterator; b_iterator = g_list_next (b_iterator)) {
			(*breaks) = g_list_prepend ((*breaks), b_iterator->data);
		}
	}
	
	/* Remove the failed packages */
	for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
		(*packages) = g_list_remove (*packages, iterator->data);
	}

	trilobite_debug ("out eazel_uninstall_upward_traverse");
}

/* This traverses downwards on all requirements in "packages", 
   checks that their uninstall do _not_ break anything, and
   adds thm to requires */

static void
eazel_uninstall_downward_traverse (EazelInstall *service,
				   GList **packages,
				   GList **failed,
				   GList **requires)
{
	GList *iterator;
	GList *tmp_requires = NULL;

	/* 
	   create set
	   find all requirements from "packages"
	   add all packs + requirements from "packages" to set
	   dep check
	   for all breaks, remove from requirements
	   recurse calling eazel_uninstall_downward_traverse (requirements, &tmp)
	   add all from tmp to requirements
	*/
	trilobite_debug ("in eazel_uninstall_downward_traverse");
	
	/* First iterate across the packages in "packages" */
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		GList *matches = NULL;
		PackageData *pack;
		GList *match_iterator;

		pack = (PackageData*)iterator->data;

		matches = eazel_package_system_query (service->private->package_system,
						      service->private->cur_root,
						      pack->name,
						      EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
						      PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
		trilobite_debug ("%s had %d hits", pack->name, g_list_length (matches));

		/* Now iterate over all packages that match pack->name */
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *matched_pack;
			const char **require_name;
			int require_name_count;
			Header hd;
			int type;
			int j;

			matched_pack = (PackageData*)match_iterator->data;
			hd = ((Header) matched_pack->packsys_struc);

			if (!headerGetEntry(hd, RPMTAG_REQUIRENAME, &type, (void **) &require_name,
					    &require_name_count)) {
				require_name_count = 0;
			}
			
			trilobite_debug ("requirename count = %d", require_name_count);
			
			/* No iterate over all packages required by the current package */
			for (j = 0; j < require_name_count; j++) {
				if ((*require_name[j] != '/') &&
				    !strstr (require_name[j], ".so")) {
					PackageData *tmp_pack = packagedata_new ();
					GList *second_matches;
					GList *second_match_iterator;

					tmp_pack->name = g_strdup (require_name[j]);
					second_matches = 
						eazel_package_system_query (service->private->package_system,
									    service->private->cur_root,
									    tmp_pack,
									    EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
									    PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
					packagedata_list_prune (&second_matches, *packages, TRUE, TRUE);
					packagedata_list_prune (&second_matches, *requires, TRUE, TRUE);
					gtk_object_unref (GTK_OBJECT (tmp_pack));

					/* Iterate over all packages that match the required package */
					for (second_match_iterator = second_matches;
					     second_match_iterator; 
					     second_match_iterator = g_list_next (second_match_iterator)) {
						PackageData *isrequired;

						isrequired = (PackageData*)second_match_iterator->data;
						if (g_list_find_custom (*requires, isrequired->name,
									(GCompareFunc)eazel_install_package_name_compare) ||
						    g_list_find_custom (*packages, isrequired->name,
									(GCompareFunc)eazel_install_package_name_compare)) {
							trilobite_debug ("skipped %s", isrequired->name);
							gtk_object_unref (GTK_OBJECT (isrequired));\
							isrequired = NULL;
							continue;
						}		
						trilobite_debug ("** %s requires %s", pack->name, isrequired->name);

						{
							GList *third_matches;

							/* Search for packages
							   requiring the requirement, excluding
							   all pacakges from requires and packages */
							third_matches = 
								eazel_package_system_query (service->private->package_system,
											    service->private->cur_root,
											    pack->name,
											    EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
											    PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
							packagedata_list_prune (&third_matches, 
										*packages, TRUE, TRUE);
							packagedata_list_prune (&third_matches, 
										*requires, TRUE, TRUE);
							packagedata_list_prune (&third_matches, 
										tmp_requires, TRUE, TRUE);
							
							if (third_matches) {
								trilobite_debug ("skipped %s, others depend on it", 
									   isrequired->name);
								print_package_list ("BY", third_matches, FALSE);
								g_list_foreach (third_matches, 
										(GFunc)gtk_object_unref, NULL);
								g_list_free (third_matches);
								third_matches = NULL;
								gtk_object_unref (GTK_OBJECT (isrequired));
								isrequired = NULL;
							} else {
								trilobite_debug ("Also nuking %s", isrequired->name);
								tmp_requires = g_list_prepend (tmp_requires, 
											       isrequired);
							}
						}
					}
					g_list_free (second_matches);
				} else {
					trilobite_debug ("must lookup %s", require_name[j]);
					/* FIXME bugzilla.eazel.com 1541:
					   lookup package "p" that provides requires[j],
					   if packages that that require "p" are not in "packages"
					   don't add it, otherwise add to requires */
				}
			}

			headerFree (hd);
		}
		g_list_foreach (matches, (GFunc)gtk_object_unref, NULL);
		g_list_free (matches);
		matches = NULL;
	}

	if (tmp_requires) {
		eazel_uninstall_downward_traverse (service, &tmp_requires, failed, requires);
	}

	/* Now move the entries in tmp_requires into *requires */
	for (iterator = tmp_requires; iterator; iterator = g_list_next (iterator)) {
		(*requires) = g_list_prepend (*requires, iterator->data);
	}
	g_list_free (tmp_requires);

	trilobite_debug ("out eazel_uninstall_downward_traverse");
}
