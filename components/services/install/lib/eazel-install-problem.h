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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 */


/*
  How to use :
  Create an instance e of EazelInstallProblem pr. complete operation you're going to try.
  (Eg. installing nautilus). Whenever the install_failed signal is called,
  call eazel_install_problem_tree_to_case (e, package). This will return
  a list of problems (you can call several times before pr. operation attempt.
  Note, that every case is only reported pr tree. So if you're installing package
  A and B, and they both cause case C, you can get case C twice.
  Now do with these cases as you want, optionally call
  eazel_install_problem_handle_cases untill the problem list is empty. If
  you do not use eazel_install_problem_handle_cases, but manually prioritize the
  problems, be sure to call eazel_install_problem_step between operation executions.
 */

#ifndef EAZEL_INSTALL_PROBLEM_H
#define EAZEL_INSTALL_PROBLEM_H

#include "eazel-package-system-types.h"
#ifdef EAZEL_INSTALL_NO_CORBA
#include "eazel-install-public.h"
#else /* EAZEL_INSTALL_NO_CORBA */
#include "eazel-install-corba-callback.h"
#endif /* EAZEL_INSTALL_NO_CORBA */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_INSTALL_PROBLEM           (eazel_install_problem_get_type ())
#define EAZEL_INSTALL_PROBLEM(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_INSTALL_PROBLEM, EazelInstallProblem))
#define EAZEL_INSTALL_PROBLEM_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_INSTALL_PROBLEM, EazelInstallProblemClass))
#define EAZEL_IS_INSTALL_PROBLEM(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_INSTALL_PROBLEM))
#define EAZEL_IS_INSTALL_PROBLEM_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_INSTALL_PROBLEM))
	
typedef enum _EazelInstallProblemContinueFlag EazelInstallProblemContinueFlag;
typedef enum _EazelInstallProblemEnum EazelInstallProblemEnum;
typedef struct _EazelInstallProblemCase EazelInstallProblemCase;
typedef struct _EazelInstallProblemClass EazelInstallProblemClass;
typedef struct _EazelInstallProblem EazelInstallProblem;

/* Please, in switches on this enum, do not handle
   default, but always handle all the enums.
   That ways you cannot add a enum without
   getting compiler borkage to let you know you're
   about to fuck up. */

enum _EazelInstallProblemEnum {
	EI_PROBLEM_BASE = 0,
	EI_PROBLEM_UPDATE,                 /* package is in the way, update or remove */
	EI_PROBLEM_CONTINUE_WITH_FLAG,     /* Ok, just barge ahead with the operation */
	EI_PROBLEM_REMOVE,                 /* Remove a package might help */
	EI_PROBLEM_FORCE_INSTALL_BOTH,     /* two packages are fighting it out, install both ? */	
	EI_PROBLEM_CASCADE_REMOVE,         /* Delete a lot of stuff */
        EI_PROBLEM_FORCE_REMOVE,           /* Okaeh, force remove the package */
	EI_PROBLEM_CANNOT_SOLVE,           /* The water is too deep */
        EI_PROBLEM_INCONSISTENCY           /* There's an inconsistency in the db */ 
};

enum _EazelInstallProblemContinueFlag {
	EazelInstallProblemContinueFlag_FORCE = 0x1,
	EazelInstallProblemContinueFlag_UPGRADE = 0x2,
	EazelInstallProblemContinueFlag_DOWNGRADE = 0x4
};

struct _EazelInstallProblemCase {
	EazelInstallProblemEnum problem;
	gboolean file_conflict;
	union {
		struct {
			EazelInstallProblemContinueFlag flag;
		} continue_with_flag;
		struct {
			PackageData *pack;
		} update;
		struct {
			PackageData *pack_1;
			PackageData *pack_2;
		} force_install_both;
		struct {
			PackageData *pack;
		} remove;
		struct {
			PackageData *pack;
		} force_remove;
		struct {
			EazelInstallProblemCase *problem;
		} cannot_solve;
		struct {
			EazelInstallProblemCase *problem;
		} force;
		struct {
			GList *packages;
		} cascade;
	} u;
};

EazelInstallProblemCase *eazel_install_problem_case_new (EazelInstallProblemEnum problem_type);
void eazel_install_problem_case_destroy (EazelInstallProblemCase *pcase);
void eazel_install_problem_case_list_destroy (GList *list);

struct _EazelInstallProblemClass {
	GtkObjectClass parent_class;
};

struct _EazelInstallProblem {
	/* Parent stuff */
	GtkObject parent;

	/* Private vars */
	/* The fist set of attributes are lists of previously encountered problems.
	   If I'm about to add a problem with the same stuff, don't,
	   but "upgrade it".
	   Eg for install:
	   EI_PROBLEM_UPDATE -> EI_PROBLEM_REMOVE
	   EI_PROBLEM_REMOVE -> EI_PROBLEM_CASCADE_REMOVE
	   EI_PROBLEM_CASCADE_REMOVE -> EI_PROBLEM_FORCE_REMOVE
	   EI_PROBLEM_FORCE_REMOVE -> EI_PROBLEM_CONTINUE_WITH_FORCE
	   EI_PROBLEM_CONTINUE_WITH_FORCE -> EI_PROBLEM_CANNOT_SOLVE

	   Uninstall will start at EI_PROBLEM_CASCADE_REMOVE
	   for uninstall, EI_PROBLEM_CONTINUE_WITH_FORCE == EI_PROBLEM_FORCE_REMOVE

	   This logic is implemented in the add_*_case functions and
	   in eazel_install_problem_step_problem.
	*/

	GHashTable *attempts;
	/* This is the list of problems currently being build,
	   called eazel_install_problem_step moves these into
	   atttempts */
	GHashTable *pre_step_attempts;
	gboolean in_step_problem_mode;
};

EazelInstallProblem *eazel_install_problem_new (void);
GtkType eazel_install_problem_get_type (void);       

void eazel_install_problem_step (EazelInstallProblem *problem);

/* This returns a GList<EazelInstallProblemCase> list containing the 
   encountered problems in the given PackageData tree */
void eazel_install_problem_tree_to_case (EazelInstallProblem *problem,
					 const PackageData *pack,
					 gboolean uninstall,
					 GList **output);

/* This returns a GList<char*> list containing the 
   encountered problems in the given PackageData tree */
GList * eazel_install_problem_tree_to_string (EazelInstallProblem *problem,
					      PackageData *pack,
					      gboolean uninstall);

/* This returns a GList<char*> list containing the 
   encountered problems in the given PackageData tree */
GList * eazel_install_problem_cases_to_string (EazelInstallProblem *problem,
					       GList *cases);

/* Like above, but only returns the package names, and only for packages that are about to be removed */
GList * eazel_install_problem_cases_to_package_names (EazelInstallProblem *problem,
						      GList *cases);

/* This lets you know which type of problems the next call
   to eazel_install_handle_cases will handle. So if eg. 
   the next type is EI_PROBLEM_FORCE_REMOVE, you can alert the user */
EazelInstallProblemEnum 
eazel_install_problem_find_dominant_problem_type (EazelInstallProblem *problem,
						  GList *problems);

/*
  If you're not satisfied with the given dominant problem,
  use this to get a peek at what the next step will be.
  If you like the result, you can g_list_free the "problems", 
  if not, g_list_free the result. Do _not_ destroy the 
  problem's themselves.
  If you like the result, pass them to eazel_install_problem_use_set 
*/
GList *
eazel_install_problem_step_problem (EazelInstallProblem *problem,
				    EazelInstallProblemEnum problem_type,
				    GList *problems);

void
eazel_install_problem_use_set  (EazelInstallProblem *problem,
				GList *problems);

/* Given a series of problems, it will remove
   the most important ones, and execute them given 
   a EazelInstall service object */
void eazel_install_problem_handle_cases (EazelInstallProblem *problem,					 
#ifdef EAZEL_INSTALL_NO_CORBA
					 EazelInstall *service,
#else /* EAZEL_INSTALL_NO_CORBA */
					 EazelInstallCallback *service,
#endif /* EAZEL_INSTALL_NO_CORBA */

					 GList **problems,
					 GList **install_categories,
					 GList **uninstall_categories,
					 char *root);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALL_PROBLEM_H */
