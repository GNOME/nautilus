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

/* 
   IMPLEMENTATION NOTE:

   Originally, the rpm3 package system module would open the dbs
   in _new, and close the in _finalize. Addtionally, it would
   close before any rpm spawning and reopen afterwards. This
   close/reopen was needed since rpm needs exclusive lock on
   the db files.

   However, since I now also use the packagesystem object in
   rpmview/packageview, that meant I would keep the db system
   open when installing, thus the install would fail, since
   the view had a open fd on the db's.

   So now, before any operation, I open the db's and close
   afterwards. This sucks pretty much, since eg. during file conflicts
   checking in libeazelinstall, I execute potientially several hundred
   queries in a row - and each query opens/closes the db's. Blech.

*/

#include <config.h>

#ifdef HAVE_RPM

#include <gnome.h>
#include "eazel-package-system-rpm3-private.h"
#include "eazel-package-system-private.h"
#include <libtrilobite/trilobite-core-utils.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/misc.h>

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libtrilobite/trilobite-root-helper.h>

#define DEFAULT_DB_PATH "/var/lib/rpm"
#define DEFAULT_ROOT "/"

#define USE_PERCENT

EazelPackageSystem* eazel_package_system_implementation (GList*);

/* This is the parent class pointer */
static EazelPackageSystemClass *eazel_package_system_rpm3_parent_class;

/************************************************************
*************************************************************/

#define PERCENTS_PER_RPM_HASH 2

struct RpmMonitorPiggyBag {
	EazelPackageSystemRpm3 *system;
	EazelPackageSystemOperation op;

	unsigned long packages_installed, total_packages;
	unsigned long bytes_installed, total_bytes;

	GList *packages_to_expect;
	GList *packages_seen;

#ifdef USE_PERCENT
	char separator;
	char line[80];
	/* state 1 waiting for package name
                 2 waiting for %%
                 3 reading percentages
	*/
	int state;
	int bytes_read_in_line;
	char *package_name;
#else
	GString *package_name;
#endif
	PackageData *pack;
	int pct;

	GHashTable *name_to_package;

	volatile gboolean subcommand_running;
};

static struct RpmMonitorPiggyBag 
rpmmonitorpiggybag_new (EazelPackageSystemRpm3 *system, 
			EazelPackageSystemOperation op) 
{
	struct RpmMonitorPiggyBag pig;
#ifdef USE_PERCENT
	struct lconv *lc;
#endif

#ifdef USE_PERCENT
	lc = localeconv ();
	pig.separator = *(lc->decimal_point);
	pig.state = 1;
	pig.bytes_read_in_line = 0;
	pig.package_name = NULL;
#else
	pig.pack = NULL;
	pig.package_name = NULL;
#endif
	pig.pct = 0;
	pig.system = system;
	pig.op = op;

	pig.packages_seen = NULL;
	pig.packages_installed = 0;	
	pig.bytes_installed = 0;	

	return pig;
}

/* Code to get and set a string field from
   a Header */
void
eazel_package_system_rpm3_get_and_set_string_tag (Header hd,
						  int tag, 
						  char **str)
{
	char *tmp;

	g_assert (str);

	headerGetEntry (hd,
			tag, NULL,
			(void **) &tmp, NULL);
	g_free (*str);
	(*str) = g_strdup (tmp);
}

/* Creates argument list for rpm */
static void
make_rpm_argument_list (EazelPackageSystemRpm3 *system,
			EazelPackageSystemOperation op,
			unsigned long flags,
			const char *dbpath,
			GList *packages,
			GList **args)
{
	GList *iterator;

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		if (op == EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL) {
			(*args) = g_list_prepend (*args, g_strdup (pack->filename));
		} else if (op == EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL) {
			(*args) = g_list_prepend (*args, packagedata_get_name (pack));
		} else {
			g_assert (0);
		}
	}

	if (dbpath) {
		if (op == EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL &&
		    !(flags & EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE) &&
		    !(flags & EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE)) {
			if (strcmp (dbpath, DEFAULT_DB_PATH)) {
				char *root = g_hash_table_lookup (system->private->db_to_root, dbpath);
				(*args) = g_list_prepend (*args, g_strdup (root));
				(*args) = g_list_prepend (*args, g_strdup ("--prefix"));
			}
		}
		(*args) = g_list_prepend (*args, g_strdup (dbpath));
		(*args) = g_list_prepend (*args, g_strdup ("--dbpath"));		
	}

	if (flags & EAZEL_PACKAGE_SYSTEM_OPERATION_TEST) {
		(*args) = g_list_prepend (*args, g_strdup ("--test"));
	} 

	if (flags & EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE) {
		(*args) = g_list_prepend (*args, g_strdup ("--nodeps"));
		if (op == EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL) {
			(*args) = g_list_prepend (*args, g_strdup ("--force"));
		}
	}

	if (op == EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL) {
		(*args) = g_list_prepend (*args, g_strdup ("-e"));
	} else  {
		if (flags & EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE) {
			(*args) = g_list_prepend (*args, g_strdup ("--oldpackage"));
		}
		if (flags & EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE) {
#ifdef USE_PERCENT
			(*args) = g_list_prepend (*args, g_strdup ("--percent"));
			(*args) = g_list_prepend (*args, g_strdup ("-Uv"));
#else
			(*args) = g_list_prepend (*args, g_strdup ("-Uvh"));
#endif
		} else {
#ifdef USE_PERCENT
			(*args) = g_list_prepend (*args, g_strdup ("--percent"));
			(*args) = g_list_prepend (*args, g_strdup ("-iv"));
#else
			(*args) = g_list_prepend (*args, g_strdup ("-ivh"));
#endif
		}
	}
}

static void
destroy_string_list (GList *list)
{
	g_list_foreach (list, (GFunc)g_free, NULL);
	g_list_free (list);
}

static GHashTable*
rpm_make_names_to_package_hash (GList *packages)
{
	GList *iterator;
	GHashTable *result;

	result = g_hash_table_new (g_str_hash, g_str_equal);

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		char *tmp;		
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		tmp = g_strdup_printf ("%s", pack->name);
		g_hash_table_insert (result,
				     tmp,
				     iterator->data);
	}
	return result;
}

/* A GHRFunc to clean
   out the name_to_package hash table 
*/
static gboolean
clear_name_to_package (char *key,
		       PackageData *pack,
		       gpointer unused)
{
	g_free (key);
	return TRUE;
}

static unsigned long
get_total_size_of_packages (const GList *packages)
{
	const GList *iterator;
	unsigned long result = 0;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		result += pack->bytesize;
	}
	return result;
}

#ifdef USE_PERCENT
/* This monitors an rpm process pipe and emits
   signals during execution */
/* Ahhh, the joy of not having C++ and not just be able
   to define a function class, but instead I get to carry
   the pig around.... */
static gboolean
monitor_rpm_process_pipe_percent_output (GIOChannel *source,
					 GIOCondition condition,
					 struct RpmMonitorPiggyBag *pig)
{
	char tmp;
	ssize_t bytes_read;
	gboolean result = TRUE;

	bytes_read = 0;
	g_io_channel_read (source, &tmp, 1, &bytes_read);
	
	if (bytes_read) {
		if (isspace (tmp)) {
			switch (pig->state) {
			case 1:
				if (pig->package_name) {
					g_free (pig->package_name);
				}
				/* Reset */
				pig->pack = NULL;
				pig->package_name = NULL;
				pig->pct = 0;
				
				pig->package_name = g_strdup (pig->line);
				pig->pack = g_hash_table_lookup (pig->name_to_package, pig->package_name);

				if (pig->pack==NULL) {
					char *dash;
					dash = strrchr (pig->package_name, '-');
					while (dash && pig->pack==NULL) {
						*dash = '\0';
						pig->pack = g_hash_table_lookup (pig->name_to_package, 
										 pig->package_name);
						dash = strrchr (pig->package_name, '-');
					}
				}

				if (pig->pack==NULL) {
					if (pig->package_name && 
					    ((strcmp (pig->package_name, "warning:") == 0) ||
					     (strcmp (pig->package_name, "error:") == 0) ||
					     (strcmp (pig->package_name, "cannot") == 0))) {
						fail (pig->system, "rpm says \"%s\"", pig->package_name);
					} else if (pig->package_name) {
						verbose (pig->system, "lookup \"%s\" failed", 
							 pig->package_name);
					}
				} else {
					unsigned long longs[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS];

					info (pig->system, "matched \"%s\"", pig->package_name);

					pig->packages_installed ++;
					
					longs[0] = 0;
					longs[1] = pig->pack->bytesize;
					longs[2] = pig->packages_installed;
					longs[3] = pig->total_packages;
					longs[4] = pig->bytes_installed;
					longs[5] = pig->total_bytes;
					
					eazel_package_system_emit_start (EAZEL_PACKAGE_SYSTEM (pig->system),
									 pig->op,
									 pig->pack);
					eazel_package_system_emit_progress (EAZEL_PACKAGE_SYSTEM (pig->system),
									    pig->op,
									    pig->pack,
									    longs);
					/* switch state */
					pig->state = 2;
				}


				break;

			case 2:
				if (strncmp (pig->line, "%%", 2) == 0) {
					pig->state = 3;
				} 
				break;
			case 3: {
				char *dot;
				int pct;

				/* Assume we don't go to state 1 */
				pig->state = 2;

				/* Remove the decimal crap */
				dot = strchr (pig->line, pig->separator);
				if (dot) {
					*dot = 0;
				}
				/* Grab the percentage */
				pct = atol (pig->line);
				/* Higher ? */
				if (pct > pig->pct) {
					unsigned long longs[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS];
					int amount;

					pig->pct = pct;
					if (pig->pct == 100) {
						amount = pig->pack->bytesize;
					} else {
						amount = (pig->pack->bytesize/100) * pig->pct;
					}

					longs[0] = amount;
					longs[1] = pig->pack->bytesize;
					longs[2] = pig->packages_installed;
					longs[3] = pig->total_packages;
					longs[4] = pig->bytes_installed + amount;
					longs[5] = pig->total_bytes;
					
					eazel_package_system_emit_progress (EAZEL_PACKAGE_SYSTEM (pig->system),
									    pig->op,
									    pig->pack, 
									    longs);
					/* Done with package ? */
					if (pig->pct==100) {
						pig->state = 1;
						pig->bytes_installed += pig->pack->bytesize;
						pig->packages_seen = g_list_prepend (pig->packages_seen,
										     pig->pack);
						eazel_package_system_emit_end (EAZEL_PACKAGE_SYSTEM (pig->system),
									       pig->op,
									       pig->pack);
						
						pig->pack = NULL;
						pig->pct = 0;
						g_free (pig->package_name);
						pig->package_name = NULL;
					}
					
				}
			}
			break;
			default:
				g_assert_not_reached ();
			}
			pig->bytes_read_in_line = 0;
		} else {
			if (pig->bytes_read_in_line > 79) {
				trilobite_debug ("Ugh, read too much");
			} else {
				pig->line[pig->bytes_read_in_line] = tmp;
				pig->bytes_read_in_line++;
				pig->line[pig->bytes_read_in_line] = '\0';
			}
		}
	} else {
		result = FALSE;
	}
	
	pig->subcommand_running = result;
	return result;
}
#endif

#ifndef USE_PERCENT
/* This monitors an rpm process pipe and emits
   signals during execution */
static gboolean
monitor_rpm_process_pipe (GIOChannel *source,
			  GIOCondition condition,
			  struct RpmMonitorPiggyBag *pig)
{
	char         tmp;
	ssize_t      bytes_read;
	gboolean result = TRUE;

	g_io_channel_read (source, &tmp, 1, &bytes_read);
	
	if (bytes_read) {
		/* Percentage output, parse and emit... */
		if (tmp=='#') {
			int amount = 0;
			if (pig->pack == NULL) {
				return TRUE;
			}
			pig->pct += PERCENTS_PER_RPM_HASH;
			if (pig->pct == 100) {
				amount = pig->pack->bytesize;
			} else {
				amount =  (pig->pack->bytesize / 100) * pig->pct;
			}
			if (pig->pack && amount) {
				unsigned long longs[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS];
				
				longs[0] = amount;
				longs[1] = pig->pack->bytesize;
				longs[2] = pig->packages_installed;
				longs[3] = pig->total_packages;
				longs[4] = pig->bytes_installed + amount;
				longs[5] = pig->total_bytes;

				eazel_package_system_emit_progress (EAZEL_PACKAGE_SYSTEM (pig->system),
								    pig->op,
								    pig->pack, 
								    longs);
			}
			/* By invalidating the pointer here, we
			   only emit with amount==total once and
			   also emit end here */
			if (pig->pct==100) {
				pig->bytes_installed += pig->pack->bytesize;
				pig->packages_seen = g_list_prepend (pig->packages_seen,
								     pig->pack);
				info (pig->system, "seen.size = %d", g_list_length (pig->packages_seen));
				eazel_package_system_emit_end (EAZEL_PACKAGE_SYSTEM (pig->system),
							       pig->op,
							       pig->pack);
				
				pig->pack = NULL;
				pig->pct = 0;
				g_string_free (pig->package_name, TRUE);
				pig->package_name = NULL;
			}
		}  else  if (!isspace (tmp)) {		       
			/* Paranoia check */
			if (pig->package_name) {
				g_string_free (pig->package_name, TRUE);
			}

			/* Reset */
			pig->package_name = g_string_new (NULL);
			pig->pack = NULL;
			
                        /* Read untill we hit a space */
			while (bytes_read && !isspace (tmp)) {
				g_string_append_c (pig->package_name, tmp);
				g_io_channel_read (source, &tmp, 1, &bytes_read);
			}
			
			/* It's not a #, and we've read a full word */

			/* first check is this an expected file name ? */
			if (pig->package_name &&
			    pig->package_name->str &&
			    g_list_find_custom (pig->packages_to_expect,
						pig->package_name->str,
						(GCompareFunc)eazel_install_package_name_compare)) {
				if (pig->package_name) {
					pig->pack = g_hash_table_lookup (pig->name_to_package, pig->package_name->str);
				} 
			} else {
				fail (pig->system, "\"%s\" wasn't expected", pig->package_name->str);
			}
					

			if (pig->pack==NULL) {
				if (pig->package_name && 
				    pig->package_name->str &&
				    ((strcmp (pig->package_name->str, "warning:") == 0) ||
				     (strcmp (pig->package_name->str, "error:") == 0) ||
				     (strcmp (pig->package_name->str, "cannot") == 0))) {
					while (tmp != '\n') {
						g_string_append_c (pig->package_name, tmp);
						g_io_channel_read (source, &tmp, 1, &bytes_read);
					}
					fail (pig->system, "rpm says \"%s\"", pig->package_name->str);
				} else if (pig->package_name) {
					verbose (pig->system, "lookup \"%s\" failed", pig->package_name->str);
				}
			} else {
				unsigned long longs[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS];
				info (pig->system, "matched \"%s\"", pig->package_name->str);
				pig->pct = 0;
				pig->packages_installed ++;
				
				longs[0] = 0;
				longs[1] = pig->pack->bytesize;
				longs[2] = pig->packages_installed;
				longs[3] = pig->total_packages;
				longs[4] = pig->bytes_installed;
				longs[5] = pig->total_bytes;

				eazel_package_system_emit_start (EAZEL_PACKAGE_SYSTEM (pig->system),
								 pig->op,
								 pig->pack);
				eazel_package_system_emit_progress (EAZEL_PACKAGE_SYSTEM (pig->system),
								    pig->op,
								    pig->pack,
								    longs);

			}
		}
	} 

	if (bytes_read == 0) {
		result = FALSE;
	} else {
		result = TRUE;
	}
	
	pig->subcommand_running = result;

	return result;
}
#endif

static void
rpm_create_db (char *dbpath,
	       char *root,
	       EazelPackageSystemRpm3 *system)
{
	addMacro (NULL, "_dbpath", NULL, "/", 0);

	if (strcmp (root, "/")) {
		info (system, "Creating %s", dbpath);
		mkdir (dbpath, 0700);
		rpmdbInit (dbpath, 0644);
	}
}

void
eazel_package_system_rpm3_create_dbs (EazelPackageSystemRpm3 *system,
				      GList *dbpaths)
{	
	GList *iterator;

	g_assert (system);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_RPM3 (system));
	g_assert (system->private->dbs);

	system->private->dbpaths = dbpaths;
	for (iterator = dbpaths; iterator; iterator = g_list_next (iterator)) {
		char *db = (char*)iterator->data;
		char *root = (char*)(iterator = g_list_next (iterator))->data;

		info (system, "Adding %s as root for %s", root, db);
		g_hash_table_insert (system->private->db_to_root, db, root);
	}

	g_hash_table_foreach (system->private->db_to_root, (GHFunc)rpm_create_db, system);

	info (system, "Read rpmrc file");
	rpmReadConfigFiles ("/usr/lib/rpm/rpmrc", NULL);	
}

static void
rpm_open_db (char *dbpath,
	     char *root,
	     EazelPackageSystemRpm3 *system)
{
	rpmdb db;

	addMacro(NULL, "_dbpath", NULL, "/", 0);
	if (rpmdbOpen (dbpath, &db, O_RDONLY, 0644)) {
		fail (system, "Opening packages database in %s failed (a)", dbpath);
	} else {			
		if (db) {
			info (system, _("Opened packages database in %s"), dbpath);
			g_hash_table_insert (system->private->dbs,
					     g_strdup (dbpath),
					     db);
		} else {
			fail (system, _("Opening packages database in %s failed"), dbpath);
		}
	}
}

void
eazel_package_system_rpm3_open_dbs (EazelPackageSystemRpm3 *system)
{
	g_assert (system);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_RPM3 (system));
	g_assert (system->private->dbs);

	g_hash_table_foreach (system->private->db_to_root, 
			      (GHFunc)rpm_open_db,
			      system);
}

static gboolean
rpm_close_db (char *key, 
	       rpmdb db, 
	       EazelPackageSystemRpm3 *system)
{
	if (db) {
		info (system, _("Closing db for %s (open)"), key);
		rpmdbClose (db);
		db = NULL;
		g_free (key);
	} else {
		fail (system, _("Closing db for %s (not open)"), key);
	}

	return TRUE;
}

gboolean
eazel_package_system_rpm3_close_dbs (EazelPackageSystemRpm3 *system)
{
	/* Close all the db's */
	g_assert (system->private->dbs);
	g_hash_table_foreach_remove (system->private->dbs, 
				     (GHRFunc)rpm_close_db,
				     system);	
	return TRUE;
}

static gboolean
rpm_free_db (char *key, 
	     char *root, 
	     EazelPackageSystemRpm3 *system)
{
	g_free (key);
	g_free (root);
	return TRUE;
}

gboolean
eazel_package_system_rpm3_free_dbs (EazelPackageSystemRpm3 *system)
{
	/* Close all the db's */
	g_assert (system->private->dbs);
	g_hash_table_foreach_remove (system->private->db_to_root, 
				     (GHRFunc)rpm_free_db,
				     system);	
	return TRUE;
}

/************************************************************
 Load Package implemementation
*************************************************************/

static EazelSoftCatSense
rpm_sense_to_softcat_sense (EazelPackageSystemRpm3 *system,
			    int rpm_sense) 
{
	EazelSoftCatSense result = 0;

	if (rpm_sense & RPMSENSE_ANY) {
		result |= EAZEL_SOFTCAT_SENSE_ANY;
	} else {
		if (rpm_sense & RPMSENSE_EQUAL) {
			result |= EAZEL_SOFTCAT_SENSE_EQ;
		}
		if (rpm_sense & RPMSENSE_GREATER) {
			result |= EAZEL_SOFTCAT_SENSE_GT;
		}
		if (rpm_sense & RPMSENSE_LESS) {
			result |= EAZEL_SOFTCAT_SENSE_LT;
		}
	}

	return result;
}

void 
eazel_package_system_rpm3_packagedata_fill_from_header (EazelPackageSystemRpm3 *system,
							PackageData *pack, 
							Header hd,
							int detail_level)
{
	unsigned long *sizep;

	eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_NAME, &pack->name);
	eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_VERSION, &pack->version);
	eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_RELEASE, &pack->minor);
	eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_ARCH, &pack->archtype);
	if (~detail_level & PACKAGE_FILL_NO_TEXT) {
		eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_DESCRIPTION, &pack->description);
		eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_SUMMARY, &pack->summary);
	}

	headerGetEntry (hd,
			RPMTAG_SIZE, NULL,
			(void **) &sizep, NULL);	
	pack->bytesize = *sizep;

	pack->packsys_struc = (gpointer)hd;
	
	pack->fillflag = detail_level;

	/* FIXME: bugzilla.eazel.com 4863 */
	if (~detail_level & PACKAGE_FILL_NO_PROVIDES) {
		char **paths = NULL;
		char **paths_copy = NULL;
		char **names = NULL;
		int *indexes = NULL;
		int count = 0;
		int index = 0;
		int num_paths = 0;
		uint_16 *file_modes;

		g_list_foreach (pack->provides, (GFunc)g_free, NULL);
		g_list_free (pack->provides);
		pack->provides = NULL;

                /* RPM v.3.0.4 and above has RPMTAG_BASENAMES, this will not work
		   with any version below 3.0.4 */

		headerGetEntry (hd,			
				RPMTAG_DIRINDEXES, NULL,
				(void**)&indexes, NULL);
		headerGetEntry (hd,			
				RPMTAG_DIRNAMES, NULL,
				(void**)&paths, &num_paths);
		headerGetEntry (hd,			
				RPMTAG_BASENAMES, NULL,
				(void**)&names, &count);
		headerGetEntry (hd,			
				RPMTAG_FILEMODES, NULL,
				(void**)&file_modes, NULL);

		/* Copy all paths and shave off last /.
		   This is needed to remove the dir entries from 
		   the packagedata's provides list. */
		paths_copy = g_new0 (char*, num_paths);
		for (index=0; index<num_paths; index++) {
			paths_copy[index] = g_strdup (paths[index]);
			paths_copy[index][strlen (paths_copy[index]) - 1] = 0;
		}

		/* Now loop through all the basenames */
		for (index=0; index<count; index++) {
			char *fullname = NULL;
			if (paths) {
				fullname = g_strdup_printf ("%s/%s", paths_copy[indexes[index]], names[index]);
			} else {
				fullname = g_strdup (names[index]);
			}
			if (detail_level & PACKAGE_FILL_NO_DIRS_IN_PROVIDES) {
				if (file_modes[index] & 040000) {
					g_free (fullname);
					fullname = NULL;
				}
#if 0
				fprintf (stderr, "file_modes[%s] = 0%o %s\n", 
					 fullname, file_modes[index],
					 (file_modes[index] & 040000) ? "DIR" : "file" );
#endif
			}
			if (fullname) {
				/* trilobite_debug ("%s provides %s", pack->name, fullname);*/
				pack->provides = g_list_prepend (pack->provides, fullname);
			}
		}
		pack->provides = g_list_reverse (pack->provides);
		for (index=0; index<num_paths; index++) {
			g_free (paths_copy[index]);
		}
		g_free (paths_copy);
		free ((void*)paths);
		free ((void*)names);
	}


	if (~detail_level & PACKAGE_FILL_NO_DEPENDENCIES) {		
		const char **requires_name, **requires_version;
		int *requires_flag;
		int count;
		int index;
		
		headerGetEntry (hd,
				RPMTAG_REQUIRENAME, NULL,
				(void**)&requires_name,
				&count);
		headerGetEntry (hd,
				RPMTAG_REQUIREVERSION, NULL,
				(void**)&requires_version,
				NULL);
		headerGetEntry (hd,
				RPMTAG_REQUIREFLAGS, NULL,
				(void**)&requires_flag,
				NULL);

		for (index = 0; index < count; index++) {
			PackageData *package = packagedata_new ();
			PackageDependency *pack_dep = packagedependency_new ();

			/* If it's a lib*.so* or a /yadayada, add to provides */
			if ((strncmp (requires_name[index], "lib", 3)==0 && 
			     strstr (requires_name[index], ".so")) ||
			    (strncmp (requires_name[index], "ld-linux.so",11)==0) ||
			    (*requires_name[index]=='/')) {
				/* Unless it has a ( in the name */
				if (strchr (requires_name[index], '(')==NULL) {
					package->features = g_list_prepend (package->features, 
									    g_strdup (requires_name[index]));
				}
			} else {
				/* Otherwise, add as a package name */
				package->name = g_strdup (requires_name[index]);
				/* and set the version if not empty */
				pack_dep->version = *requires_version[index]=='\0' ? 
					NULL : g_strdup (requires_version[index]);
			}
			/* If anything set, add dep */
			if (package->name || package->provides) {
				pack_dep->sense = rpm_sense_to_softcat_sense (system,
									      requires_flag[index]);
				package->archtype = trilobite_get_distribution_arch ();
				pack_dep->package = package;
				pack->depends = g_list_prepend (pack->depends, pack_dep);
			} else {
				packagedependency_destroy (pack_dep);
				gtk_object_unref (GTK_OBJECT (package));
			}
		}
		free ((void*)requires_name);
		free ((void*)requires_version);

	}
	/* FIXME: bugzilla.eazel.com 5826
	   Load in the features of the package */	
}

static gboolean 
rpm_packagedata_fill_from_file (EazelPackageSystemRpm3 *system,
				PackageData *pack, 
				const char *filename, 
				int detail_level)
{
	static FD_t fd;
	Header hd;

	/* Set filename field */
	if (pack->filename != filename) {
		g_free (pack->filename);
		pack->filename = g_strdup (filename);
	}

	/* FIXME: Would be better to call a package_data_ function to do this. */
	if (pack->packsys_struc) {
		headerFree ((Header) pack->packsys_struc);
		pack->packsys_struc = NULL;
	}

	/* Open rpm */
	fd = fdOpen (filename, O_RDONLY, 0);

	if (fd == NULL) {
		g_warning (_("Cannot open %s"), filename);
		pack->status = PACKAGE_CANNOT_OPEN;
		return FALSE;
	}

	/* Get Header block */
	rpmReadPackageHeader (fd, &hd, &pack->source_package, NULL, NULL);
	eazel_package_system_rpm3_packagedata_fill_from_header (system, pack, hd, detail_level);
	pack->status = PACKAGE_UNKNOWN_STATUS;

	fdClose (fd);
	return TRUE;
}

static PackageData* 
rpm_packagedata_new_from_file (EazelPackageSystemRpm3 *system,
			       const char *file, 
			       int detail_level)
{
	PackageData *pack;

	pack = packagedata_new ();
	if (rpm_packagedata_fill_from_file (system, pack, file, detail_level)==FALSE) {
		gtk_object_unref (GTK_OBJECT (pack));
		pack = NULL;
	}
	return pack;
}

PackageData*
eazel_package_system_rpm3_load_package (EazelPackageSystemRpm3 *system,
					PackageData *in_package,
					const char *filename,
					int detail_level)
{
	PackageData *result = NULL;

	if (in_package) {
		result = in_package;
		if (rpm_packagedata_fill_from_file (system, result, filename, detail_level)==FALSE) {
			result = NULL;
		}
	} else {
		result = rpm_packagedata_new_from_file (system, filename, detail_level);
	}

	return result;
}

/************************************************************
 Query implemementation
*************************************************************/

rpmdb
eazel_package_system_rpm3_get_db (EazelPackageSystemRpm3 *system,
				  const char *dbpath)
{
	rpmdb db;

	db = g_hash_table_lookup (system->private->dbs, dbpath);
	if (!db) {
		fail (system, "query could not access db in %s", dbpath);
		return NULL;
	}
	return db;
}

#ifdef HAVE_RPM_30
static void
eazel_package_system_rpm3_query_impl (EazelPackageSystemRpm3 *system,
				      const char *dbpath,
				      const char* key,
				      EazelPackageSystemQueryEnum flag,
				      int detail_level,
				      GList **result)
{
	rpmdb db = eazel_package_system_rpm3_get_db (system, dbpath);
	int rc = 1;
	dbiIndexSet matches;

	if (!db) {
		return;
	}

	switch (flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
		info (system, "query (in %s) OWNS %s", dbpath, key);
		rc = rpmdbFindByFile (db, key, &matches);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
		info (system, "query (in %s) PROVIDES %s", dbpath, key);
		rc = rpmdbFindByProvides (db, key, &matches);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		info (system, "query (in %s) MATCHES %s", dbpath, key);
		rc = rpmdbFindPackage (db, key, &matches);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES:
		info (system, "query (in %s) REQUIRES %s", dbpath, key);
		rc = rpmdbFindByRequiredBy (db, key, &matches);
		break;
	default:
		g_warning ("Unknown query flag %d", flag);
		g_assert_not_reached ();
	}
	       
	if (rc == 0) {
		unsigned int i;		

		info (system, "%d hits", dbiIndexSetCount (matches));
		for (i = 0; i < dbiIndexSetCount (matches); i++) {
			unsigned int offset;
			Header hd;
			PackageData *pack;
			
			offset = dbiIndexRecordOffset (matches, i);
			hd = rpmdbGetRecord (db, offset);
			pack = packagedata_new ();
			eazel_package_system_rpm3_packagedata_fill_from_header (system, 
										pack, 
										hd, 
										detail_level);
			g_free (pack->install_root);
			pack->install_root = g_strdup (dbpath);
			if (g_list_find_custom (*result, 
						pack, 
						(GCompareFunc)eazel_install_package_compare)!=NULL) {
				info (system, "%s already in set", pack->name);
				packagedata_destroy (pack, TRUE);
			} else {
				(*result) = g_list_prepend (*result, pack);
			}
		}
		dbiFreeIndexRecord (matches);
	} else {
		info (system, "0 hits");
	}
}

static void
eazel_package_system_rpm3_query_substr (EazelPackageSystemRpm3 *system,
					const char *dbpath,
					const char *key,
					int detail_level,
					GList **result)
{
	int offset;
	rpmdb db = eazel_package_system_rpm3_get_db (system, dbpath);
 
	if (!db) {
		return;
	}

	for (offset = rpmdbFirstRecNum (db); offset; offset = rpmdbNextRecNum (db, offset)) {
		Header hd;
		char *name = NULL;
		
		hd = rpmdbGetRecord (db, offset);

		eazel_package_system_rpm3_get_and_set_string_tag (hd, RPMTAG_NAME, &name);

		/* If key occurs in name, create package and add to result */
		if (strstr (name, key)) {
			PackageData *pack = packagedata_new ();			
			eazel_package_system_rpm3_packagedata_fill_from_header (system, 
										pack, 
										hd, 
										detail_level);
			(*result) = g_list_prepend (*result, pack);
		} else {
			headerFree (hd);
		}
		g_free (name);
	}
	
}

static void
eazel_package_system_rpm3_query_foreach (char *dbpath,
					 rpmdb db,
					 struct RpmQueryPiggyBag *pig)
{
	info (pig->system, "eazel_package_system_rpm3_query_foreach");
	switch (pig->flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		eazel_package_system_rpm3_query_impl (pig->system,
						      dbpath,
						      pig->key,
						      pig->flag,
						      pig->detail_level,
						      pig->result);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES:
		eazel_package_system_rpm3_query_requires (pig->system,
							  dbpath,
							  pig->key,
							  pig->detail_level,
							  pig->result);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR:
		eazel_package_system_rpm3_query_substr (pig->system,
							dbpath,
							pig->key,
							pig->detail_level,
							pig->result);
		break;
	default:
		g_warning ("Unknown query flag %d", pig->flag);
		g_assert_not_reached ();
	}
}
#endif /* HAVE_RPM_30 */

void
eazel_package_system_rpm3_query_requires (EazelPackageSystemRpm3 *system,
					  const char *dbpath,
					  const gpointer *key,
					  int detail_level,
					  GList **result)
{
	const PackageData *pack = (PackageData*)key;

	if (pack->name) {
		(EAZEL_PACKAGE_SYSTEM_RPM3_CLASS (GTK_OBJECT (system)->klass)->query_impl) (EAZEL_PACKAGE_SYSTEM (system),
											    dbpath,
											    pack->name,
											    EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
											    detail_level,
											    result);
	}
	if (pack->provides) {
		GList *iterator;
		/* FIXME: ideally, this could use package->features instead, that would
		   be safer then doing the strstr check. But for now, I just check if
		   fkey is "lib.*\.so.*", or "/bin/.*" or "/sbin/.*" */
		for (iterator = pack->provides; iterator; iterator = g_list_next (iterator)) {
			const char *fkey = (const char*)iterator->data;
			if ((strncmp (g_basename (fkey), "lib", 3)==0 && strstr (fkey, ".so")) ||
			    strncmp (fkey, "/bin/", 5)==0 ||
			    strncmp (fkey, "/sbin/", 6)==0) {
				(EAZEL_PACKAGE_SYSTEM_RPM3_CLASS (GTK_OBJECT (system)->klass)->query_impl) (EAZEL_PACKAGE_SYSTEM (system),
													    dbpath,
													    g_basename (fkey),
													    EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
													    detail_level,
													    result);
			}
		}
		info (system, "result set size is now %d", g_list_length (*result));
	}
}

GList*               
eazel_package_system_rpm3_query (EazelPackageSystemRpm3 *system,
				 const char *dbpath,
				 const gpointer key,
				 EazelPackageSystemQueryEnum flag,
				 int detail_level)
{
	GList *result = NULL;
	struct RpmQueryPiggyBag pig;

	info (system, "eazel_package_system_rpm3_query (dbpath=\"%s\", key=%p, flag=%d, detail=%d)", 
	      dbpath, key, flag, detail_level);
	
	pig.system = system;
	pig.key = key;
	pig.flag = flag;
	pig.detail_level = detail_level;
	pig.result = &result;
	
	eazel_package_system_rpm3_open_dbs (system);
	if (dbpath==NULL) {
		g_hash_table_foreach (system->private->dbs, 
				      (GHFunc)(EAZEL_PACKAGE_SYSTEM_RPM3_CLASS (GTK_OBJECT (system)->klass)->query_foreach),
				      &pig);
	} else {
		(EAZEL_PACKAGE_SYSTEM_RPM3_CLASS (GTK_OBJECT (system)->klass)->query_foreach) (dbpath, NULL, &pig);
	}
	eazel_package_system_rpm3_close_dbs (system);

	return result;
}

/************************************************************
 Install implemementation
*************************************************************/

static void
display_arguments (EazelPackageSystemRpm3 *system,
		   GList *args) 
{
	char *str, *tmp;
	GList *iterator;

	str = g_strdup ("rpm");
	for (iterator = args; iterator; iterator = g_list_next (iterator)) {
		tmp = g_strdup_printf ("%s %s", str, (char*)iterator->data);
		g_free (str);
		str = tmp;
		/* Since there is a mex lenght on g_message output ... */
		if (strlen (str) > 1000) {
			info (system, "%s", str);
			g_free (str);
			str = g_strdup ("");
		}
	}
	info (system, "%s", str);
}

static void
monitor_subcommand_pipe (EazelPackageSystemRpm3 *system,
			 int fd, 
			 GIOFunc monitor_func,
			 struct RpmMonitorPiggyBag *pig)
{
	GIOChannel *channel;

	pig->subcommand_running = TRUE;
	channel = g_io_channel_unix_new (fd);

	info (system, "beginning monitor on %d", fd);
	g_io_add_watch (channel, G_IO_IN | G_IO_ERR | G_IO_NVAL | G_IO_HUP, 
			monitor_func, 
			pig);

	while (pig->subcommand_running) {
		g_main_iteration (TRUE);
	}
	info (system, "ending monitor on %d", fd);
}

static void 
eazel_package_system_rpm3_execute (EazelPackageSystemRpm3 *system,
				   struct RpmMonitorPiggyBag *pig,
				   GList *args)
{
	TrilobiteRootHelper *root_helper;
	int fd;
	gboolean go = TRUE;

	display_arguments (system, args);

	root_helper = gtk_object_get_data (GTK_OBJECT (system), "trilobite-root-helper");
	if (root_helper) {
		TrilobiteRootHelperStatus root_helper_stat;

		root_helper_stat = trilobite_root_helper_start (root_helper);
		if (root_helper_stat != TRILOBITE_ROOT_HELPER_SUCCESS) {
			g_warning ("Error in starting trilobite_root_helper");			
			go = FALSE;
		} else if (trilobite_root_helper_run (root_helper, 
						      TRILOBITE_ROOT_HELPER_RUN_RPM, args, &fd) != 
			   TRILOBITE_ROOT_HELPER_SUCCESS) {
			g_warning ("Error in running trilobite_root_helper");
			trilobite_root_helper_destroy (GTK_OBJECT (root_helper));
			go = FALSE;
		}
	} else {
		/* FIXME:
		   ugh, start /bin/rpm manually, see code in eazel-install-logic.c rev 1.26 */
		g_assert (root_helper);
		go = FALSE;
	}
	if (go) {
#ifdef USE_PERCENT
		monitor_subcommand_pipe (system, fd, (GIOFunc)monitor_rpm_process_pipe_percent_output, pig);
#else
		monitor_subcommand_pipe (system, fd, (GIOFunc)monitor_rpm_process_pipe, pig);
#endif
	} else {
		/* FIXME: fail all the packages in pig */
	}
}

/* If any package in "packages" does not occur in "seen",
   emit failed signal */
static void
check_if_all_packages_seen (EazelPackageSystemRpm3 *system, 
			    const char *dbpath,
			    EazelPackageSystemOperation op,
			    GList *packages,
			    GList *seen)
{
	GList *iterator;
	
	/* HACK: that fixes bugzilla.eazel.com 4914 */		  
	eazel_package_system_rpm3_open_dbs (system);

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		/* HACK: that fixes bugzilla.eazel.com 4914 */		  
		if (op==EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL) {
			if (eazel_package_system_is_installed (EAZEL_PACKAGE_SYSTEM (system),
							       dbpath,
							       pack->name,
							       pack->version,
							       pack->minor,
							       EAZEL_SOFTCAT_SENSE_EQ)) {
				fail (system, "%s is still installed", pack->name);
				eazel_package_system_emit_failed (EAZEL_PACKAGE_SYSTEM (system), op, pack);
			} else {
				eazel_package_system_emit_start (EAZEL_PACKAGE_SYSTEM (system),
								 op,
								 pack);				
				eazel_package_system_emit_end (EAZEL_PACKAGE_SYSTEM (system),
							       op,
							       pack);
			}
		} else {
			if (!g_list_find_custom (seen, 
						 pack,
						 (GCompareFunc)eazel_install_package_compare)) {
				fail (system, "did not see %s", pack->name);
				eazel_package_system_emit_failed (EAZEL_PACKAGE_SYSTEM (system), op, pack);
			}
		}
	}

	/* HACK: that fixes bugzilla.eazel.com 4914 */		  
	eazel_package_system_rpm3_close_dbs (system);
}
			    

static void 
eazel_package_system_rpm3_install_uninstall (EazelPackageSystemRpm3 *system, 
					     EazelPackageSystemOperation op,
					     const char *dbpath,
					     GList* packages,
					     unsigned long flags)
{
	struct RpmMonitorPiggyBag pig = rpmmonitorpiggybag_new (system, op);
	GList *args = NULL;

	pig.system = system;
	pig.op = op;

	pig.total_packages = g_list_length (packages);
	pig.total_bytes = get_total_size_of_packages (packages);
	pig.packages_to_expect = packages;
	pig.name_to_package = rpm_make_names_to_package_hash (packages);

	make_rpm_argument_list (system, op, flags, dbpath, packages, &args);
	eazel_package_system_rpm3_execute (system, &pig, args);
	destroy_string_list (args);

	check_if_all_packages_seen (system, dbpath, op, packages, pig.packages_seen);
	g_list_free (pig.packages_seen);

	g_hash_table_foreach_remove (pig.name_to_package, (GHRFunc)clear_name_to_package, NULL);
	g_hash_table_destroy (pig.name_to_package);
}

void                 
eazel_package_system_rpm3_install (EazelPackageSystemRpm3 *system, 
				   const char *dbpath,
				   GList* packages,
				   unsigned long flags)
{
	info (system, "eazel_package_system_rpm3_install");

	eazel_package_system_rpm3_install_uninstall (system, 
						     EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL,
						     dbpath,
						     packages,
						     flags);
}

/************************************************************
 Uninstall implemementation
*************************************************************/

void                 
eazel_package_system_rpm3_uninstall (EazelPackageSystemRpm3 *system, 
				     const char *dbpath,
				     GList* packages,
				     unsigned long flags)
{
	info (system, "eazel_package_system_rpm3_uninstall");
	eazel_package_system_rpm3_install_uninstall (system, 
						     EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL,
						     dbpath,
						     packages,
						     flags);
}

/************************************************************
 Verify implemementation
*************************************************************/

static void
eazel_package_system_rpm3_verify_impl (EazelPackageSystemRpm3 *system, 
				       const char *root,
				       PackageData *package,
				       unsigned long *info,
				       gboolean *cont)
{
	unsigned int i;
	int result;
	unsigned long infoblock[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS];

	g_assert (package->packsys_struc);

	infoblock[0] = 0;
	infoblock[1] = g_list_length (package->provides);
	infoblock[2] = info[0];
	infoblock[3] = info[1];
	infoblock[4] = info[2];
	infoblock[5] = info[3];

	(*cont) = eazel_package_system_emit_start (EAZEL_PACKAGE_SYSTEM (system), 
						   EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY,
						   package);
	/* abort if signal returns false */
	if (*cont == FALSE) {
		return;
	}
	for (i = 0; i < g_list_length (package->provides); i++) {
		int res;
		/* next file... */
		infoblock [0]++;
		infoblock [4]++;
		
		info (system, "checking file %d/%d \"%s\" from \"%s\"", 
		      infoblock[0], g_list_length (package->provides),
		      (char*)((g_list_nth (package->provides, i))->data),
		      package->name);

		(*cont) = eazel_package_system_emit_progress (EAZEL_PACKAGE_SYSTEM (system), 
							      EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY,
							      package, 
							      infoblock);
		/* abort if signal returns false */
		if (*cont == FALSE) {
			break;
		}
		res = rpmVerifyFile ("", (Header)package->packsys_struc, i, &result, 0);
		if (res!=0) {
			fail (system, "%d failed", i);
			(*cont) = eazel_package_system_emit_failed (EAZEL_PACKAGE_SYSTEM (system), 
								    EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY,
								    package);
			
			/* abort if signal returns false */
			if (*cont == FALSE) {
				break;
			}
		} 
		
	}
	/* Update the total-amount-completed counter */
	info[2] = infoblock[4];

	if (*cont) {
		(*cont) = eazel_package_system_emit_end (EAZEL_PACKAGE_SYSTEM (system), 
							 EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY,
							 package);
		/* no need to check, called will abort if *cont == FALSE */
	}
}

static unsigned long
get_num_of_files_in_packages (GList *packages)
{
	GList *iterator;
	unsigned long result = 0;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		result += g_list_length (pack->provides);
	}
	return result;
}

void                 
eazel_package_system_rpm3_verify (EazelPackageSystemRpm3 *system, 
				  const char *dbpath,
				  GList* packages)
{
	GList *iterator;
	char *root = ""; /* FIXME: fill this using dbpath */
	unsigned long info[4];
	gboolean cont = TRUE;

	info[0] = 0;
	info[1] = g_list_length (packages);
	info[2] = 0; /* updated by eazel_package_system_rpm3_verify_impl */
	info[3] = get_num_of_files_in_packages (packages);

	info (system, "eazel_package_system_rpm3_verify");

	eazel_package_system_rpm3_open_dbs (system);
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		info[0] ++;
		eazel_package_system_rpm3_verify_impl (system, root, pack, info, &cont);
		if (cont == FALSE) {
			break;
		}
	}
	eazel_package_system_rpm3_close_dbs (system);
}

/************************************************************
 Version compare implementation							    
*************************************************************/

int
eazel_package_system_rpm3_compare_version (EazelPackageSystem *system,
					   const char *a,
					   const char *b)
{
	return rpmvercmp (a, b);
}

/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_rpm3_finalize (GtkObject *object)
{
	EazelPackageSystemRpm3 *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM_RPM3 (object));

	system = EAZEL_PACKAGE_SYSTEM_RPM3 (object);

	eazel_package_system_rpm3_free_dbs (system);
	g_hash_table_destroy (system->private->dbs);

	if (GTK_OBJECT_CLASS (eazel_package_system_rpm3_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_rpm3_parent_class)->finalize (object);
	}
}

static void
eazel_package_system_rpm3_class_initialize (EazelPackageSystemRpm3Class *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_rpm3_finalize;
	
	eazel_package_system_rpm3_parent_class = gtk_type_class (eazel_package_system_get_type ());
#ifdef HAVE_RPM_30
	klass->query_foreach = (EazelPackageSystemRpmQueryForeachFunc)eazel_package_system_rpm3_query_foreach;
	klass->query_impl = (EazelPackageSystemRpmQueryImplFunc)eazel_package_system_rpm3_query_impl;
#else
	klass->query_foreach = NULL;
	klass->query_impl = NULL;
#endif /* HAVE_RPM_30 */
}

static void
eazel_package_system_rpm3_initialize (EazelPackageSystemRpm3 *system) 
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_RPM3 (system));
	
	system->private = g_new0 (EazelPackageSystemRpm3Private, 1);
	system->private->dbs = g_hash_table_new (g_str_hash, g_str_equal);
	system->private->db_to_root = g_hash_table_new (g_str_hash, g_str_equal);
}

GtkType
eazel_package_system_rpm3_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystemRpm3",
			sizeof (EazelPackageSystemRpm3),
			sizeof (EazelPackageSystemRpm3Class),
			(GtkClassInitFunc) eazel_package_system_rpm3_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_rpm3_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (eazel_package_system_get_type (), &system_info);
	}

	return system_type;
}

EazelPackageSystemRpm3 *
eazel_package_system_rpm3_new (GList *dbpaths) 
{
	EazelPackageSystemRpm3 *system;

	g_return_val_if_fail (dbpaths, NULL);

	system = EAZEL_PACKAGE_SYSTEM_RPM3 (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM_RPM3, NULL));

	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));

	eazel_package_system_rpm3_create_dbs (system, dbpaths);

	return system;
}

#ifdef HAVE_RPM_30
EazelPackageSystem*
eazel_package_system_implementation (GList *dbpaths)
{
	EazelPackageSystem *result;
	GList *tdbpaths = dbpaths;

	g_message ("Eazel Package System - rpm3");
	
	tdbpaths = g_list_prepend (tdbpaths, g_strdup (DEFAULT_ROOT));
	tdbpaths = g_list_prepend (tdbpaths, g_strdup (DEFAULT_DB_PATH));
	result = EAZEL_PACKAGE_SYSTEM (eazel_package_system_rpm3_new (tdbpaths));
	
	result->private->load_package = 
		(EazelPackageSytemLoadPackageFunc)eazel_package_system_rpm3_load_package;
	result->private->query = (EazelPackageSytemQueryFunc)eazel_package_system_rpm3_query;
	result->private->install = (EazelPackageSytemInstallFunc)eazel_package_system_rpm3_install;
	result->private->uninstall = (EazelPackageSytemUninstallFunc)eazel_package_system_rpm3_uninstall;
	result->private->verify = (EazelPackageSytemVerifyFunc)eazel_package_system_rpm3_verify;
	result->private->compare_version = 
		(EazelPackageSystemCompareVersionFunc)eazel_package_system_rpm3_compare_version;

	return result;
}
#endif /* HAVE_RPM_30 */

#endif /* HAVE_RPM */
