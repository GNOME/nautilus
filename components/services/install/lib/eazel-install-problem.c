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

#include <eazel-install-problem.h>

#include <libtrilobite/trilobite-i18n.h>

#include <string.h>

static GtkObjectClass *eazel_install_problem_parent_class;

#define ASSERT_SANITY(s) g_assert (s!=NULL); \
                         g_assert (EAZEL_IS_INSTALL_PROBLEM(s));

#define P_SANITY(s) g_return_if_fail (s!=NULL); \
                    g_return_if_fail (EAZEL_IS_INSTALL_PROBLEM(s));

#define P_SANITY_VAL(s,val) g_return_val_if_fail (s!=NULL, val); \
                            g_return_val_if_fail (EAZEL_IS_INSTALL_PROBLEM(s), val);


#undef EIP_DEBUG

/* Data argument to get_detailed_errors_foreach.
   Contains the installer and a path in the tree
   leading to the actual package */
typedef struct {
	EazelInstallProblem *problem;
	GList *errors;
	GList *path;
	GList *packs;
	GList *handled;
} GetErrorsForEachData;

#ifdef EIP_DEBUG
static void
eazel_install_problem_debug_attempts (int *key,
				      GList *val,
				      EazelInstallProblem *problem)
{
	g_message ("eazel_install_problem_debug_attempts key = %d", *key);
	g_message ("eazel_install_problem_debug_attempts val.length = %d", g_list_length (val));
}
#endif

static void
get_detailed_messages_breaks_foreach (PackageBreaks *breakage, GetErrorsForEachData *data)
{
	PackageData *previous_pack = NULL;
	PackageData *top_pack = NULL;
	GList **errors = &(data->errors);
	char *top_name = NULL;
	char *previous_name = NULL;
	char *package_broken_name = NULL;;

	if (data->path) {
		previous_pack = PACKAGEDATA(data->path->data);
		previous_name = packagedata_get_readable_name (previous_pack);
		top_pack = PACKAGEDATA(g_list_last (data->path)->data);
		if (top_pack == previous_pack) {
			top_pack = NULL;
		} else {
			top_name = packagedata_get_readable_name (top_pack);
		}
	}

	package_broken_name = packagedata_get_readable_name (packagebreaks_get_package (breakage));

	if (IS_PACKAGEFILECONFLICT (breakage)) {
		char *message;

		if (top_pack != NULL) {
			message = g_strdup_printf ("Conflict between %s (required by %s) and %s", 
						   previous_name,
						   top_name,
						   package_broken_name);
		} else {
			message = g_strdup_printf ("Conflict between %s and %s", 
						   previous_name,
						   package_broken_name);
		}
		(*errors) = g_list_append (*errors, message);
	} else if (IS_PACKAGEFEATUREMISSING (breakage)) {
		PackageFeatureMissing *missing = PACKAGEFEATUREMISSING (breakage);
		missing = NULL;
	} else {
		char *message;
		
		message = g_strdup_printf ("Feature dependency between %s and %s", 
					   previous_name,
					   package_broken_name);
		(*errors) = g_list_append (*errors, message);
	}

	g_free (top_name);
	g_free (previous_name);
	g_free (package_broken_name);
}

static void
get_detailed_messages_foreach (GtkObject *foo, GetErrorsForEachData *data)
{
	char *message = NULL;
	char *required = NULL;
	char *required_by = NULL;
	char *top_name = NULL;
	GList **errors = &(data->errors);
	PackageData *previous_pack = NULL;
	PackageData *top_pack = NULL;
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


	if (data->path) {
		previous_pack = PACKAGEDATA(data->path->data);
		top_pack = PACKAGEDATA(g_list_last (data->path)->data);
		if (top_pack == previous_pack) {
			previous_pack = NULL;
		}
		required_by = packagedata_get_readable_name (top_pack);
		top_name = packagedata_get_readable_name (top_pack);
	}
	required = packagedata_get_readable_name (pack);

	if (g_list_find (data->handled, pack)) { return; }

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
		break;
	case PACKAGE_CANCELLED:
		if (required_by) {
			if (pack->modifies!=NULL) {
				message = g_strdup_printf (_("%s was cancelled"), required);
			}
		}
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		message = g_strdup_printf (_("%s is a source package, which is not yet supported"), 
					   required);
		break;
	case PACKAGE_FILE_CONFLICT:
		/* this will be reported below, when parsing the PackageBreaks */
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		if (pack->depends) {

		} else {
			if (previous_pack && previous_pack->status == PACKAGE_BREAKS_DEPENDENCY) {
				if (required_by) {

				} else {
					message = g_strdup_printf (_("%s would break other packages"), required);
				}				
			} else {
				message = g_strdup_printf (_("%s would break"), required);
			}
		}
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		if (required_by) {
			message = g_strdup_printf (_("%s would break %s"), required, required_by);
		} else {
			message = g_strdup_printf (_("%s would break other packages"), required);
		}
		break;
	case PACKAGE_INVALID:
		message = g_strdup_printf (_("%s is damaged"), required);
		break;
	case PACKAGE_CANNOT_OPEN:
		if (previous_pack) {
			message = g_strdup_printf (_("%s requires %s, which could not be found on the server"), 
						   required_by,required);
		} else {
			message = g_strdup_printf (_("%s for %s could not be found on the server"), 
						   required,
						   trilobite_get_distribution_name (pack->distribution,
										    TRUE, FALSE));
						   
		}
		break;
	case PACKAGE_PARTLY_RESOLVED:
		break;
	case PACKAGE_ALREADY_INSTALLED:
		if (pack->modifies==NULL) {
			message = g_strdup_printf (_("%s is already installed"), required);
		}
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY: 
		if (previous_pack == NULL) {
			message = g_strdup_printf ("%s depends on itself, internal error",
						   required);
		} else {
			if (previous_pack->status == PACKAGE_CIRCULAR_DEPENDENCY) {
				if (strcmp (required_by, required)==0) {
					message = g_strdup_printf ("%s depends on itself. internal error",
								   required);					
				} else {
					if (g_list_length (data->path) >= 3) {
						PackageData *causing_package = PACKAGEDATA((g_list_nth (data->path, 1))->data);
						char *cause = packagedata_get_readable_name (causing_package);
						message = g_strdup_printf ("%s and %s are mutexed because of %s", 
									   required_by,
									   required, 
									   cause);
						g_free (cause);
					} else {
						message = g_strdup_printf ("%s and %s exclude each other, but were both needed",
									   required_by,
									   required);
					}
				}
			} 
		}
		break;
	case PACKAGE_RESOLVED:
		break;
	case PACKAGE_PACKSYS_FAILURE:
		message = g_strdup_printf ("Cannot access the local package system");
		break;
	}

	if (message != NULL) {
		(*errors) = g_list_append (*errors, message);
	} else if (pack->status == PACKAGE_CANCELLED) {
		switch (pack->modify_status) {
		case PACKAGE_MOD_UNTOUCHED:
			break;
		case PACKAGE_MOD_DOWNGRADED:
			message = g_strdup_printf (_("%s, which is newer, needs downgrade and downgrade is not enabled"), 
						   required);
			break;
		case PACKAGE_MOD_UPGRADED:
			message = g_strdup_printf (_("%s, which is older, needs upgrade and upgrade is not enabled"), 
						   required);						     
			break;
		case PACKAGE_MOD_INSTALLED:
			break;
		case PACKAGE_MOD_UNINSTALLED:
			break;
		}
		
		if (message != NULL) {
			(*errors) = g_list_append (*errors, message);
		}
	}

	g_free (required);
	g_free (required_by);
	g_free (top_name);

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);
	data->handled = g_list_prepend (data->handled, pack);

	if (pack->status != PACKAGE_CANCELLED) {
		g_list_foreach (pack->depends, (GFunc)get_detailed_messages_foreach, data);
		g_list_foreach (pack->modifies, (GFunc)get_detailed_messages_foreach, data);
		g_list_foreach (pack->breaks, (GFunc)get_detailed_messages_breaks_foreach, data);
	}

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
}

static void
get_detailed_uninstall_messages_foreach (GtkObject *foo,
					 GetErrorsForEachData *data)
{
	char *message = NULL;
	char *required = NULL;
	char *required_by = NULL;
	char *top_name = NULL;
	GList **errors = &(data->errors);
	PackageData *previous_pack = NULL;
	PackageData *top_pack = NULL;
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

	if (data->path) {
		previous_pack = PACKAGEDATA(data->path->data);
		top_pack = PACKAGEDATA(g_list_last (data->path)->data);
		if (top_pack == previous_pack) {
			previous_pack = NULL;
		}
		required = packagedata_get_readable_name (previous_pack);
		top_name = packagedata_get_readable_name (top_pack);
	}
	required_by = packagedata_get_readable_name (pack);

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
	case PACKAGE_CANCELLED:
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		message = g_strdup_printf (_("%s is a source package, which is not yet supported"), 
					   required);
		break;
	case PACKAGE_FILE_CONFLICT:
		break;
	case PACKAGE_DEPENDENCY_FAIL:
	case PACKAGE_BREAKS_DEPENDENCY:
		if (previous_pack) {
			message = g_strdup_printf (_("%s requires %s"), 
						   required_by, required);
		} else if (top_pack) {
			message = g_strdup_printf (_("%s requires %s"), 
						   required_by, top_name);
		}
		break;
		break;
	case PACKAGE_INVALID:
		break;
	case PACKAGE_CANNOT_OPEN:
		message = g_strdup_printf (_("%s is not installed and could not be found on server"), 
					   required_by);
		break;
	case PACKAGE_PARTLY_RESOLVED:
		break;
	case PACKAGE_ALREADY_INSTALLED:
		if (pack->modifies == NULL) {
			message = g_strdup_printf (_("%s is already installed"), required);
		}
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY: 
		break;
	case PACKAGE_RESOLVED:
		break;
	case PACKAGE_PACKSYS_FAILURE:
		message = g_strdup_printf ("Cannot access the local package system");
		break;
	}

	if (message != NULL) {
		(*errors) = g_list_append (*errors, message);
	}

	g_free (required);
	g_free (required_by);
	g_free (top_name);

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);

	g_list_foreach (pack->depends, (GFunc)get_detailed_uninstall_messages_foreach, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_uninstall_messages_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_uninstall_messages_foreach, data);

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
}

static int
compare_problem_case (EazelInstallProblemCase *a, EazelInstallProblemCase *b)
{
	int result = -1;
	if (a->problem == b->problem) {
		switch (a->problem) {
		case EI_PROBLEM_UPDATE:
			result = eazel_install_package_compare (a->u.update.pack,
								b->u.update.pack);
			break;
		case EI_PROBLEM_FORCE_INSTALL_BOTH:
			result = (eazel_install_package_compare (a->u.force_install_both.pack_1,
								 b->u.force_install_both.pack_1) ||
				  eazel_install_package_compare (a->u.force_install_both.pack_2,
								 b->u.force_install_both.pack_2));
			break;
		case EI_PROBLEM_REMOVE:
			result = eazel_install_package_compare (a->u.remove.pack,
								b->u.remove.pack);
			break;
		case EI_PROBLEM_FORCE_REMOVE:
			result = eazel_install_package_compare (a->u.force_remove.pack,
								b->u.force_remove.pack);
			break;
		case EI_PROBLEM_INCONSISTENCY:
			result = 0;
			break;
		case EI_PROBLEM_BASE:
			g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
			g_assert_not_reached ();
			break;
		case EI_PROBLEM_CANNOT_SOLVE:
			result = compare_problem_case (a->u.cannot_solve.problem,
						       b->u.cannot_solve.problem);
			break;
		case EI_PROBLEM_CONTINUE_WITH_FLAG:
			result = (a->u.continue_with_flag.flag == a->u.continue_with_flag.flag);
			break;
		case EI_PROBLEM_CASCADE_REMOVE: {
			GList *a_iterator;
			result = 0;
			/* same number of packages ? */
			if (g_list_length (a->u.cascade.packages) != g_list_length (b->u.cascade.packages)) {
				result = 1;
			} else
			/* Check that all packages in a occur in b */
			for (a_iterator = a->u.cascade.packages; 
			     a_iterator && !result; a_iterator = g_list_next (a_iterator)) {
				PackageData *a_pack = PACKAGEDATA(a_iterator->data);
				GList *b_iterator;
				for (b_iterator = b->u.cascade.packages; b_iterator; 
				     b_iterator = g_list_next (b_iterator)) {
					PackageData *b_pack = PACKAGEDATA(b_iterator->data);
					result = eazel_install_package_compare (a_pack, b_pack);
				}
			}
		}
		break;
		}
	}
	return result;
}

static gboolean
add_case (EazelInstallProblem *problem,
	  EazelInstallProblemCase *pcase,
	  GList **output)
{
	GList *already_attempted = NULL;
	GList *case_list;

#ifdef EIP_DEBUG
	g_message ("add_case, pcase->problem = %d", pcase->problem);
	g_message ("g_hash_table_size (problem->attempts) = %d", 
		   g_hash_table_size (problem->attempts));
#endif /* EIP_DEBUG */

	/* Did a previous atttempt generate this case ? */
	/* first get the list for this case type */
	case_list = g_hash_table_lookup (problem->attempts, 
					 &(pcase->problem));
#ifdef EIP_DEBUG
	g_message ("g_list_length (case_list) = %d", 
		   g_list_length (case_list));
#endif /* EIP_DEBUG */
	/* then lookup the problem */
	already_attempted = g_list_find_custom (case_list,
						pcase,
						(GCompareFunc)compare_problem_case);

	if (already_attempted) {
#ifdef EIP_DEBUG
		g_message ("  already_attempted 1");
#endif /* EIP_DEBUG */
		return FALSE;
	}

	/* No, have we already tried this in this attempt then ? */
	case_list = g_hash_table_lookup (problem->pre_step_attempts, 
					 &(pcase->problem));
	already_attempted = NULL;
	already_attempted = g_list_find_custom (case_list,
						pcase,
						(GCompareFunc)compare_problem_case);
	if (!already_attempted) {
		/* No ? Then add it */
		if (!problem->in_step_problem_mode) {
			case_list = g_list_prepend (case_list,
						    pcase);
			g_hash_table_insert (problem->pre_step_attempts,
					     &(pcase->problem),
					     case_list);
		}
		(*output) = g_list_prepend (*output,
					    pcase);
	} else {
#ifdef EIP_DEBUG
		g_message ("  already_attempted 2");
#endif /* EIP_DEBUG */
	}

#ifdef EIP_DEBUG
	g_hash_table_foreach (problem->attempts,
			      (GHFunc)eazel_install_problem_debug_attempts,
			      problem);
#endif
	return TRUE;
}

static void
add_cannot_solve_case (EazelInstallProblem *problem,
		       EazelInstallProblemCase *org_pcase,
		       GList **output)
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_CANNOT_SOLVE);

#ifdef EIP_DEBUG
	g_message ("add_cannot_solve_case");
#endif /* EIP_DEBUG */

	pcase->u.cannot_solve.problem = org_pcase;

	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		g_warning ("%s:%d : Adding an EI_PROBLEM_CANNOT_SOLVE failed", __FILE__,__LINE__);
	}
}

static void
add_continue_with_flag_case (EazelInstallProblem *problem,
			     EazelInstallProblemCase *org_pcase,
			     EazelInstallProblemContinueFlag flag,
			     gboolean file_conflict,
			      GList **output)
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_CONTINUE_WITH_FLAG);

	pcase->u.cannot_solve.problem = org_pcase;
	pcase->file_conflict = file_conflict;
	pcase->u.continue_with_flag.flag = flag;

#ifdef EIP_DEBUG
	g_message ("add_continue_with_flag_case %p %d %d", 
		   pcase, flag,
		   pcase->u.continue_with_flag.flag);
#endif /* EIP_DEBUG */


	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		add_cannot_solve_case (problem, org_pcase, output);
	}
}

static void
add_force_install_both_case (EazelInstallProblem *problem,
			     PackageData *pack_1, 
			     PackageData *pack_2,
			     GList **output) 
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_FORCE_INSTALL_BOTH);

#ifdef EIP_DEBUG
	g_message ("add_force_install_both_case");
#endif /* EIP_DEBUG */
	gtk_object_ref (GTK_OBJECT (pack_1));
	gtk_object_ref (GTK_OBJECT (pack_2));

	pcase->u.force_install_both.pack_1 = pack_1;
	pcase->u.force_install_both.pack_2 = pack_2;
	
	if (!add_case (problem, pcase, output)) {
		add_continue_with_flag_case (problem, 
					     pcase, 
					     EazelInstallProblemContinueFlag_FORCE, 
					     FALSE, 
					     output);
	}
}

static void
add_force_remove_case (EazelInstallProblem *problem,
		       PackageData *pack,
		       gboolean file_conflict,
		       GList **output) 
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_FORCE_REMOVE);

#ifdef EIP_DEBUG
	g_message ("add_force_remove_case");
#endif /* EIP_DEBUG */
	
	gtk_object_ref (GTK_OBJECT (pack));
	pcase->u.force_remove.pack = pack;
	pcase->file_conflict = file_conflict;
	
	if (!add_case (problem, pcase, output)) {
		add_continue_with_flag_case (problem, 
					     pcase, 
					     EazelInstallProblemContinueFlag_FORCE, 
					     file_conflict, output);
	}
}

static void
add_remove_case (EazelInstallProblem *problem,
		 PackageData *pack,
		 gboolean file_conflict,
		 GList **output)
{

	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_REMOVE);

#ifdef EIP_DEBUG
	g_message ("add_remove_case");
#endif /* EIP_DEBUG */
	
	gtk_object_ref (GTK_OBJECT (pack));
	pcase->u.remove.pack = pack;
	pcase->file_conflict = file_conflict;
	
	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		add_force_remove_case (problem, pack, file_conflict, output);
	}
}

static void
add_update_case (EazelInstallProblem *problem,
		 PackageData *pack,
		 gboolean file_conflict,
		 GList **output)
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_UPDATE);
	PackageData *copy = packagedata_new ();
#ifdef EIP_DEBUG
	g_message ("add_update_case");
#endif /* EIP_DEBUG */

	copy->name = g_strdup (pack->name);
	copy->distribution = pack->distribution;
	copy->archtype = g_strdup (pack->archtype);

	pcase->u.update.pack = copy;
	pcase->file_conflict = file_conflict;

	if (!add_case (problem, pcase, output)) {
		add_remove_case (problem, pack, file_conflict, output);
	}
}

static void
add_cascade_remove (EazelInstallProblem *problem,
		    GList **packs,
		    gboolean file_conflict,
		    GList **output)
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new (EI_PROBLEM_CASCADE_REMOVE);

#ifdef EIP_DEBUG
	g_message ("add_cascade_remove_case");
#endif /* EIP_DEBUG */

	pcase->u.cascade.packages = (*packs);
	pcase->file_conflict = file_conflict;
	
	if (!add_case (problem, pcase, output)) {
		GList *iterator;
		for (iterator = *packs; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = PACKAGEDATA(iterator->data);
			add_force_remove_case (problem, pack, file_conflict, output);
		}
		eazel_install_problem_case_destroy (pcase);
	}
}

static void
get_detailed_cases_breaks_foreach (PackageBreaks *breakage, GetErrorsForEachData *data)
{
}

/* 
   FIXME bugzilla.eazel.com
   Needs to handle the following :
   - package status looks ok, check modification_status
*/
static void
get_detailed_cases_foreach (GtkObject *foo, 
			    GetErrorsForEachData *data)
{
	/* GList **errors = &(data->errors); */
	PackageData *previous_pack = NULL;
	PackageData *pack = NULL;
	gboolean no_problem_added = TRUE;

	if (IS_PACKAGEDATA (foo)) {
		pack = PACKAGEDATA (foo);
	} else if (IS_PACKAGEBREAKS (foo)) {
		pack = packagebreaks_get_package (PACKAGEBREAKS (foo));
	} else if (IS_PACKAGEDEPENDENCY (foo)) {
		pack = PACKAGEDEPENDENCY (foo)->package;
	} else {
		g_assert_not_reached ();
	}

#ifdef EIP_DEBUG
	g_message ("get_detailed_cases_foreach (%p)", pack);
	g_message ("get_detailed_cases_foreach (data->path = %p)", data->path);
	g_message ("get_detailed_cases_foreach (pack->status = %s)", 
		   packagedata_status_enum_to_str (pack->status));
	g_message ("get_detailed_cases_foreach (pack->modify_status = %s)", 
		   packagedata_modstatus_enum_to_str (pack->modify_status));
#endif /* EIP_DEBUG */

	if (g_list_find (data->handled, pack)) { return; }

	if (data->path) {
		previous_pack = PACKAGEDATA (data->path->data);
	}

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
		break;
	case PACKAGE_CANCELLED:
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		break;
	case PACKAGE_FILE_CONFLICT:
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		if (pack->depends) {
		} else {
			if (previous_pack && previous_pack->status == PACKAGE_BREAKS_DEPENDENCY) {
				add_update_case (data->problem, pack, FALSE, &(data->errors)); 
				no_problem_added = FALSE;
			} else {
				g_warning ("%s:%d : oops", __FILE__,__LINE__);
			}
		}
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		break;
	case PACKAGE_INVALID:
		if (previous_pack) {
			add_remove_case (data->problem, pack, FALSE, &(data->errors));
			no_problem_added = FALSE;
		}
		break;
	case PACKAGE_CANNOT_OPEN:
		break;
	case PACKAGE_PARTLY_RESOLVED:
		break;
	case PACKAGE_ALREADY_INSTALLED:
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY: 
		if (previous_pack && previous_pack->status == PACKAGE_CIRCULAR_DEPENDENCY) {
			add_force_install_both_case (data->problem, pack, previous_pack, &(data->errors));
			no_problem_added = FALSE;
		} else {
			/*
			add_cannot_solve_case (data->problem,
					       NULL,
					       &(data->errors));
			*/
		}
		break;
	case PACKAGE_RESOLVED:
		break;
	case PACKAGE_PACKSYS_FAILURE:
		break;
	}

	if (no_problem_added) {
		if (pack->status == PACKAGE_CANCELLED) {
			switch (pack->modify_status) {
			case PACKAGE_MOD_UNTOUCHED:
				break;
			case PACKAGE_MOD_DOWNGRADED:
				add_continue_with_flag_case (data->problem,
							     NULL,
							     EazelInstallProblemContinueFlag_DOWNGRADE,
							     FALSE,
							     &(data->errors));
				no_problem_added = FALSE;
				break;
			case PACKAGE_MOD_UPGRADED:
				add_continue_with_flag_case (data->problem,
							     NULL,
							     EazelInstallProblemContinueFlag_UPGRADE,
							     FALSE,
							     &(data->errors));
				no_problem_added = FALSE;
				break;
			case PACKAGE_MOD_INSTALLED:
				break;
			case PACKAGE_MOD_UNINSTALLED:
				break;
			}
		}
	}

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);
	data->handled = g_list_prepend (data->handled, pack);

	g_list_foreach (pack->depends, (GFunc)get_detailed_cases_foreach, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_cases_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_cases_breaks_foreach, data);

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
}

/* 
   FIXME bugzilla.eazel.com
   Needs to handle the following :
   - package status looks ok, check modification_status
*/
static void
get_detailed_uninstall_cases_foreach (GtkObject *foo, GetErrorsForEachData *data)
{
	PackageData *previous_pack = NULL;
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
	
#ifdef EIP_DEBUG
	g_message ("get_detailed_uninstall_cases_foreach (data->path = %p)", data->path);
	g_message ("get_detailed_uninstall_cases_foreach (pack->status = %s)", 
		   packagedata_status_enum_to_str (pack->status));
#endif /* EIP_DEBUG */

	if (data->path) {
		previous_pack = PACKAGEDATA(data->path->data);
	}

	if (pack->toplevel) {
		data->packs = NULL;
	}

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
	case PACKAGE_CANCELLED:
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		break;
	case PACKAGE_FILE_CONFLICT:
		break;
	case PACKAGE_DEPENDENCY_FAIL:
	case PACKAGE_BREAKS_DEPENDENCY:
		/* This is what a uninstall fail tree will normally have */
		gtk_object_ref (GTK_OBJECT (pack));
		data->packs = g_list_prepend (data->packs, pack);
		break;
	case PACKAGE_INVALID:
		break;
	case PACKAGE_CANNOT_OPEN:
		break;
	case PACKAGE_PARTLY_RESOLVED:
		break;
	case PACKAGE_ALREADY_INSTALLED:
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY: 
		break;
	case PACKAGE_RESOLVED:
		break;
	case PACKAGE_PACKSYS_FAILURE:
		break;
	}

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);

	g_list_foreach (pack->depends, (GFunc)get_detailed_uninstall_cases_foreach, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_uninstall_cases_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_uninstall_cases_foreach, data);

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
	if (g_list_length (data->path) == 0) {
		g_list_free (data->path);
		data->path = NULL;
	}

	if (pack->toplevel && pack->status == PACKAGE_BREAKS_DEPENDENCY) {
                /* This is just to make sure the toplevel is first */
		data->packs = g_list_reverse (data->packs);
		add_cascade_remove (data->problem, &(data->packs), FALSE, &(data->errors));
	}
}

static char*
eazel_install_problem_case_to_string (EazelInstallProblemCase *pcase, gboolean name_only)
{
	char *message = NULL;
	switch (pcase->problem) {
	case EI_PROBLEM_UPDATE: {
		char *required = packagedata_get_readable_name (pcase->u.update.pack);
		if (! name_only) {
		/* TRANSLATORS : This string is a solution to a dependency problem,
		   %s is a package name or filename */
			message = g_strdup_printf (_("Check for a new version of %s"), required);
		}
		g_free (required);
	}
	break;
	case EI_PROBLEM_FORCE_INSTALL_BOTH: {
		char *required_1 = packagedata_get_readable_name (pcase->u.force_install_both.pack_1);
		char *required_2 = packagedata_get_readable_name (pcase->u.force_install_both.pack_2);
		if (! name_only) {
		/* TRANSLATORS : This string is a solution to a dependency problem,
		   both %s's are package names or filenames */
			message = g_strdup_printf (_("Install both %s and %s"), 
						   required_1, 
						   required_2);
		}
		g_free (required_1);
		g_free (required_2);
	}
	break;
	case EI_PROBLEM_REMOVE: {
		char *required = packagedata_get_readable_name (pcase->u.remove.pack);
		if (name_only) {
			message = required;
		} else {
		/* TRANSLATORS : This string is a solution to a dependency problem,
		   %s is a package name or filename */
			message = g_strdup_printf (_("Remove %s from your system"), required);
			g_free (required);
		}
	}
	break;
	case EI_PROBLEM_FORCE_REMOVE: {
		char *required = packagedata_get_readable_name (pcase->u.force_remove.pack);
		if (name_only) {
			message = required;
		} else {
		/* TRANSLATORS : This string is a solution to a dependency problem,
		   %s is a package name or filename. "Force" is in the rpm sense of force,
		   meaning that no dependency checking etc will be done */
			message = g_strdup_printf (_("Force the removal of %s from your system"), required);
			g_free (required);
		}
	}
	break;
	case EI_PROBLEM_INCONSISTENCY: {
		if (! name_only) {
		/* TRANSLATORS : This string is a solution to a dependency problem */
			message = g_strdup (_("Package database has an inconsistency"));
		}
	}
	break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
        case EI_PROBLEM_CANNOT_SOLVE: {
		char *tmp = eazel_install_problem_case_to_string (pcase->u.cannot_solve.problem, name_only);
		if (! name_only) {
			message = g_strdup_printf ("I could not solve \"%s\"", tmp);
		}
		g_free (tmp);
	}
	break;
	case EI_PROBLEM_CASCADE_REMOVE: {
		GList *iterator;
		GString *str;

		if (name_only) {
			str = g_string_new ("");
		} else {
			str = g_string_new ("Remove these packages: ");
		}
		iterator  = pcase->u.cascade.packages;
		while (iterator) {
			PackageData *pack = PACKAGEDATA(iterator->data);
			iterator = g_list_next (iterator);
			if (iterator) {
				if (g_list_next (iterator)) {
					str = g_string_append (str, pack->name);
					str = g_string_append (str, ", ");
				} else {
					str = g_string_append (str, pack->name);
					str = g_string_append (str, ", ");
				}
			} else {
				str = g_string_append (str, pack->name);
			}
		}
		message = g_strdup (str->str);
		g_string_free (str, TRUE);
	}
	break;
	case EI_PROBLEM_CONTINUE_WITH_FLAG: {
		if (!name_only) {
			switch (pcase->u.continue_with_flag.flag) {
			case EazelInstallProblemContinueFlag_FORCE:
				message = g_strdup_printf (_("Continue with force"));
				break;
			case EazelInstallProblemContinueFlag_UPGRADE:
				message = g_strdup_printf (_("Allow upgrades"));
				break;
			case EazelInstallProblemContinueFlag_DOWNGRADE:
				message = g_strdup_printf (_("Allow downgrade"));
				break;
			}
		}
	}
	break;
	}
	return message;
}

static void 
eazel_install_problem_case_foreach_to_string (EazelInstallProblemCase *pcase,
					      GList **output)
{
	char *message = eazel_install_problem_case_to_string (pcase, FALSE);
	if (message) {
		(*output) = g_list_prepend (*output,
					    message);
	}
}

static void
eazel_install_problem_case_foreach_to_package_names (EazelInstallProblemCase *pcase,
						     GList **output)
{
	char *message = eazel_install_problem_case_to_string (pcase, TRUE);
	if (message) {
		(*output) = g_list_prepend (*output,
					    message);
	}
}

static void 
eazel_install_problem_case_foreach_destroy (EazelInstallProblemCase *pcase,
					    gpointer unused)
{
	switch (pcase->problem) {
	case EI_PROBLEM_UPDATE:
		gtk_object_unref (GTK_OBJECT (pcase->u.update.pack));
		break;
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
		gtk_object_unref (GTK_OBJECT (pcase->u.force_install_both.pack_1));
		gtk_object_unref (GTK_OBJECT (pcase->u.force_install_both.pack_2));
		break;
	case EI_PROBLEM_REMOVE:
		gtk_object_unref (GTK_OBJECT (pcase->u.remove.pack));
		break;
	case EI_PROBLEM_FORCE_REMOVE:
		gtk_object_unref (GTK_OBJECT (pcase->u.force_remove.pack));
		break;
	case EI_PROBLEM_INCONSISTENCY:
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	case EI_PROBLEM_CANNOT_SOLVE:
		eazel_install_problem_case_foreach_destroy (pcase->u.cannot_solve.problem, NULL);
		break;
	case EI_PROBLEM_CONTINUE_WITH_FLAG:
		break;
	case EI_PROBLEM_CASCADE_REMOVE:
		g_list_foreach (pcase->u.cascade.packages,
				(GFunc)gtk_object_unref,
				NULL);
		break;
	}
	g_free (pcase);
}

EazelInstallProblemCase*
eazel_install_problem_case_new (EazelInstallProblemEnum problem_type)
{
	EazelInstallProblemCase *result;
	result = g_new0 (EazelInstallProblemCase, 1);
	result->problem = problem_type;
	result->file_conflict = FALSE;
	/* g_new0 ensures that all union members are NULL */
	return result;
}

void 
eazel_install_problem_case_destroy (EazelInstallProblemCase *pcase)
{
	eazel_install_problem_case_foreach_destroy (pcase, NULL);
}

void 
eazel_install_problem_case_list_destroy (GList *list)
{
	g_list_foreach (list, (GFunc)eazel_install_problem_case_foreach_destroy, NULL);
}

EazelInstallProblem*
eazel_install_problem_new (void)
{
	EazelInstallProblem *problem;

	problem = EAZEL_INSTALL_PROBLEM (gtk_object_new (TYPE_EAZEL_INSTALL_PROBLEM, NULL));
	gtk_object_ref (GTK_OBJECT (problem));
	gtk_object_sink (GTK_OBJECT (problem));
	return problem;
}

static gboolean
finalize_attempts_hash_cleanup (gpointer key,
				GList *val,
				gpointer unused)
{
	eazel_install_problem_case_list_destroy (val);
	return TRUE;
}

static void 
eazel_install_problem_finalize (GtkObject *object) {
	EazelInstallProblem *problem;

	ASSERT_SANITY (object);	
#ifdef EIP_DEBUG
	g_message ("eazel_install_problem_finalize");
#endif
	problem = EAZEL_INSTALL_PROBLEM (object);
	g_hash_table_foreach_remove (problem->attempts,
				     (GHRFunc)finalize_attempts_hash_cleanup,
				     NULL);
	g_hash_table_destroy (problem->attempts);

	if (GTK_OBJECT_CLASS (eazel_install_problem_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_install_problem_parent_class)->finalize (object);
	}
}

static void
eazel_install_problem_class_initialize (EazelInstallProblemClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_install_problem_finalize;
}

static void
eazel_install_problem_initialize (EazelInstallProblem *problem) {
	P_SANITY (problem);
	problem->attempts = g_hash_table_new (g_int_hash, g_int_equal);
	problem->pre_step_attempts = g_hash_table_new (g_int_hash, g_int_equal);
}

GtkType 
eazel_install_problem_get_type (void)
{
	GtkType parent_type;
	static GtkType obj_type = 0;

	/* First time it's called ? */
	if (!obj_type)
	{
		static const GtkTypeInfo obj_info =
		{
			"EazelInstallProblem",
			sizeof (EazelInstallProblem),
			sizeof (EazelInstallProblemClass),
			(GtkClassInitFunc) eazel_install_problem_class_initialize,
			(GtkObjectInitFunc) eazel_install_problem_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		parent_type = gtk_object_get_type ();
		eazel_install_problem_parent_class = gtk_type_class (parent_type);
		obj_type = gtk_type_unique (parent_type, &obj_info);
	}

	return obj_type;
}

static gboolean
problem_step_foreach_remove (int *key,
			     GList *val,
			     GHashTable *joint)
{
	GList *real_list;
#ifdef EIP_DEBUG
	g_message ("problem_step_foreach_remove, key = %d", *key);
#endif
	real_list = g_hash_table_lookup (joint, key);
	if (real_list) {
		real_list = g_list_concat (real_list,
					   val);
	} else {
		real_list = val;
	}
	g_hash_table_insert (joint, key, real_list);

	return TRUE;
}

void 
eazel_install_problem_step (EazelInstallProblem *problem)
{
	P_SANITY (problem);
#ifdef EIP_DEBUG
	g_message ("g_hash_table_size (problem->pre_step_attempts) = %d", 
		   g_hash_table_size (problem->pre_step_attempts));
	g_message ("g_hash_table_size (problem->attempts) = %d", 
		   g_hash_table_size (problem->attempts));
#endif
	g_hash_table_foreach_remove (problem->pre_step_attempts,
				     (GHRFunc)problem_step_foreach_remove,
				     problem->attempts);
#ifdef EIP_DEBUG
	g_message ("g_hash_table_size (problem->pre_step_attempts) = %d", 
		   g_hash_table_size (problem->pre_step_attempts));
	g_message ("g_hash_table_size (problem->attempts) = %d", 
		   g_hash_table_size (problem->attempts));
	g_hash_table_foreach (problem->attempts,
			      (GHFunc)eazel_install_problem_debug_attempts,
			      problem);
#endif
}

/* This returns a GList<EazelInstallProblemCase> list containing the 
   encountered problems in the given PackageData tree */
void
eazel_install_problem_tree_to_case (EazelInstallProblem *problem,
				    PackageData *pack,
				    gboolean uninstall,
				    GList **output)
{
	GetErrorsForEachData data;

	P_SANITY (problem);

#ifdef EIP_DEBUG
	g_message ("eazel_install_problem_tree_to_case ( pack = %p, uninstall=%s)", 
		   pack,  
		   uninstall ? "TRUE" : "FALSE");
#endif

	data.problem = problem;
	data.errors = (*output);
	data.path = NULL;
	data.handled = NULL;
	problem->in_step_problem_mode = FALSE;

	gtk_object_ref (GTK_OBJECT (pack));

	if (uninstall) {
		get_detailed_uninstall_cases_foreach (GTK_OBJECT (pack), &data);
	} else {
		get_detailed_cases_foreach (GTK_OBJECT (pack), &data);
	}

	gtk_object_unref (GTK_OBJECT (pack));
	(*output) = data.errors;
}

/* This returns a GList<char*> list containing the 
   encountered problems in the given PackageData tree */
GList* 
eazel_install_problem_tree_to_string (EazelInstallProblem *problem,				      
				      PackageData *pack,
				      gboolean uninstall)
{
	GList *result = NULL;
	GetErrorsForEachData data;

	P_SANITY_VAL (problem, result);

	data.problem = problem;
	data.errors = NULL;
	data.path = NULL;
	data.handled = NULL;

	gtk_object_ref (GTK_OBJECT (pack));
	if (uninstall) {
		get_detailed_uninstall_messages_foreach (GTK_OBJECT (pack), &data);
	} else {
		get_detailed_messages_foreach (GTK_OBJECT (pack), &data);
	}

	gtk_object_unref (GTK_OBJECT (pack));
	result = data.errors;

	return result;
}


/* This returns a GList<char*> list containing the 
   encountered problems in the given PackageData tree */
GList* 
eazel_install_problem_cases_to_string (EazelInstallProblem *problem,
				       GList *cases)
{
	GList *result = NULL;
	g_list_foreach (cases, 
			(GFunc)eazel_install_problem_case_foreach_to_string,
			&result);
	return result;
}

/* Like above, but only returns the package names, and only for packages that are about to be removed */
GList *
eazel_install_problem_cases_to_package_names (EazelInstallProblem *problem,
					      GList *cases)
{
	GList *result = NULL;
	g_list_foreach (cases,
			(GFunc)eazel_install_problem_case_foreach_to_package_names,
			&result);
	return result;
}

EazelInstallProblemEnum
eazel_install_problem_find_dominant_problem_type (EazelInstallProblem *problem,
						  GList *problems)
{
	GList *iterator;
	EazelInstallProblemEnum dominant = EI_PROBLEM_BASE;

	for (iterator = problems; iterator; iterator = g_list_next (iterator)) {
		EazelInstallProblemCase *pcase = (EazelInstallProblemCase*)(iterator->data);
		if (pcase->problem > dominant) {
			dominant = pcase->problem;
#ifdef EIP_DEBUG
			{
				char *message = eazel_install_problem_case_to_string (pcase, FALSE);
				g_message ("dominant problem is now %d", dominant);
				g_message ("aka %s", message);
				g_free (message);
			}
#endif
		}
	}
	return dominant;
}

static GList*
find_problems_of_type (EazelInstallProblem *problem,
		       GList *problems,
		       EazelInstallProblemEnum problem_type)
{
	GList *iterator;
	GList *result = NULL;

	for (iterator = problems; iterator; iterator = g_list_next (iterator)) {
		EazelInstallProblemCase *pcase = (EazelInstallProblemCase*)(iterator->data);
		if (pcase->problem == problem_type) {
			result = g_list_prepend (result, pcase);
		}
	}

	return result;
}

static GList*
find_dominant_problems (EazelInstallProblem *problem,
			GList **problems)
{
	EazelInstallProblemEnum dominant_problem_type;
	GList *dominant_problems;
	GList *iterator;

	dominant_problem_type = eazel_install_problem_find_dominant_problem_type (problem, 
										  *problems);
	dominant_problems = find_problems_of_type (problem, *problems, dominant_problem_type);
	
	for (iterator = dominant_problems; iterator; iterator = g_list_next (iterator)) {
		(*problems) = g_list_remove (*problems, iterator->data);
	}

	if (g_list_length (*problems) == 0) {
		/* g_list_free (*problems); */
		(*problems) = NULL;
	}
	return dominant_problems;
}
      
static GList*
build_categories_from_problem_list (EazelInstallProblem *problem,
				    GList *problems)
{
	GList *iterator;
	GList *result = NULL;
	GList *packages = NULL;

	for (iterator = problems; iterator; iterator = g_list_next (iterator)) {
		EazelInstallProblemCase *pcase = (EazelInstallProblemCase*)(iterator->data);

		switch (pcase->problem) {
		case EI_PROBLEM_UPDATE:
			gtk_object_ref (GTK_OBJECT (pcase->u.update.pack));
			packages = g_list_prepend (packages, pcase->u.update.pack);
			break;
		case EI_PROBLEM_FORCE_INSTALL_BOTH:
			gtk_object_ref (GTK_OBJECT (pcase->u.force_install_both.pack_1));
			gtk_object_ref (GTK_OBJECT (pcase->u.force_install_both.pack_2));
			packages = g_list_prepend (packages, pcase->u.force_install_both.pack_1);
			packages = g_list_prepend (packages, pcase->u.force_install_both.pack_2);
			break;
		case EI_PROBLEM_REMOVE:
			gtk_object_ref (GTK_OBJECT (pcase->u.remove.pack));
			packages = g_list_prepend (packages, pcase->u.remove.pack);
			break;
		case EI_PROBLEM_FORCE_REMOVE:
			gtk_object_ref (GTK_OBJECT (pcase->u.force_remove.pack));
			packages = g_list_prepend (packages, pcase->u.force_remove.pack);
			break;
		case EI_PROBLEM_CASCADE_REMOVE: {
			GList *iterator;
			for (iterator = pcase->u.cascade.packages; iterator; iterator = g_list_next (iterator)) {
				packages = g_list_prepend (packages, iterator->data);
			}
		}
		break;
		case EI_PROBLEM_CONTINUE_WITH_FLAG:
		case EI_PROBLEM_INCONSISTENCY:
		case EI_PROBLEM_CANNOT_SOLVE:
			break;
		case EI_PROBLEM_BASE:
			g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
			g_assert_not_reached ();
			break;
		}		
	}

	if (packages) {
		CategoryData *category = categorydata_new ();
		category->name = g_strdup ("The pain...");
		category->packages = packages;
		result = g_list_prepend (result, category);
	}

	return result;
}

void
eazel_install_problem_use_set  (EazelInstallProblem *problem,
				GList *problems)
{
	GList *iterator;

	for (iterator = problems; iterator; iterator = g_list_next (iterator)) {
		EazelInstallProblemCase *pcase = (EazelInstallProblemCase*)iterator->data;
		GList *case_list;

		case_list = g_hash_table_lookup (problem->attempts, 
						 &(pcase->problem));

		case_list = g_list_prepend (case_list,
					    pcase);

		g_hash_table_insert (problem->pre_step_attempts,
				     &(pcase->problem),
				     case_list);
	}
}

/*
  If you're not satisfied with the given dominant problem,
  use this to get a peek at what the next step will be */
GList *
eazel_install_problem_step_problem (EazelInstallProblem *problem,
				    EazelInstallProblemEnum problem_type,
				    GList *problems)
{
	GList *result = NULL;
	GList *unwanted = NULL;
	GList *iterator;

	problem->in_step_problem_mode = TRUE;

	unwanted = find_problems_of_type (problem, problems, problem_type);	
	g_message ("eazel_install_problem_discard_problem %d unwanted", g_list_length (unwanted));
	for (iterator = unwanted; iterator; iterator = g_list_next (iterator)) {
		EazelInstallProblemCase *pcase = (EazelInstallProblemCase*)iterator->data;
		
		switch (pcase->problem) {
		case EI_PROBLEM_UPDATE:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			add_remove_case (problem, 
					 pcase->u.update.pack,
					 pcase->file_conflict,
					 &result);
			break;
		case EI_PROBLEM_CONTINUE_WITH_FLAG:		
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			add_cannot_solve_case (problem, 
					       pcase, 
					       &result);
			break;
		case EI_PROBLEM_FORCE_INSTALL_BOTH:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			add_continue_with_flag_case (problem, 
						     pcase, 
						     EazelInstallProblemContinueFlag_FORCE,
						     pcase->file_conflict,
						     &result);
			break;
		case EI_PROBLEM_CASCADE_REMOVE:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			add_force_remove_case (problem, 
					       pcase->u.force_remove.pack,
					       pcase->file_conflict,
					       &result);
			break;
		case EI_PROBLEM_REMOVE:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			add_force_remove_case (problem, 
					       pcase->u.remove.pack,
					       pcase->file_conflict,
					       &result);
			break;
		case EI_PROBLEM_FORCE_REMOVE:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			add_continue_with_flag_case (problem, 
						     pcase, 
						     EazelInstallProblemContinueFlag_FORCE,
						     pcase->file_conflict,
						     &result);
			break;
		case EI_PROBLEM_INCONSISTENCY:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			result = g_list_prepend (result, pcase);
			break;
		case EI_PROBLEM_CANNOT_SOLVE:
			g_message ("%s:%d, conflict = %s", __FILE__, __LINE__, pcase->file_conflict ? "TRUE":"FALSE");
			result = g_list_prepend (result, pcase);
			break;
		case EI_PROBLEM_BASE:
			g_assert_not_reached ();
			break;
		}
	}
	g_message ("eazel_install_problem_discard_problem %d left", g_list_length (result));

	return result;
}

/* #IFDEF HELL!!!
   This function can be compiled for the corba object
   or the raw gtk object...
   wonderfull eh ? */

/* Given a series of problems, it will remove
   the most important ones, and execute them given 
   a EazelInstall service object */

#ifndef EAZEL_INSTALL_NO_CORBA
static void
eazel_install_problem_done (EazelInstallCallback *service,
			    gboolean result,
			    gboolean *done)
{
	(*done) = TRUE;
}

static void
eazel_install_problem_wait_for_completion (EazelInstallProblem *problem,
					   EazelInstallCallback *service)
{
	guint handle;
	gboolean done = FALSE;
	handle = gtk_signal_connect (GTK_OBJECT (service), "done", 
				     GTK_SIGNAL_FUNC (eazel_install_problem_done), 
				     (gpointer)&done);

	while (!done) {
		g_main_iteration (TRUE);
	}

	gtk_signal_disconnect (GTK_OBJECT (service), handle);
}
#endif

/*
  FIXME:
  I need to hookup to service's "done" signal, if successfull, erase
  the contents of install_categories
 */
void 
eazel_install_problem_handle_cases (EazelInstallProblem *problem,					 
#ifdef EAZEL_INSTALL_NO_CORBA
				    EazelInstall *service,
#else /* EAZEL_INSTALL_NO_CORBA */
				    EazelInstallCallback *service,
#endif /* EAZEL_INSTALL_NO_CORBA */
				    
				    GList **problems,
				    GList **install_categories,
				    GList **uninstall_categories,
				    char *root)
{
	EazelInstallProblemEnum dominant_problem_type;
	GList *dominant_problems;
	gboolean service_force, service_update, service_downgrade, service_uninstall;
	gboolean force = FALSE, update = FALSE, downgrade = FALSE;
	GList *categories, *final_categories = NULL;
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	GNOME_Trilobite_Eazel_Install corba_service = eazel_install_callback_corba_objref (service);
#endif /* EAZEL_INSTALL_NO_CORBA */

	if (problems==NULL) { return; }
	if ((*problems)==NULL) { return; }

	P_SANITY (problem);

	/* the service_ are used to store the current values */
	service_force = FALSE; 
	service_update = FALSE; 
	service_downgrade = FALSE; 
	service_uninstall = FALSE;

	/* These are used to set new values */
	force = FALSE; 
	update = FALSE; 
	downgrade = FALSE;

	if (install_categories && uninstall_categories) {
		g_warning ("%s: didn't expect both install_categories and uninstall_categories to be set", 
			   G_GNUC_PRETTY_FUNCTION);
	}
	if (uninstall_categories) {
		service_uninstall = TRUE;
	} else {
		service_uninstall = FALSE;
	}

#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_exception_init (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	/* Parse list and find which case->problem enum is the most
	   interesting. Now extract all these from problems */
	dominant_problem_type = eazel_install_problem_find_dominant_problem_type (problem, *problems);
	dominant_problems = find_dominant_problems (problem, problems);
	categories = build_categories_from_problem_list (problem, dominant_problems);

	/* update the attemps hashtables */
	eazel_install_problem_step (problem);

	/* set up the installer object and fire off that sucker
	   First get the old values to we can reset them */
#ifdef EAZEL_INSTALL_NO_CORBA
	service_force = eazel_install_get_force (service);
	service_update = eazel_install_get_upgrade (service);
	service_downgrade = eazel_install_get_downgrade (service);
#else /* EAZEL_INSTALL_NO_CORBA */
	service_force = GNOME_Trilobite_Eazel_Install__get_force (corba_service, &ev);
	service_update = GNOME_Trilobite_Eazel_Install__get_upgrade (corba_service, &ev);
	service_downgrade = GNOME_Trilobite_Eazel_Install__get_downgrade (corba_service, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	force = service_force;
	update = service_update;
	downgrade = service_downgrade;

	/* now determine the new parameters */
	switch (dominant_problem_type) {
	case EI_PROBLEM_UPDATE:
		force = FALSE;
		update = TRUE;
		downgrade = FALSE;
		break;
	case EI_PROBLEM_CONTINUE_WITH_FLAG: {
		GList *iterator;

		for (iterator = dominant_problems; iterator; iterator = g_list_next (iterator)) {
			EazelInstallProblemCase *pcase = (EazelInstallProblemCase*)iterator->data;
			switch (pcase->u.continue_with_flag.flag) {
			case EazelInstallProblemContinueFlag_FORCE:
				force = TRUE;
				break;
			case EazelInstallProblemContinueFlag_UPGRADE:
				update = TRUE;
				break;
			case EazelInstallProblemContinueFlag_DOWNGRADE:
				downgrade = TRUE;
				break;
			}
		}
		break;
	}
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
		force = TRUE;
		update = TRUE;
		downgrade = FALSE;
		break;
	case EI_PROBLEM_CASCADE_REMOVE:
	case EI_PROBLEM_REMOVE:
		force = FALSE;
		update = TRUE;
		downgrade = FALSE;
		break;
	case EI_PROBLEM_FORCE_REMOVE:
		force = TRUE;
		update = FALSE;
		downgrade = TRUE;
		break;
	case EI_PROBLEM_INCONSISTENCY:
	case EI_PROBLEM_CANNOT_SOLVE:
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}

	/* set the new parameters */
#ifdef EAZEL_INSTALL_NO_CORBA		
	eazel_install_set_force (service, force);
	eazel_install_set_upgrade (service, update);
	eazel_install_set_downgrade (service, downgrade);
#else /* EAZEL_INSTALL_NO_CORBA */
	GNOME_Trilobite_Eazel_Install__set_force (corba_service, force, &ev);
	GNOME_Trilobite_Eazel_Install__set_upgrade (corba_service, update, &ev);
	GNOME_Trilobite_Eazel_Install__set_downgrade (corba_service, downgrade, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	/* do we add any of the usergiven cateogries ? */
	switch (dominant_problem_type) {
	case EI_PROBLEM_CASCADE_REMOVE:
		if (service_uninstall) {
			final_categories = g_list_concat (g_list_copy (categories), 
							  categorydata_list_copy (*uninstall_categories));
		} else {
			final_categories = g_list_copy (categories);
		}
		break;
	case EI_PROBLEM_CONTINUE_WITH_FLAG:
		if (service_uninstall) {
			if (!categories) {
				final_categories = categorydata_list_copy (*uninstall_categories);
			} else {
				final_categories = g_list_concat (g_list_copy (categories), 
								  categorydata_list_copy (*uninstall_categories));
			}
		} else if (install_categories) {
			if (!categories) {
				final_categories = categorydata_list_copy (*install_categories);
			} else {
				final_categories = g_list_concat (g_list_copy (categories), 
								  categorydata_list_copy (*install_categories));
			}
		} else {
			final_categories = g_list_copy (categories);
		}
		break;
	case EI_PROBLEM_UPDATE:
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
		/* Add the install_categories */
		if (install_categories) {
			final_categories = g_list_concat (g_list_copy (categories), 
							  categorydata_list_copy (*install_categories));
		}  else {
			final_categories = g_list_copy (categories);
		}
		break;
	case EI_PROBLEM_REMOVE:
	case EI_PROBLEM_FORCE_REMOVE:
		/* Add the uninstall categories */
		final_categories = g_list_copy (categories);
		break;
	case EI_PROBLEM_INCONSISTENCY:
	case EI_PROBLEM_CANNOT_SOLVE:
		g_message ("%s:%d I have no clue ?!", __FILE__, __LINE__);
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}

	/* fire it off */
	switch (dominant_problem_type) {
	case EI_PROBLEM_CONTINUE_WITH_FLAG:
		if (service_uninstall) {
#ifdef EAZEL_INSTALL_NO_CORBA
			eazel_install_uninstall_packages (service, final_categories, root);
#else /* EAZEL_INSTALL_NO_CORBA */
			eazel_install_callback_uninstall_packages (service, final_categories, root, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
		} else {
#ifdef EAZEL_INSTALL_NO_CORBA
			eazel_install_install_packages (service, final_categories, root);
#else /* EAZEL_INSTALL_NO_CORBA */
			eazel_install_callback_install_packages (service, final_categories, root, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
		}
		break;
	case EI_PROBLEM_UPDATE:
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
#ifdef EAZEL_INSTALL_NO_CORBA
		eazel_install_install_packages (service, final_categories, root);
#else /* EAZEL_INSTALL_NO_CORBA */
		eazel_install_callback_install_packages (service, final_categories, root, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
		break;
	case EI_PROBLEM_CASCADE_REMOVE:
	case EI_PROBLEM_REMOVE:
	case EI_PROBLEM_FORCE_REMOVE:
#ifdef EAZEL_INSTALL_NO_CORBA
		eazel_install_uninstall_packages (service, final_categories, root);
#else /* EAZEL_INSTALL_NO_CORBA */
		eazel_install_callback_uninstall_packages (service, final_categories, root, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
		break;
	case EI_PROBLEM_INCONSISTENCY:
	case EI_PROBLEM_CANNOT_SOLVE:
		g_message ("%s:%d I have no clue ?!", __FILE__, __LINE__);
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}

#ifndef EAZEL_INSTALL_NO_CORBA 
	eazel_install_problem_wait_for_completion (problem, service);
#endif

#ifdef EAZEL_INSTALL_NO_CORBA
	eazel_install_set_force (service, service_force);
	eazel_install_set_upgrade (service, service_update);
	eazel_install_set_downgrade (service, service_downgrade);
#else /* EAZEL_INSTALL_NO_CORBA */
	GNOME_Trilobite_Eazel_Install__set_force (corba_service, service_force, &ev);
	GNOME_Trilobite_Eazel_Install__set_upgrade (corba_service, service_update, &ev);
	GNOME_Trilobite_Eazel_Install__set_downgrade (corba_service, service_downgrade, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	/* the dominant problems list are not leaked, as they're
	 kept in problem->attempts */
	g_list_free (dominant_problems);
	categorydata_list_destroy (final_categories);

}
