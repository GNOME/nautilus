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

static GtkObjectClass *eazel_install_problem_parent_class;

#define ASSERT_SANITY(s) g_assert (s!=NULL); \
                         g_assert (IS_EAZEL_INSTALL_PROBLEM(s));

#define P_SANITY(s) g_return_if_fail (s!=NULL); \
                    g_return_if_fail (IS_EAZEL_INSTALL_PROBLEM(s));

#define P_SANITY_VAL(s,val) g_return_val_if_fail (s!=NULL, val); \
                            g_return_val_if_fail (IS_EAZEL_INSTALL_PROBLEM(s), val);


#undef EIP_DEBUG

/* Data argument to get_detailed_errors_foreach.
   Contains the installer and a path in the tree
   leading to the actual package */
typedef struct {
	EazelInstallProblem *problem;
	GList *errors;
	GList *path;
} GetErrorsForEachData;

static void
get_detailed_messages_foreach (PackageData *pack, GetErrorsForEachData *data)
{
	char *message = NULL;
	char *required = NULL;
	char *required_by = NULL;
	char *top_name = NULL;
	GList **errors = &(data->errors);
	PackageData *previous_pack = NULL;
	PackageData *top_pack = NULL;

	if (data->path) {
		previous_pack = (PackageData*)(data->path->data);
		top_pack = (PackageData*)(g_list_last (data->path)->data);
		if (top_pack == previous_pack) {
			previous_pack = NULL;
		}
		required_by = packagedata_get_readable_name (previous_pack);
		top_name = packagedata_get_readable_name (top_pack);
	}
	required = packagedata_get_readable_name (pack);

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		message = g_strdup_printf (_("%s is a source package, which is not yet supported"), 
					   required);
		break;
	case PACKAGE_FILE_CONFLICT:
		if (required_by && top_name) {
			message = g_strdup_printf (_("%s had a file conflict with %s which %s required"), 
						   required, 
						   required_by, 
						   top_name);
		} else if (top_name) {
			message = g_strdup_printf (_("%s had a file conflict with %s"), 
						   required, top_name);
		}
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		if (pack->soft_depends || pack->hard_depends) {

		} else {
			if (previous_pack->status == PACKAGE_BREAKS_DEPENDENCY) {
				if (required_by) {

				} else {
					message = g_strdup_printf (_("%s would break other packages"), required);
				}				
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
		break;
	case PACKAGE_CANNOT_OPEN:
		if (previous_pack) {
			if (previous_pack->status == PACKAGE_DEPENDENCY_FAIL) {
				message = g_strdup_printf (_("%s requires %s, which could not be found on the server"), 
							   required_by,required);
			}
		} else {
			message = g_strdup_printf (_("%s could not be found on the server"), 
						   required);
		}
		break;
	case PACKAGE_PARTLY_RESOLVED:
		break;
	case PACKAGE_ALREADY_INSTALLED:
		message = g_strdup_printf (_("%s is already installed"), required);
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY: 
		if (previous_pack->status == PACKAGE_CIRCULAR_DEPENDENCY) {
			if (g_list_length (data->path) >= 3) {
				PackageData *causing_package = (PackageData*)((g_list_nth (data->path, 1))->data);
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
		break;
	case PACKAGE_RESOLVED:
		break;
	}

	if (message != NULL) {
		(*errors) = g_list_append (*errors, message);
	} else {
		switch (pack->modify_status) {
		case PACKAGE_MOD_UNTOUCHED:
			break;
		case PACKAGE_MOD_UPGRADED:
			break;
		case PACKAGE_MOD_DOWNGRADED:
			message = g_strdup_printf (_("%s, which is newer, is already installed"), 
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

	g_list_foreach (pack->soft_depends, (GFunc)get_detailed_messages_foreach, data);
	g_list_foreach (pack->hard_depends, (GFunc)get_detailed_messages_foreach, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_messages_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_messages_foreach, data);

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
	g_message ("add_case");
#endif /* EIP_DEBUG */

	/* Did a previous atttempt generate this case ? */
	case_list = g_hash_table_lookup (problem->attempts, 
					 &(pcase->problem));
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
		case_list = g_list_prepend (case_list,
					    pcase);
		g_hash_table_insert (problem->pre_step_attempts,
				     &(pcase->problem),
				     case_list);
		(*output) = g_list_prepend (*output,
					    pcase);
	} else {
#ifdef EIP_DEBUG
		g_message ("  already_attempted 2");
#endif /* EIP_DEBUG */
	}

	return TRUE;
}

static void
add_force_install_both_case (EazelInstallProblem *problem,
			     const PackageData *pack_1, 
			     const PackageData *pack_2,
			     GList **output) 
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new ();
	PackageData *copy_1 = packagedata_copy (pack_1, FALSE);
	PackageData *copy_2 = packagedata_copy (pack_2, FALSE);

#ifdef EIP_DEBUG
	g_message ("add_force_install_both_case");
#endif /* EIP_DEBUG */

	pcase->problem = EI_PROBLEM_FORCE_INSTALL_BOTH;
	pcase->u.force_install_both.pack_1 = copy_1;
	pcase->u.force_install_both.pack_2 = copy_2;
	
	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		g_warning ("%s:%d : oops", __FILE__,__LINE__);
	}
}

static void
add_force_remove_case (EazelInstallProblem *problem,
		       const PackageData *pack,
		       GList **output) 
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new ();
	PackageData *copy = packagedata_copy (pack, FALSE);

#ifdef EIP_DEBUG
	g_message ("add_force_remove_case");
#endif /* EIP_DEBUG */

	pcase->problem = EI_PROBLEM_FORCE_REMOVE;
	pcase->u.force_remove.pack = copy;
	
	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		g_warning ("%s:%d : oops", __FILE__,__LINE__);
	}
}

static void
add_remove_case (EazelInstallProblem *problem,
		 const PackageData *pack,
		 GList **output) 
{

	EazelInstallProblemCase *pcase = eazel_install_problem_case_new ();
	PackageData *copy = packagedata_copy (pack, FALSE);

#ifdef EIP_DEBUG
	g_message ("add_remove_case");
#endif /* EIP_DEBUG */

	pcase->problem = EI_PROBLEM_REMOVE;
	pcase->u.remove.pack = copy;
	
	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		add_force_remove_case (problem, pack, output);
	}
}

static void
add_update_case (EazelInstallProblem *problem,
		 const PackageData *pack,
		 GList **output)
{
	EazelInstallProblemCase *pcase = eazel_install_problem_case_new ();
	PackageData *copy = packagedata_new ();

#ifdef EIP_DEBUG
	g_message ("add_update_case");
#endif /* EIP_DEBUG */

	copy->name = g_strdup (pack->name);
	copy->distribution = pack->distribution;
	copy->archtype = g_strdup (pack->archtype);

	pcase->problem = EI_PROBLEM_UPDATE;
	pcase->u.update.pack = copy;
	
	if (!add_case (problem, pcase, output)) {
		eazel_install_problem_case_destroy (pcase);
		add_remove_case (problem, pack, output);
	}
}

/* 
   FIXME bugzilla.eazel.com
   Needs to handle the following :
   - package status looks ok, check modification_status
*/
static void
get_detailed_cases_foreach (PackageData *pack, GetErrorsForEachData *data)
{
	/* GList **errors = &(data->errors); */
	PackageData *previous_pack = NULL;

#ifdef EIP_DEBUG
	g_message ("get_detailed_cases_foreach (data->path = 0x%x)", (int)(data->path));
	g_message ("get_detailed_cases_foreach (pack->status = %s)", 
		   packagedata_status_enum_to_str (pack->status));
#endif /* EIP_DEBUG */

	if (data->path) {
		previous_pack = (PackageData*)(data->path->data);
	}

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		break;
	case PACKAGE_FILE_CONFLICT:
		if ((pack->name!= NULL) && previous_pack && (strcmp (pack->name, previous_pack->name) != 0)) {
			add_update_case (data->problem, pack, &(data->errors)); 
		} else {
			g_warning ("%s:%d : oops", __FILE__,__LINE__);
		}
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		if (pack->soft_depends || pack->hard_depends) {
		} else {
			if (previous_pack && previous_pack->status == PACKAGE_BREAKS_DEPENDENCY) {
				add_update_case (data->problem, pack, &(data->errors)); 
			} else {
				g_warning ("%s:%d : oops", __FILE__,__LINE__);
			}
		}
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
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
		if (previous_pack && previous_pack->status == PACKAGE_CIRCULAR_DEPENDENCY) {
			add_force_install_both_case (data->problem, pack, previous_pack, &(data->errors));
		} else {
			g_warning ("%s:%d : oops", __FILE__,__LINE__);
		}
		break;
	case PACKAGE_RESOLVED:
		break;
	}

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);

	g_list_foreach (pack->soft_depends, (GFunc)get_detailed_cases_foreach, data);
	g_list_foreach (pack->hard_depends, (GFunc)get_detailed_cases_foreach, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_cases_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_cases_foreach, data);

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
}

static char*
eazel_install_problem_case_to_string (EazelInstallProblemCase *pcase)
{
	char *message = NULL;
	switch (pcase->problem) {
	case EI_PROBLEM_UPDATE: {
		char *required = packagedata_get_readable_name (pcase->u.update.pack);
		message = g_strdup_printf ("Update %s", required);
		g_free (required);
	}
	break;
	case EI_PROBLEM_FORCE_INSTALL_BOTH: {
		char *required_1 = packagedata_get_readable_name (pcase->u.force_install_both.pack_1);
		char *required_2 = packagedata_get_readable_name (pcase->u.force_install_both.pack_2);
		message = g_strdup_printf ("Install both %s and %s", 
					   required_1, 
					   required_2);
		g_free (required_1);
		g_free (required_2);
	}
	break;
	case EI_PROBLEM_REMOVE: {
		char *required = packagedata_get_readable_name (pcase->u.remove.pack);
		message = g_strdup_printf ("Remove %s", required);
		g_free (required);
	}
	break;
	case EI_PROBLEM_FORCE_REMOVE: {
		char *required = packagedata_get_readable_name (pcase->u.force_remove.pack);
		message = g_strdup_printf ("Force remove %s", required);
		g_free (required);
	}
	break;
	case EI_PROBLEM_INCONSISTENCY: {
		message = g_strdup ("Package database has an inconsistency");
	}
	break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}
	return message;
}

static void 
eazel_install_problem_case_foreach_to_string (EazelInstallProblemCase *pcase,
					      GList **output)
{
	char *message = eazel_install_problem_case_to_string (pcase);
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
		packagedata_destroy (pcase->u.update.pack, TRUE);
		break;
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
		packagedata_destroy (pcase->u.force_install_both.pack_1, TRUE);
		packagedata_destroy (pcase->u.force_install_both.pack_2, TRUE);
		break;
	case EI_PROBLEM_REMOVE:
		packagedata_destroy (pcase->u.remove.pack, TRUE);
		break;
	case EI_PROBLEM_FORCE_REMOVE:
		packagedata_destroy (pcase->u.force_remove.pack, TRUE);
		break;
	case EI_PROBLEM_INCONSISTENCY:
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}
	g_free (pcase);
}

EazelInstallProblemCase*
eazel_install_problem_case_new ()
{
	EazelInstallProblemCase *result;
	result = g_new0 (EazelInstallProblemCase, 1);
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

static void 
eazel_install_problem_finalize (GtkObject *object) {
	ASSERT_SANITY (object);	

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
#endif
}

/* This returns a GList<EazelInstallProblemCase> list containing the 
   encountered problems in the given PackageData tree */
void
eazel_install_problem_tree_to_case (EazelInstallProblem *problem,
				    const PackageData *pack,
				    GList **output)
{
	GetErrorsForEachData data;
	PackageData *pack_copy;

	P_SANITY (problem);

	data.problem = problem;
	data.errors = (*output);
	data.path = NULL;
	pack_copy = packagedata_copy (pack, TRUE);

	get_detailed_cases_foreach (pack_copy, &data);

	packagedata_destroy (pack_copy, TRUE);
	(*output) = data.errors;
}

/* This returns a GList<char*> list containing the 
   encountered problems in the given PackageData tree */
GList* 
eazel_install_problem_tree_to_string (EazelInstallProblem *problem,
				      const PackageData *pack)
{
	GList *result = NULL;
	GetErrorsForEachData data;
	PackageData *pack_copy;

	P_SANITY_VAL (problem, result);

	data.problem = problem;
	data.errors = NULL;
	data.path = NULL;
	pack_copy = packagedata_copy (pack, TRUE);

	get_detailed_messages_foreach (pack_copy, &data);

	packagedata_destroy (pack_copy, TRUE);
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

static EazelInstallProblemEnum
find_dominant_problem_type (EazelInstallProblem *problem,
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
				char *message = eazel_install_problem_case_to_string (pcase);
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

	dominant_problem_type = find_dominant_problem_type (problem, *problems);
	dominant_problems = find_problems_of_type (problem, *problems, dominant_problem_type);
	
	for (iterator = dominant_problems; iterator; iterator = g_list_next (iterator)) {
		(*problems) = g_list_remove (*problems, iterator->data);
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
			packages = g_list_prepend (packages, 
						   packagedata_copy (pcase->u.update.pack, TRUE));
			break;
		case EI_PROBLEM_FORCE_INSTALL_BOTH:
			packages = g_list_prepend (packages, 
						   packagedata_copy (pcase->u.force_install_both.pack_1, TRUE));
			packages = g_list_prepend (packages, 
						   packagedata_copy (pcase->u.force_install_both.pack_2, TRUE));
			break;
		case EI_PROBLEM_REMOVE:
			packages = g_list_prepend (packages, 
						   packagedata_copy (pcase->u.remove.pack, TRUE));
			break;
		case EI_PROBLEM_FORCE_REMOVE:
			packages = g_list_prepend (packages, 
						   packagedata_copy (pcase->u.force_remove.pack, TRUE));
			break;
		case EI_PROBLEM_INCONSISTENCY:
			g_message ("%s:%d I have no clue ?!", __FILE__, __LINE__);
			break;
		case EI_PROBLEM_BASE:
			g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
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


/* #IFDEF HELL!!!
   This function can be compiled for the corba object
   or the raw gtk object...
   wonderfull eh ? */

/* Given a series of problems, it will remove
   the most important ones, and execute them given 
   a EazelInstall service object */
void 
eazel_install_problem_handle_cases (EazelInstallProblem *problem,					 
#ifdef EAZEL_INSTALL_NO_CORBA
				    EazelInstall *service,
#else /* EAZEL_INSTALL_NO_CORBA */
				    EazelInstallCallback *service,
#endif /* EAZEL_INSTALL_NO_CORBA */
				    
				    GList **problems,
				    char *root)
{
	EazelInstallProblemEnum dominant_problem_type;
	GList *dominant_problems;
	gboolean service_force, service_update, service_downgrade;
	gboolean force, update, downgrade;
	GList *categories;
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	Trilobite_Eazel_Install corba_service = eazel_install_callback_corba_objref (service);
#endif /* EAZEL_INSTALL_NO_CORBA */

	if (problems==NULL) { return; }
	if ((*problems)==NULL) { return; }

	P_SANITY (problem);

#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_exception_init (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	/* Parse list and find which case->problem enum is the most
	   interesting. Now extract all these from problems */
	dominant_problem_type = find_dominant_problem_type (problem, *problems);
	dominant_problems = find_dominant_problems (problem, problems);
	categories = build_categories_from_problem_list (problem, dominant_problems);

	/* update the attemps hashtables */
	eazel_install_problem_step (problem);

	/* set up the installer object and fire off that sucker
	   First get the old values to we can reset them */
#ifdef EAZEL_INSTALL_NO_CORBA
	service_force = eazel_install_get_force (service);
	service_update = eazel_install_get_update (service);
	service_downgrade = eazel_install_get_downgrade (service);
#else /* EAZEL_INSTALL_NO_CORBA */
	service_force = Trilobite_Eazel_Install__get_force (corba_service, &ev);
	service_update = Trilobite_Eazel_Install__get_update (corba_service, &ev);
	service_downgrade = Trilobite_Eazel_Install__get_downgrade (corba_service, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	/* now determine the new parameters */
	switch (dominant_problem_type) {
	case EI_PROBLEM_UPDATE:
		force = FALSE;
		update = TRUE;
		downgrade = FALSE;
		break;
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
		force = TRUE;
		update = TRUE;
		downgrade = TRUE;
		break;
	case EI_PROBLEM_REMOVE:
		force = FALSE;
		update = TRUE;
		downgrade = FALSE;
		break;
	case EI_PROBLEM_FORCE_REMOVE:
		force = TRUE;
		update = TRUE;
		downgrade = FALSE;
		break;
	case EI_PROBLEM_INCONSISTENCY:
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}

	/* set the new parameters */
#ifdef EAZEL_INSTALL_NO_CORBA		
	eazel_install_set_force (service, force);
	eazel_install_set_update (service, update);
	eazel_install_set_downgrade (service, downgrade);
#else /* EAZEL_INSTALL_NO_CORBA */
	Trilobite_Eazel_Install__set_force (corba_service, service_force, &ev);
	Trilobite_Eazel_Install__set_update (corba_service, service_update, &ev);
	Trilobite_Eazel_Install__set_downgrade (corba_service, service_downgrade, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	/* fire it off */
	switch (dominant_problem_type) {
	case EI_PROBLEM_UPDATE:
	case EI_PROBLEM_FORCE_INSTALL_BOTH:
#ifdef EAZEL_INSTALL_NO_CORBA
		eazel_install_install_packages (service, categories, root);
#else /* EAZEL_INSTALL_NO_CORBA */
		eazel_install_callback_install_packages (service, categories, root, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
		break;
	case EI_PROBLEM_REMOVE:
	case EI_PROBLEM_FORCE_REMOVE:
#ifdef EAZEL_INSTALL_NO_CORBA
		eazel_install_uninstall_packages (service, categories, root);
#else /* EAZEL_INSTALL_NO_CORBA */
		eazel_install_callback_uninstall_packages (service, categories, root, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
		break;
	case EI_PROBLEM_INCONSISTENCY:
		g_message ("%s:%d I have no clue ?!", __FILE__, __LINE__);
		break;
	case EI_PROBLEM_BASE:
		g_warning ("%s:%d: should not be reached", __FILE__, __LINE__);
		break;
	}

#ifdef EAZEL_INSTALL_NO_CORBA
	eazel_install_set_force (service, service_force);
	eazel_install_set_update (service, service_update);
	eazel_install_set_downgrade (service, service_downgrade);
#else /* EAZEL_INSTALL_NO_CORBA */
	Trilobite_Eazel_Install__set_force (corba_service, service_force, &ev);
	Trilobite_Eazel_Install__set_update (corba_service, service_update, &ev);
	Trilobite_Eazel_Install__set_downgrade (corba_service, service_downgrade, &ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */

	eazel_install_problem_case_list_destroy (dominant_problems);
	categorydata_list_destroy (categories);

}
