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
 *
 */

#include <config.h>
#include <gnome.h>
#include <gmodule.h>
#include "eazel-package-system-private.h"
#include <libtrilobite/trilobite-core-distribution.h>
#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/trilobite-md5-tools.h>

#undef EPS_DEBUG

enum {
	START,
	END,
	PROGRESS,
	FAILED,
	LAST_SIGNAL
};

/* The signal array, used for building the signal bindings */
static guint signals[LAST_SIGNAL] = { 0 };
/* This is the parent class pointer */
static GtkObjectClass *eazel_package_system_parent_class;

/*****************************************/

EazelPackageSystemId 
eazel_package_system_suggest_id ()
{
	EazelPackageSystemId  result = EAZEL_PACKAGE_SYSTEM_UNSUPPORTED;
	DistributionInfo dist = trilobite_get_distribution ();

	switch (dist.name) {
	case DISTRO_REDHAT: 
		if (dist.version_major == 6) {
			result = EAZEL_PACKAGE_SYSTEM_RPM_3;
		} else {
			result = EAZEL_PACKAGE_SYSTEM_RPM_4;
		}
		break;
	case DISTRO_MANDRAKE: 
		result = EAZEL_PACKAGE_SYSTEM_RPM_3;
		break;
	case DISTRO_YELLOWDOG:
		result = EAZEL_PACKAGE_SYSTEM_UNSUPPORTED;
		break;
	case DISTRO_TURBOLINUX: 
		result = EAZEL_PACKAGE_SYSTEM_RPM_3;
		break;
	case DISTRO_COREL: 
		result = EAZEL_PACKAGE_SYSTEM_DEB;
		break;
	case DISTRO_DEBIAN: 
		result = EAZEL_PACKAGE_SYSTEM_DEB;
		break;
	case DISTRO_CALDERA: 
		result = EAZEL_PACKAGE_SYSTEM_RPM_3;
		break;
	case DISTRO_SUSE: 
		result = EAZEL_PACKAGE_SYSTEM_RPM_3;
		break;
	case DISTRO_LINUXPPC: 
		result = EAZEL_PACKAGE_SYSTEM_RPM_3;
		break;
	case DISTRO_UNKNOWN:
		result = EAZEL_PACKAGE_SYSTEM_UNSUPPORTED;
		break;
	}
	return result;		
}

#ifdef EAZEL_INSTALL_SLIM
EazelPackageSystem *eazel_package_system_implementation (GList *roots);
#endif

static EazelPackageSystem*
eazel_package_system_load_implementation (EazelPackageSystemId id, GList *roots)
{
	EazelPackageSystem *result = NULL;
	EazelPackageSystemConstructorFunc const_func = NULL;
	GModule *module = NULL;

#ifdef EAZEL_INSTALL_SLIM
	return eazel_package_system_implementation (roots);
#endif

	switch (id) {
	case EAZEL_PACKAGE_SYSTEM_DEB:
		module = g_module_open ("libeazelpackagesystem-dpkg.so", G_MODULE_BIND_LAZY);
		break;
	case EAZEL_PACKAGE_SYSTEM_RPM_3:
		module = g_module_open ("libeazelpackagesystem-rpm3.so", G_MODULE_BIND_LAZY);
		break;
	case EAZEL_PACKAGE_SYSTEM_RPM_4:
		module = g_module_open ("libeazelpackagesystem-rpm4.so", G_MODULE_BIND_LAZY);
		break;
#if 0	/* someday */
	case EAZEL_PACKAGE_SYSTEM_BUILTIN:
		module = g_module_open (NULL /* myself */, G_MODULE_BIND_LAZY);
		break;
#endif
	default:
		trilobite_debug ("EPS: Unsupported System");
	};

	if (module==NULL) {
		g_warning ("gmodule: %s", g_module_error ());
	} else {
		g_module_make_resident (module);
		g_module_symbol (module, "eazel_package_system_implementation", (gpointer)&const_func);
		result = (*const_func)(roots);
	}

	return result;
}

static gboolean
eazel_package_system_matches_versioning (EazelPackageSystem *package_system,
					 PackageData *pack,
					 const char *version,
					 const char *minor,
					 EazelSoftCatSense sense)
{
	gboolean version_result = FALSE;
	gboolean minor_result = FALSE;
	int result;

	g_assert (!((version == NULL) && (minor != NULL)));

	if (version != NULL) {
		result = eazel_package_system_compare_version (package_system, pack->version, version);
		if ((sense & EAZEL_SOFTCAT_SENSE_EQ) && (result == 0)) {
			version_result = TRUE;
		}
		if ((sense & EAZEL_SOFTCAT_SENSE_GT) && (result > 0)) {
			version_result = TRUE;
		}
		if ((sense & EAZEL_SOFTCAT_SENSE_LT) && (result < 0)) {
			version_result = TRUE;
		}
	} else {
		version_result = TRUE;
	}

	if (minor != NULL) {
		result = eazel_package_system_compare_version (package_system, pack->minor, minor);
		if ((sense & EAZEL_SOFTCAT_SENSE_EQ) && (result == 0)) {
			minor_result = TRUE;
		}
		if ((sense & EAZEL_SOFTCAT_SENSE_GT) && (result > 0)) {
			minor_result = TRUE;
		}			
		if ((sense & EAZEL_SOFTCAT_SENSE_LT) && (result < 0)) {
			minor_result = TRUE;
		}			
	} else {
		minor_result = TRUE;
	}

	return (version_result && minor_result);
}

gboolean             
eazel_package_system_is_installed (EazelPackageSystem *package_system,
				   const char *dbpath,
				   const char *name,
				   const char *version,
				   const char *minor,
				   EazelSoftCatSense version_sense)
{
	GList *matches;
	gboolean result = FALSE;
	
#if EPS_DEBUG
	trilobite_debug ("eazel_package_system_is_installed (..., %s, %s, %s, %d)",
			 name, version, minor, version_sense);
#endif			 

	matches = eazel_package_system_query (package_system,
					      dbpath,
					      (const gpointer)name,
					      EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					      PACKAGE_FILL_MINIMAL);

	if (matches) {
		if (version || minor) {
			GList *iterator;

			for (iterator = matches; iterator && !result; iterator = g_list_next (iterator)) {
				PackageData *pack = (PackageData*)iterator->data;				
										  
				if (eazel_package_system_matches_versioning (package_system,
									     pack, 
									     version, 
									     minor, 
									     version_sense)) {
					result = TRUE;
					
				}
#if EPS_DEBUG
				g_message("is_installed (%s, %s, %s, %d) == (%s-%s %d %s-%s) %s", 
					  name, version, minor, version_sense, 
					  pack->version, pack->minor,
					  version_sense, 
					  version, minor, 
					  result ? "TRUE" : "FALSE");
#endif
			}
		} else {
			result = TRUE;
		}
		g_list_foreach (matches, (GFunc)gtk_object_unref, NULL);
	}
	g_list_free (matches);
	
	return result;
}

PackageData*
eazel_package_system_load_package (EazelPackageSystem *system,
				   PackageData *in_package,
				   const char *filename,
				   int detail_level)
{
	PackageData *result = NULL;
	char md5[16];

	EPS_SANE_VAL (system, NULL);

	if (system->private->load_package) {
		result = (*system->private->load_package) (system, in_package, filename, detail_level);
		if (result) {
			trilobite_md5_get_digest_from_file (filename, md5);
			result->md5 = g_strdup (trilobite_md5_get_string_from_md5_digest (md5));
		}
	}

	return result;
}

GList*               
eazel_package_system_query (EazelPackageSystem *system,
			    const char *root,
			    const gpointer key,
			    EazelPackageSystemQueryEnum flag,
			    int detail_level)
{
	GList *result = NULL;

	EPS_SANE_VAL (system, NULL);

	if (system->private->query) {
		g_assert (key);
		result = (*system->private->query) (system, root, key, flag, detail_level);
	}

	return result;
}

static void
eazel_package_system_fail_all_packages (EazelPackageSystem *system, 
					EazelPackageSystemOperation op, 
					GList *packages)
{
	GList *iterator;
	
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *p = PACKAGEDATA (iterator->data);
		eazel_package_system_emit_start (system, op, p);
		eazel_package_system_emit_failed (system, op, p);			
		eazel_package_system_emit_end (system, op, p);		
	}
}

void                 
eazel_package_system_install (EazelPackageSystem *system, 
			      const char *root,
			      GList* packages,
			      unsigned long flags)
{
	EPS_SANE (system);

	if (system->private->install) {		
		/* If we're in test mode, disable FORCE just to trigger
		   any potiental errors */
		if (flags & EAZEL_PACKAGE_SYSTEM_OPERATION_TEST) {
			(*system->private->install) (system, root, packages, 
						     flags & ~EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE);
		} else {
			(*system->private->install) (system, root, packages, flags);
		}
	} else {
		eazel_package_system_fail_all_packages (system, 
							EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL, 
							packages);
	}
}

void                 
eazel_package_system_uninstall (EazelPackageSystem *system, 
				const char *root,
				GList* packages,
				unsigned long flags)
{
	EPS_SANE (system);

	if (system->private->uninstall) {
		(*system->private->uninstall) (system, root, packages, flags);
	} else {
		eazel_package_system_fail_all_packages (system, 
							EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL, 
							packages);
	} 
}

gboolean
eazel_package_system_verify (EazelPackageSystem *system, 
			     const char *dbpath,
			     GList* packages)
{
	EPS_SANE_VAL (system, FALSE);
	if (system->private->verify) {
		return (*system->private->verify) (system, dbpath, packages);	
	} else {
		eazel_package_system_fail_all_packages (system, 
							EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY, 
							packages);
		return FALSE;
	}
}

int
eazel_package_system_compare_version (EazelPackageSystem *system,
				      const char *a,
				      const char *b)
{
	int result;
	EPS_SANE_VAL (system, 0);
	g_assert (system->private->compare_version);
	result = (*system->private->compare_version) (system, a, b);
	return result;
}

time_t
eazel_package_system_database_mtime (EazelPackageSystem *system)
{
	time_t result;
	EPS_SANE_VAL (system, 0);
	if (system->private->database_mtime == NULL) {
		return 0;
	}
	result = (*system->private->database_mtime) (system);
	return result;
}

/******************************************
 The private emitter functions
*******************************************/

gboolean 
eazel_package_system_emit_start (EazelPackageSystem *system, 
				 EazelPackageSystemOperation op, 
				 const PackageData *package)
{
	gboolean result = TRUE;
	EPS_API (system);
	gtk_signal_emit (GTK_OBJECT (system),
			 signals [START],
			 op, 
			 package,
			 &result);
	return result;
}

gboolean 
eazel_package_system_emit_progress (EazelPackageSystem *system, 
				    EazelPackageSystemOperation op, 
				    const PackageData *package,
				    unsigned long info[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS])
{
	gboolean result = TRUE;
	int infos;
	unsigned long *infoblock;

	EPS_API (system);
	infoblock = g_new0 (unsigned long, EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS+1);
	for (infos = 0; infos < EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS; infos++) {
		infoblock[infos] = info[infos];
	}

	gtk_signal_emit (GTK_OBJECT (system),
			 signals [PROGRESS],
			 op, 
			 package,
			 infoblock,
			 &result);

	g_free (infoblock);
	return result;
}

gboolean 
eazel_package_system_emit_failed (EazelPackageSystem *system, 
				  EazelPackageSystemOperation op, 
				  const PackageData *package)
{
	gboolean result = TRUE;
	EPS_API (system);

	gtk_signal_emit (GTK_OBJECT (system),
			 signals [FAILED],
			 op, 
			 package,
			 &result);

	return result;
}

gboolean 
eazel_package_system_emit_end (EazelPackageSystem *system, 
			       EazelPackageSystemOperation op, 
			       const PackageData *package)
{
	gboolean result = TRUE;
	EPS_API (system);

	gtk_signal_emit (GTK_OBJECT (system),
			 signals [END],
			 op, 
			 package,
			 &result);

	return result;
}

EazelPackageSystemDebug 
eazel_package_system_get_debug (EazelPackageSystem *system)
{
	if (system->private) {
		return system->private->debug;
	} else {
		return 0;
	}
}

void                    
eazel_package_system_set_debug (EazelPackageSystem *system, 
				EazelPackageSystemDebug d)
{
	EPS_API (system);
	system->private->debug = d;
}

/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_finalize (GtkObject *object)
{
	EazelPackageSystem *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM (object));

	system = EAZEL_PACKAGE_SYSTEM (object);

	if (GTK_OBJECT_CLASS (eazel_package_system_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_parent_class)->finalize (object);
	}
}

static void
eazel_package_system_class_initialize (EazelPackageSystemClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_finalize;
	
	eazel_package_system_parent_class = gtk_type_class (gtk_object_get_type ());

	signals[START] = 
		gtk_signal_new ("start",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, start),
				eazel_package_system_marshal_BOOL__ENUM_POINTER,
				GTK_TYPE_BOOL, 2, 
				GTK_TYPE_ENUM, GTK_TYPE_POINTER);	
	signals[END] = 
		gtk_signal_new ("end",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, end),
				eazel_package_system_marshal_BOOL__ENUM_POINTER,
				GTK_TYPE_BOOL, 2, 
				GTK_TYPE_ENUM, GTK_TYPE_POINTER);	
	signals[PROGRESS] = 
		gtk_signal_new ("progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, progress),
				eazel_package_system_marshal_BOOL__ENUM_POINTER_POINTER,
				GTK_TYPE_BOOL, 3, 
				GTK_TYPE_ENUM, GTK_TYPE_POINTER, GTK_TYPE_POINTER);
	signals[FAILED] = 
		gtk_signal_new ("failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, failed),
				eazel_package_system_marshal_BOOL__ENUM_POINTER,
				GTK_TYPE_BOOL, 2, 
				GTK_TYPE_ENUM, GTK_TYPE_POINTER);	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	klass->start = NULL;
	klass->progress = NULL;
	klass->failed = NULL;
	klass->end = NULL;
}

static void
eazel_package_system_initialize (EazelPackageSystem *system) {
	g_assert (system!=NULL); 
	g_assert (EAZEL_IS_PACKAGE_SYSTEM (system));
	
	system->private = g_new0 (EazelPackageSystemPrivate, 1);
	system->err = NULL;
}

GtkType
eazel_package_system_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystem",
			sizeof (EazelPackageSystem),
			sizeof (EazelPackageSystemClass),
			(GtkClassInitFunc) eazel_package_system_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (gtk_object_get_type (), &system_info);
	}

	return system_type;
}

/* 
   This is the real constructor 
*/
EazelPackageSystem *
eazel_package_system_new_real ()
{
	EazelPackageSystem *system;

	system = EAZEL_PACKAGE_SYSTEM (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM, NULL));

	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));

	return system;
}

/* 
   This lets the user create a packagesystem with a specific 
   id 
*/
EazelPackageSystem *
eazel_package_system_new_with_id (EazelPackageSystemId id, GList *roots)
{
	EazelPackageSystem *result;
	
	result = eazel_package_system_load_implementation (id, roots);

	/* If we failed (eg. unsupported system id), return
	   an empty object */
	if (result == NULL) {
		result = eazel_package_system_new_real ();
	}

	return result;
}

/*
  Autodetect distribution and creates
  an instance of a EazelPackageSystem with the appropriate
  type
 */
EazelPackageSystem *
eazel_package_system_new (GList *roots) 
{
	return eazel_package_system_new_with_id (eazel_package_system_suggest_id (), roots);
}

/* Marshal functions */

typedef gboolean (*GtkSignal_BOOL__ENUM_POINTER_POINTER) (GtkObject *object,
							  gint arg1,
							  gpointer arg2,
							  gpointer arg3,
							  gpointer user_data);

void eazel_package_system_marshal_BOOL__ENUM_POINTER_POINTER (GtkObject *object,
							      GtkSignalFunc func,
							      gpointer func_data, 
							      GtkArg *args)
{
	GtkSignal_BOOL__ENUM_POINTER_POINTER rfunc;
	gboolean *result;

	result = GTK_RETLOC_BOOL (args[3]);
	rfunc = (GtkSignal_BOOL__ENUM_POINTER_POINTER)func;
	(*result) = (*rfunc) (object,
			      GTK_VALUE_ENUM (args[0]),
			      GTK_VALUE_POINTER (args[1]),
			      GTK_VALUE_POINTER (args[2]),
			      func_data);
}

typedef gboolean (*GtkSignal_BOOL__ENUM_POINTER) (GtkObject *object,
						  gint arg1,
						  gpointer arg2,
						  gpointer user_data);

void eazel_package_system_marshal_BOOL__ENUM_POINTER (GtkObject *object,
						      GtkSignalFunc func,
						      gpointer func_data, 
						      GtkArg *args)
{
	GtkSignal_BOOL__ENUM_POINTER rfunc;
	gboolean *result;
	
	rfunc = (GtkSignal_BOOL__ENUM_POINTER)func;
	result = GTK_RETLOC_BOOL (args[2]);
	(*result) = (*rfunc) (object,
			      GTK_VALUE_ENUM (args[0]),
			      GTK_VALUE_POINTER (args[1]),
			      func_data);
}
