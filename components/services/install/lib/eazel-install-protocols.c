/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 1998-1999 James Henstridge
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
 *          Robey Pointer <robey@eazel.com>
 *          J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-protocols.h"
#include "eazel-install-private.h"
#include "eazel-softcat.h"
#include <ghttp.h>
#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "eazel-package-system.h"

/* We use rpmvercmp to compare versions... */
#include <rpm/rpmlib.h>
#include <rpm/misc.h>

#include <libtrilobite/trilobite-core-utils.h>

#ifndef EAZEL_INSTALL_SLIM
#include <libgnomevfs/gnome-vfs.h>
#endif /* EAZEL_INSTALL_SLIM */

/* evil evil hack because RPM doesn't understand that a package for i386 is still okay to run on i686! */
#define ASSUME_ix86_IS_i386

typedef struct {
	EazelInstall *service;
	const PackageData *package;
} gnome_vfs_callback_struct;

typedef gboolean (*eazel_install_file_fetch_function) (gpointer *obj, 
						       char *url,
						       const char *file_to_report,
						       const char *target_file,
						       const PackageData *package);


#ifdef EAZEL_INSTALL_SLIM	       

gboolean http_fetch_remote_file (EazelInstall *service,
				 char *url, 
				 const char *file_to_report,
				 const char* target_file,
				 const PackageData *package);
gboolean ftp_fetch_remote_file (EazelInstall *service,
				char *url, 
				const char *file_to_report,
				const char* target_file,
				const PackageData *package);
#else /* EAZEL_INSTALL_SLIM */
gboolean  gnome_vfs_fetch_remote_file (EazelInstall *service, 
				       char *url, 
				       const char *file_to_report, 
				       const char *target_file,
				       const PackageData *package);
#endif /* EAZEL_INSTALL_SLIM */
gboolean local_fetch_remote_file (EazelInstall *service,
				  char *url, 
				  const char *file_to_report,
				  const char* target_file,
				  const PackageData *package);


#ifdef EAZEL_INSTALL_SLIM
gboolean
http_fetch_remote_file (EazelInstall *service,
			char *url, 
			const char *file_to_report,
			const char* target_file,
			const PackageData *package) 
{
        int length, get_failed;
        ghttp_request* request;
        ghttp_status status;
        char* body;
        FILE* file;
	int total_bytes = 0;
	int last_flush_bytes = 0;
	gboolean first_emit;
	const char *report;
	char *target_file_premove;

	report = file_to_report ? file_to_report : g_basename (target_file);
	trilobite_debug (_("Downloading %s..."), url);	

	target_file_premove = g_strdup_printf ("%s~", target_file);

        file = fopen (target_file_premove, "wb");
        get_failed = 0;
        length = -1;
        request = NULL;
        body = NULL;
	first_emit = TRUE;

	if (file == NULL) {
		get_failed = 1;
		g_warning (_("Could not open target file %s"), target_file_premove);
		g_free (target_file_premove);
		return FALSE;
	}

        request = ghttp_request_new();
        if (!request) {
                g_warning (_("Could not create an http request !"));
                get_failed = 1;
        }

	/* bootstrap installer does it this way */
	if (g_getenv ("http_proxy") != NULL) {
		if (ghttp_set_proxy (request, g_getenv ("http_proxy")) != 0) {
			g_warning (_("Proxy: Invalid uri !"));
			get_failed = 1;
		}
	}
        if (ghttp_set_uri (request, url) != 0) {
                g_warning (_("Invalid uri !"));
                get_failed = 1;
        }

        ghttp_set_header (request, http_hdr_Connection, "close");
        ghttp_set_header (request, http_hdr_User_Agent, trilobite_get_useragent_string (NULL));
        if (ghttp_prepare (request) != 0) {
                g_warning (_("Could not prepare http request !"));
                get_failed = 1;
        }

        if (ghttp_set_sync (request, ghttp_async)) {
                g_warning (_("Couldn't get async mode "));
                get_failed = 1;
        }

        while ((status = ghttp_process (request)) == ghttp_not_done) {
                ghttp_current_status curStat = ghttp_get_status (request);

		total_bytes = curStat.bytes_total;
		/* Ensure first emit is with amount==0 */
		if (first_emit && total_bytes > 0) {
			eazel_install_emit_download_progress (service, package, 0, total_bytes);
			first_emit = FALSE;
		}
		/* And that amount==0 & amount==total only occurs once */
		if (curStat.bytes_read!=0 && (curStat.bytes_read != curStat.bytes_total)) {
			eazel_install_emit_download_progress (service, 
							      package,
							      curStat.bytes_read, 
							      curStat.bytes_total);
		}

		/* arbitrary -- flush every 16k or so */
		if (curStat.bytes_read > last_flush_bytes + 16384) {
			ghttp_flush_response_buffer (request);
			length = ghttp_get_body_len (request);
			body = ghttp_get_body (request);
			if (body != NULL) {
				if (fwrite (body, length, 1, file) < 1) {
					/* probably out of disk space */
					g_warning (_("DISK FULL: could not write %s"), target_file);
					service->private->disk_full = TRUE;
					get_failed = 1;
					break;
				}
			} else {
				g_warning (_("Could not get request body!"));
				get_failed = 1;
				break;
			}
			last_flush_bytes = curStat.bytes_read;
		}

		g_main_iteration (FALSE);
        }
	/* Last emit amount==total */
	eazel_install_emit_download_progress (service, package, total_bytes, total_bytes);

        if (ghttp_status_code (request) != 200) {
                g_warning (_("HTTP error: %d %s"), ghttp_status_code (request),
                         ghttp_reason_phrase (request));
                get_failed = 1;
        }
	if (ghttp_status_code (request) != 404) {
		length = ghttp_get_body_len (request);
		body = ghttp_get_body (request);
		if (body != NULL) {
			if (fwrite (body, length, 1, file) < 1) {
				/* probably out of disk space */
				g_warning (_("DISK FULL: could not write %s"), target_file);
				service->private->disk_full = TRUE;
				get_failed = 1;
			}
		} else {
			g_warning (_("Could not get request body!"));
			get_failed = 1;
		}
	} else {
		get_failed = 1;
	}

        if (request) {
                ghttp_request_destroy (request);
        }
        if (fclose (file) != 0) {
		g_warning (_("DISK FULL: could not write %s"), target_file);
		service->private->disk_full = TRUE;
		get_failed = 1;
	}

	if (! get_failed) {
		rename (target_file_premove, target_file);
	}
	g_free (target_file_premove);

        if (get_failed != 0) {
		return FALSE;
        } else {
		return TRUE;
	}
} /* end http_fetch_remote_file */

gboolean
ftp_fetch_remote_file (EazelInstall *service,
		       char *url, 
		       const char *file_to_report,
		       const char* target_file,
		       const PackageData *package)
{
	trilobite_debug (_("Downloading %s..."), url);
	trilobite_debug (_("FTP not supported yet"));
	return FALSE;
}

#else /* EAZEL_INSTALL_SLIM */

static int
gnome_vfs_xfer_callback (GnomeVFSXferProgressInfo *info,
			 gnome_vfs_callback_struct *cbstruct)

{
	static gboolean initial_emit;
	static gboolean last_emit;
	EazelInstall *service = cbstruct->service;
	const PackageData *package = cbstruct->package;

	switch (info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
		trilobite_debug ("GnomeVFS Error: %s\n",
			   gnome_vfs_result_to_string (info->vfs_status));
		return FALSE;
		break;
	case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
		trilobite_debug ("Overwriting `%s' with `%s'",
			   info->target_name, info->source_name);
		return TRUE;
		break;
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		if (service->private->cancel_download) {
			/* tell gnome-vfs to STOP */
			trilobite_debug ("telling gnome-vfs to cancel download");
			return FALSE;
		}
		switch (info->phase) {
		case GNOME_VFS_XFER_PHASE_INITIAL:
			initial_emit = TRUE;
			last_emit = FALSE;
			return TRUE;
		case GNOME_VFS_XFER_PHASE_COLLECTING:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_READYTOGO:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_OPENSOURCE:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_OPENTARGET:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_COPYING:
			if (initial_emit && info->file_size>0) {
				initial_emit = FALSE;
				eazel_install_emit_download_progress (service,
								      package,
								      0,
								      info->file_size);
			} else if (!last_emit && info->bytes_copied == info->file_size) {
				last_emit = TRUE;
				eazel_install_emit_download_progress (service, 
								      package,
								      info->file_size,
								      info->file_size);
			} else if (info->bytes_copied > 0) {
				eazel_install_emit_download_progress (service, 
								      package,
								      info->bytes_copied,
								      info->file_size);

			}

			/*
			g_message ("Transferring `%s' to `%s' (file %ld/%ld, byte %ld/%ld in file, "
				   "%" GNOME_VFS_SIZE_FORMAT_STR "/%" GNOME_VFS_SIZE_FORMAT_STR " total)",
				   info->source_name,
				   info->target_name,
				   info->file_index,
				   info->files_total,
				   (glong) info->bytes_copied,
				   (glong) info->file_size,
				   info->total_bytes_copied,
				   info->bytes_total);
			*/
			return TRUE;
		case GNOME_VFS_XFER_PHASE_CLOSESOURCE:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_CLOSETARGET:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_FILECOMPLETED:
			return TRUE;
		case GNOME_VFS_XFER_PHASE_COMPLETED:
			if (!last_emit) {
				last_emit = TRUE;
				eazel_install_emit_download_progress (service, 
								      package,
								      info->file_size,
								      info->file_size);
			}
			return TRUE;
		default:
			trilobite_debug ("Unexpected phase %d", info->phase);
			return FALSE; /* keep going anyway */
		}
		break;
	case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
		trilobite_debug ("Duplicate");
		return FALSE;
	default:
		trilobite_debug ("Unknown status");
		return FALSE;
	}       

	return FALSE; 	
}

gboolean 
gnome_vfs_fetch_remote_file (EazelInstall *service, 
			     char *url, 
			     const char *file_to_report, 
			     const char *target_file,
			     const PackageData *package)
{
	GnomeVFSResult result;
	GnomeVFSXferOptions xfer_options = 0;
	GnomeVFSURI *src_uri;
	GnomeVFSURI *dest_uri;
	char *t_file;
	char *target_file_premove;
	gnome_vfs_callback_struct *cbstruct;

	target_file_premove = g_strdup_printf ("%s~", target_file);

	/* this will always be a file: uri */
	t_file = g_strdup_printf ("file://%s", target_file_premove);

	trilobite_debug ("gnome_vfs_xfer_uri ( %s %s )", url, t_file);
	
	src_uri = gnome_vfs_uri_new (url);
	g_assert (src_uri != NULL);
	if (eazel_install_get_ssl_rename (service)) {
		trilobite_debug ("ssl renaming %s to localhost", gnome_vfs_uri_get_host_name (src_uri));
		gnome_vfs_uri_set_host_name (src_uri, "localhost");
	}
	
	dest_uri = gnome_vfs_uri_new (target_file_premove);
	g_assert (dest_uri != NULL);
	
	/* Setup the userdata for the callback, I need both the
	   service object to emit signals too, and the filename to report */
	cbstruct = g_new0 (gnome_vfs_callback_struct, 1);
	cbstruct->service = service;
	cbstruct->package = package;

	/* Execute the gnome_vfs copy */
	service->private->cancel_download = FALSE;
	result = gnome_vfs_xfer_uri (src_uri, dest_uri,
				     xfer_options,
				     GNOME_VFS_XFER_ERROR_MODE_QUERY,
				     GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
				     (GnomeVFSXferProgressCallback)gnome_vfs_xfer_callback,
				     cbstruct);

	if (result==GNOME_VFS_OK) {
		chmod (target_file, 0600);
		trilobite_debug ("File download successful");
		rename (target_file_premove, target_file);
	} else {
		trilobite_debug ("File download failed");
		if (result == GNOME_VFS_ERROR_BAD_PARAMETERS) {
			trilobite_debug ("gnome_vfs_xfer_uri returned BAD_PARAMETERS");
		}
		if (service->private->cancel_download) {
			trilobite_debug ("download was cancelled from afar");
		}
	}

	/* Free the various stuff */
	g_free (t_file);
	g_free (cbstruct);

	/* Unref all the uri's */
	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dest_uri);
	
	return result == GNOME_VFS_OK ? TRUE : FALSE;
}
#endif /* EAZEL_INSTALL_SLIM */

gboolean
local_fetch_remote_file (EazelInstall *service,
			 char *url, 
			 const char *file_to_report,
			 const char* target_file,
			 const PackageData *package)
{
	gboolean result;
	const char *report;

	report = file_to_report ? file_to_report : g_basename (target_file);
	trilobite_debug (_("Checking local file %s..."), target_file);
	result = FALSE;
	if (access (target_file, R_OK|W_OK) == 0) {
		struct stat sbuf;
		stat (target_file, &sbuf);
		/* Emit bogus download progress */
		eazel_install_emit_download_progress (service, package,            0, sbuf.st_size);
		eazel_install_emit_download_progress (service, package, sbuf.st_size, sbuf.st_size);
		result = TRUE;
	} 
	return result;
}

static eazel_install_file_fetch_function*
eazel_install_fill_file_fetch_table (void)
{
	eazel_install_file_fetch_function *res;

	res = g_new0 (eazel_install_file_fetch_function, 3);
#ifdef EAZEL_INSTALL_SLIM
	res [PROTOCOL_HTTP] = (eazel_install_file_fetch_function)http_fetch_remote_file;
	res [PROTOCOL_FTP] = (eazel_install_file_fetch_function)ftp_fetch_remote_file;
#else /* EAZEL_INSTALL_SLIM */
	res [PROTOCOL_HTTP] = (eazel_install_file_fetch_function)gnome_vfs_fetch_remote_file;
	res [PROTOCOL_FTP] = (eazel_install_file_fetch_function)gnome_vfs_fetch_remote_file;
#endif /* EAZEL_INSTALL_SLIM */
	res [PROTOCOL_LOCAL] = (eazel_install_file_fetch_function)local_fetch_remote_file;
	
	return res;
}

static void
my_copy_file (const char *orig, const char *dest)
{
	int ofd, nfd;
	char buffer[1024];
	int n, n2, count;

	ofd = open (orig, O_RDONLY);
	if (ofd < 0) {
		return;
	}
	nfd = open (dest, O_CREAT|O_WRONLY, 0644);
	if (nfd < 0) {
		close (ofd);
		return;
	}

	while (1) {
		n = read (ofd, buffer, sizeof (buffer));
		if (n <= 0) {
			break;
		}
		count = 0;
		while (count < n) {
			n2 = write (nfd, buffer+count, n-count);
			if (n2 < 0) {
				/* error */
				close (ofd);
				close (nfd);
				unlink (dest);
				return;
			}
			count += n2;
		}
	}
	close (ofd);
	close (nfd);
	return;
}

gboolean
eazel_install_fetch_file (EazelInstall *service,
			  char *url, 
			  const char *file_to_report,
			  const char* target_file,
			  const PackageData *package)
{
	gboolean result;
	GList *iter;
	char *filename;

	static eazel_install_file_fetch_function *func_table = NULL;
	
	if (!func_table) {
		func_table = eazel_install_fill_file_fetch_table ();
	}
	
	result = FALSE;

	g_return_val_if_fail (url!=NULL, FALSE);
	g_return_val_if_fail (target_file!=NULL, FALSE);

	for (iter = g_list_first (service->private->local_repositories); iter != NULL; iter = g_list_next (iter)) {
		filename = g_strdup_printf ("%s/%s", (char *)(iter->data), g_basename (target_file));
		if (g_file_test (filename, G_FILE_TEST_ISFILE)) {
			/* copy this file to target_file */
			trilobite_debug ("%s found at %s, copying", file_to_report, filename);
			my_copy_file (filename, target_file);
		}
		g_free (filename);
	}

	if (g_file_test (target_file, G_FILE_TEST_ISFILE)) {
		/* File is already present, so just emit to progress callbacks and get on with
		   your life */
		struct stat buf;
		stat (target_file, &buf);
		trilobite_debug ("%s already present, not downloading", target_file);
		eazel_install_emit_download_progress (service, package, 0, buf.st_size);
		eazel_install_emit_download_progress (service, package, buf.st_size, buf.st_size);
		result = TRUE;
	} else {
		result = (func_table [eazel_install_get_protocol (service)])((gpointer)service, 
									     url, 
									     file_to_report, 
									     target_file,
									     package);
	}
 	
	if (!result) {
		g_warning (_("Failed to retrieve %s!"), 
			   file_to_report ? file_to_report : g_basename (target_file));
		eazel_install_emit_download_failed (service, package);
	} 

	return result;
}


static const char*
filename_from_url (const char *url)
{
	static char *filename = NULL;
	const char *ptr;

	g_return_val_if_fail (url!=NULL, NULL);

	g_free (filename);

	ptr = url + strlen (url);
	while ((ptr != url) && (*ptr != '/')) { 
		ptr--; 
	}
	if (ptr == url) {
		filename = g_strdup (ptr);
	} else {
		filename = g_strdup (ptr + 1);
	}
	
	return filename;
}

gboolean
eazel_install_fetch_package (EazelInstall *service, 
			     PackageData* package) 
{
	gboolean result = FALSE;
	char* url = NULL;
	char* targetname = NULL;
	char *name = g_strdup (package->name);
	char *version = g_strdup (package->version);

	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_FTP:
	case PROTOCOL_HTTP: 
	{
		if (eazel_softcat_get_info (service->private->softcat, package, EAZEL_SOFTCAT_SENSE_GE,
					    PACKAGE_FILL_NO_PROVIDES | PACKAGE_FILL_NO_DEPENDENCIES) 
		    == EAZEL_SOFTCAT_SUCCESS) {
			url = g_strdup (package->remote_url);
		} else {
			url = NULL;
		}
	}
	break;
	case PROTOCOL_LOCAL:
		url = g_strdup_printf ("%s", rpmfilename_from_packagedata (package));
		break;
	};

	if (url == NULL) {
		char *rname = packagedata_get_readable_name (package);
		g_warning (_("Could not get an URL for %s"), rname);
		g_free (rname);
	} else {
		targetname = g_strdup_printf ("%s/%s",
					      eazel_install_get_tmp_dir (service),
					      filename_from_url (url));
		result = eazel_install_fetch_file (service, url, package->name, targetname, package);
		if (result) {
			package = eazel_package_system_load_package (service->private->package_system,
								     package, 
								     targetname,
								     PACKAGE_FILL_NO_DIRS_IN_PROVIDES|PACKAGE_FILL_NO_DEPENDENCIES);
							   
			if (name) {
				if (strcmp (name, package->name)) {
					g_warning (_("Downloaded package does not have the correct name"));
					g_warning (_("Package %s should have had name %s"), 
						   package->name, name);
					result = FALSE;
				}
			}
			if (version) {
				if (rpmvercmp (package->version, version)<0) {
					g_warning (_("Downloaded package does not have the correct version"));
					g_warning (_("Package %s had version %s and not %s"), 
						   package->name, package->version, version);
					result = FALSE;
				}
			}
		} 
	}
	
	if (result) {
		/* By always adding the file to the downloaded_files list,
		   we enforce md5 check on files that were present but should have
		   been downloaded */
		if (! g_list_find_custom (service->private->downloaded_files, (char *)targetname,
					  (GCompareFunc)g_strcasecmp)) {
			service->private->downloaded_files = g_list_prepend (service->private->downloaded_files,
									     g_strdup (targetname));
		}
		trilobite_debug ("%s resolved v2", package->name);
	} else {	
		g_warning (_("File download failed"));
		unlink (targetname);
	} 
	g_free (name);
	g_free (version);
	g_free (targetname);
	g_free (url);

	return result;
}


static void
flatten_tree_func (PackageData *pack, GList **out)
{
	trilobite_debug ("    --- %s", pack->name);
	*out = g_list_append (*out, pack);
	g_list_foreach (pack->hard_depends, (GFunc)flatten_tree_func, out);
	g_list_foreach (pack->soft_depends, (GFunc)flatten_tree_func, out);
	g_list_free (pack->hard_depends);
	g_list_free (pack->soft_depends);
	pack->hard_depends = pack->soft_depends = NULL;
}

/* this was never used yet */
#if 0
/* given a list of packages with incomplete info (for example, the initial bootstrap install list),
 * go ask for real info and compile a new list of packages.
 */
static GList *
eazel_install_fetch_definitive_package_info (EazelInstall *service, PackageData *pack)
{
	char *search_url = NULL;
	char *body = NULL;
	int length;
	GList *treelist;
	GList *out;

	search_url = get_search_url_for_package (service, RPMSEARCH_ENTRY_NAME, pack);
	if (search_url == NULL) {
		trilobite_debug ("No search URL");
		return NULL;
	}

	trilobite_debug ("Search URL: %s", search_url);
	trilobite_setenv ("GNOME_VFS_HTTP_USER_AGENT", trilobite_get_useragent_string (NULL), TRUE);

	if (! trilobite_fetch_uri (search_url, &body, &length)) {
		g_free (search_url);
		trilobite_debug ("Couldn't fetch search URL");
		return NULL;
	}

	trilobite_debug ("parse osd");
	treelist = parse_osd_xml_from_memory (body, length);
	g_free (search_url);
	g_free (body);

	/* the install lib will spaz if we give it the dependencies in their tree form. :( */
	trilobite_debug (">>> package '%s' => %d packages", pack->name, g_list_length (treelist));
	out = NULL;
	g_list_foreach (treelist, (GFunc)flatten_tree_func, &out);

dump_packages (out);
	return out;
}

void
eazel_install_fetch_definitive_category_info (EazelInstall *service, CategoryData *category)
{
	GList *iter;
	GList *real_packages = NULL;
	GList *packlist;

	for (iter = g_list_first (category->packages); iter != NULL; iter = g_list_next (iter)) {
		packlist = eazel_install_fetch_definitive_package_info (service, (PackageData *)(iter->data));
		if (packlist != NULL) {
			real_packages = g_list_concat (real_packages, packlist);
			packagedata_destroy ((PackageData *)(iter->data), TRUE);
		} else {
			/* fetch of real URL failed, so just add the original package info */
			real_packages = g_list_append (real_packages, (PackageData *)(iter->data));
		}
	}

	g_list_free (category->packages);
	category->packages = real_packages;
}
#endif
