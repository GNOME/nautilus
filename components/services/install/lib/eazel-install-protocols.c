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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-protocols.h"
#include "eazel-install-private.h"
#include "eazel-install-xml-package-list.h"
#include <ghttp.h>
#include <config.h>
#include <sys/utsname.h>
#include <errno.h>

#include <libtrilobite/trilobite-core-utils.h>

#ifndef EAZEL_INSTALL_SLIM
#include <libgnomevfs/gnome-vfs.h>
#endif /* EAZEL_INSTALL_SLIM */

typedef struct {
	EazelInstall *service;
	const char *file_to_report;
} gnome_vfs_callback_struct;

typedef gboolean (*eazel_install_file_fetch_function) (gpointer *obj, 
						       char *url,
						       const char *file_to_report,
						       const char *target_file);

/* This string defines the url for the rpmsearch cgi script.
   It should contain a %s for the server name, and later 
   a %d for the portnumber. In this order, no other
   order */
#define CGI_BASE "http://%s:%d/catalog/find" 

#ifdef EAZEL_INSTALL_SLIM	       

gboolean http_fetch_remote_file (EazelInstall *service,
				 char *url, 
				 const char *file_to_report,
				 const char* target_file);
gboolean ftp_fetch_remote_file (EazelInstall *service,
				char *url, 
				const char *file_to_report,
				const char* target_file);
#else /* EAZEL_INSTALL_SLIM */
gboolean  gnome_vfs_fetch_remote_file (EazelInstall *service, 
				       char *url, 
				       const char *file_to_report, 
				       const char *target_file);
#endif /* EAZEL_INSTALL_SLIM */
gboolean local_fetch_remote_file (EazelInstall *service,
				  char *url, 
				  const char *file_to_report,
				  const char* target_file);

typedef enum { RPMSEARCH_ENTRY_NAME, RPMSEARCH_ENTRY_PROVIDES } RpmSearchEntry;

/* This method takes a RpmSearch, which describes the thing to search for
   (ie. a package name or a package that provides file foo).
   If RPMSEARCH_ENTRY_NAME, data must be a PackageData pointer,
   if PROVIDES, it must be a string containing the file which is needed.
   It creates a search url when can be used to get info about the package */
char* get_search_url_for_package (EazelInstall *service, 
				  RpmSearchEntry, 
				  const gpointer data);

/* This method takes a RpmSearch, which describes the thing to search for
   (ie. a package name or a package that provides file foo).
   If RPMSEARCH_ENTRY_NAME, data must be a PackageData pointer,
   if PROVIDES, it must be a string containing the file which is needed.
   The last argument is a PackageData structure to insert info into, 
   eg. the url (->filename), the server md5 (->md5).
   It uses get_search_url_for_package, downloads the contents of the url and
   parses it and returns a url for the package itself */
char* get_url_for_package (EazelInstall *service, 
			   RpmSearchEntry, 
			   const gpointer data, 
			   PackageData *pack);

#ifdef EAZEL_INSTALL_SLIM
gboolean
http_fetch_remote_file (EazelInstall *service,
			char *url, 
			const char *file_to_report,
			const char* target_file) 
{
        int length, get_failed;
        ghttp_request* request;
        ghttp_status status;
        char* body;
        FILE* file;
	int total_bytes;
	gboolean first_emit;
	const char *report;

	report = file_to_report ? file_to_report : g_basename (target_file);
	g_message (_("Downloading %s..."), url);	

        file = fopen (target_file, "wb");
        get_failed = 0;
        length = -1;
        request = NULL;
        body = NULL;
	first_emit = TRUE;

	if (file == NULL) {
		get_failed = 1;
		g_warning (_("Could not open target file %s"),target_file);
		return FALSE;

	}

        request = ghttp_request_new();
        if (!request) {
                g_warning (_("Could not create an http request !"));
                get_failed = 1;
        }

        if (ghttp_set_uri (request, url) != 0) {
                g_warning (_("Invalid uri !"));
                get_failed = 1;
        }

        ghttp_set_header (request, http_hdr_Connection, "close");
        ghttp_set_header (request, http_hdr_User_Agent, trilobite_get_useragent_string (FALSE, NULL));
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
			eazel_install_emit_download_progress (service, report, 0, total_bytes);
			first_emit = FALSE;
		}
		/* And that amount==0 & amount==total only occurs once */
		if (curStat.bytes_read!=0 && (curStat.bytes_read != curStat.bytes_total)) {
			eazel_install_emit_download_progress (service, 
							      report,
							      curStat.bytes_read, 
							      curStat.bytes_total);
		}
		g_main_iteration (FALSE);
        }
	/* Last emit amount==total */
	eazel_install_emit_download_progress (service, report, total_bytes, total_bytes);		

        if (ghttp_status_code (request) != 200) {
                g_warning (_("HTTP error: %d %s"), ghttp_status_code (request),
                         ghttp_reason_phrase (request));
                get_failed = 1;
        }
	if (ghttp_status_code (request) != 404) {
		length = ghttp_get_body_len (request);
		body = ghttp_get_body (request);
		if (body != NULL) {
			fwrite (body, length, 1, file);
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
        fclose (file);

        if (get_failed != 0) {
		return FALSE;
        }
	else {
		return TRUE;
	}
} /* end http_fetch_remote_file */

gboolean
ftp_fetch_remote_file (EazelInstall *service,
		       char *url, 
		       const char *file_to_report,
		       const char* target_file) 
{
	g_message (_("Downloading %s..."), url);
	g_warning (_("FTP not supported yet"));
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
	const char *file_to_report = cbstruct->file_to_report;

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
								      file_to_report ? file_to_report : info->source_name,
								      0,
								      info->file_size);
			} else if (!last_emit && info->bytes_copied == info->file_size) {
				last_emit = TRUE;
				eazel_install_emit_download_progress (service, 
								      file_to_report ? file_to_report : info->source_name,
								      info->file_size,
								      info->file_size);
			} else if (info->bytes_copied > 0) {
				eazel_install_emit_download_progress (service, 
								      file_to_report ? file_to_report : info->source_name,
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
								      file_to_report ? file_to_report : info->source_name,
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

static void 
free_string (char *str, gpointer unused) \
{
	g_free (str);
}

gboolean 
gnome_vfs_fetch_remote_file (EazelInstall *service, 
			     char *url, 
			     const char *file_to_report, 
			     const char *target_file)
{
	GnomeVFSResult result;
	GnomeVFSXferOptions xfer_options = 0;
	GnomeVFSURI *src_uri;
	GnomeVFSURI *dest_uri;
	
	GList *src_uri_list, *dest_uri_list;
	char *tmp;
	char *t_file;
	gnome_vfs_callback_struct *cbstruct;

	/* Ensure the target_file has a protocol://,
	   if not, prefix a file:// */
	if (strstr (target_file, "://")==NULL) {
		t_file = g_strdup_printf ("file://%s", target_file);
	} else {
		t_file = g_strdup (target_file);
	}

	trilobite_debug ("gnome_vfs_xfer_uri ( %s %s )", url, t_file);
	
	src_uri = gnome_vfs_uri_new (url);
	g_assert (src_uri != NULL);
	
	dest_uri = gnome_vfs_uri_new (t_file);
	g_assert (dest_uri != NULL);
	
	/* Setup the userdata for the callback, I need both the
	   service object to emit signals too, and the filename to report */
	cbstruct = g_new0 (gnome_vfs_callback_struct, 1);
	cbstruct->service = service;
	cbstruct->file_to_report = file_to_report;

	/* Execute the gnome_vfs copy */
	result = gnome_vfs_xfer_uri (src_uri, dest_uri,
				     xfer_options,
				     GNOME_VFS_XFER_ERROR_MODE_QUERY,
				     GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
				     (GnomeVFSXferProgressCallback)gnome_vfs_xfer_callback,
				     cbstruct);

	if (result==GNOME_VFS_OK) {
		g_message ("File download successfull");
	} else {
		g_message ("File download failed");
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
			 const char* target_file) 
{
	gboolean result;
	const char *report;

	report = file_to_report ? file_to_report : g_basename (target_file);
	g_message (_("Checking local file %s..."), target_file);
	result = FALSE;
	if (access (target_file, R_OK|W_OK) == 0) {
		struct stat sbuf;
		stat (target_file, &sbuf);
		/* Emit bogus download progress */
		eazel_install_emit_download_progress (service, report,            0, sbuf.st_size);
		eazel_install_emit_download_progress (service, report, sbuf.st_size, sbuf.st_size);
		result = TRUE;
	} 
	return result;
}

eazel_install_file_fetch_function*
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

gboolean
eazel_install_fetch_file (EazelInstall *service,
			  char *url, 
			  const char *file_to_report,
			  const char* target_file) 
{
	gboolean result;

	static eazel_install_file_fetch_function *func_table = NULL;
	
	if (!func_table) {
		func_table = eazel_install_fill_file_fetch_table ();
	}
	
	result = FALSE;

	g_return_val_if_fail (url!=NULL, FALSE);
	g_return_val_if_fail (target_file!=NULL, FALSE);

	if (g_file_test (target_file, G_FILE_TEST_ISFILE)) {
		g_message ("%s already present, not downloading", target_file);
		result = TRUE;
	} else {
		result = (func_table [eazel_install_get_protocol (service)])((gpointer)service, 
									     url, 
									     file_to_report, 
									     target_file);
	}

	/* By always adding the file to the downloaded_files list,
	   we enforce md5 check on files that were present but should have
	   been downloaded */
	if (result) {
		service->private->downloaded_files = g_list_prepend (service->private->downloaded_files,
								     g_strdup (target_file));
	}
	
	if (result==FALSE) {
		g_warning (_("Failed to retreive %s!"), 
			   file_to_report ? file_to_report : g_basename (target_file));
		eazel_install_emit_download_failed (service, 
						    file_to_report ? file_to_report : g_basename (target_file));
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
	gboolean result;
	char* url;
	char *md5 = NULL;
	char* targetname;

	result = FALSE;
	url = NULL;
	
	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_FTP:
	case PROTOCOL_HTTP: 
	{
		url = get_url_for_package (service, RPMSEARCH_ENTRY_NAME, package, package);
	}
	break;
	case PROTOCOL_LOCAL:
		url = g_strdup_printf ("%s", rpmfilename_from_packagedata (package));
		break;
	};

	if (url == NULL) {
		g_warning (_("Could not get a URL for %s"), rpmfilename_from_packagedata (package));
	} else {
		targetname = g_strdup_printf ("%s/%s",
					      eazel_install_get_tmp_dir (service),
					      filename_from_url (url));
		trilobite_debug ("%s resolved", package->name);
#ifndef EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI
		result = eazel_install_fetch_file (service, url, package->name, targetname);
#else /*  EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI */
		if (filename_from_url (url) && strlen (filename_from_url (url))>1) {
			result = eazel_install_fetch_file (service, url, package->name, targetname);
		} else {
			trilobite_debug ("cannot handle %s", url);
		}
#endif /* EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI */
		if (result==TRUE) {
			packagedata_fill_from_file (package, targetname); 
		} else {
			g_warning (_("File download failed"));
		}

		g_free (targetname);
	}
	
	g_free (url);
	return result;
}

gboolean eazel_install_fetch_package_which_provides (EazelInstall *service,
						     const char *file,
						     PackageData *package)
{
	gboolean result;
	char *url;
	char *targetname;

	g_assert (package != NULL);

	result = FALSE;

	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_FTP:
	case PROTOCOL_HTTP: 
	{
		url = get_url_for_package (service, RPMSEARCH_ENTRY_PROVIDES, (const gpointer)file, package);
	}
	break;
	case PROTOCOL_LOCAL:
		g_warning (_("Using local protocol cannot resolve library dependencies"));
		url = NULL;
		break;
	};

	if (url == NULL) {
		g_warning (_("Could not get a URL for %s"), file);
	} else {
		/* FIXME bugzilla.eazel.com 1315:
		   Loose the check once a proper rpmsearch.cgi is up and running */
		if (filename_from_url (url) && strlen (filename_from_url (url))>1) {
			targetname = g_strdup_printf ("%s/%s",
						      eazel_install_get_tmp_dir (service),
						      filename_from_url (url));
			result = eazel_install_fetch_file (service, url, NULL, targetname);
			if (result) {
				packagedata_fill_from_file (package, targetname);
			} else {
				package->status = PACKAGE_DEPENDENCY_FAIL;
				g_warning (_("File download failed"));
			}
			g_free (targetname);
		}
		g_free (url);
	}

	return result;
}


static void
add_to_url (char **url,
	    const char *cgi_string,
	    const char *val)
{
	char *tmp;

	g_assert (url != NULL);
	g_assert ((*url) != NULL);
	g_assert (cgi_string != NULL);

	if (val) {
		tmp = g_strconcat (*url, cgi_string, val, NULL);
		g_free (*url);
		(*url) = tmp;
	}
}

char*
get_url_for_package  (EazelInstall *service, 
		      RpmSearchEntry entry,
		      gpointer data,
		      PackageData *out_package)		      
{
	char *search_url = NULL;
	char *url = NULL;
	char *body = NULL;
	int length;

	search_url = get_search_url_for_package (service, entry, data);
	trilobite_debug ("Search URL: %s", search_url);

	{
		/* NOTE: uses snprintf, as this memory ends up somewhere in libc */
		char *tmp;
		int len;

		len = 28+strlen (trilobite_get_useragent_string (FALSE, NULL));
		tmp = (char*)malloc (len);
		snprintf (tmp, len-1, "GNOME_VFS_HTTP_USER_AGENT=%s", 
			  trilobite_get_useragent_string (FALSE, NULL));
		putenv (tmp);
		/* NOTE: tmp is now owned by env, wonder if it's leaked everytime
		   I set it...
		   FIXME: bugzilla.eazel.com  */
	}

	if (trilobite_fetch_uri (search_url, &body, &length)) {
#ifndef EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI
		/* Parse the returned xml */
		GList *packages;
		
		packages = parse_osd_xml_from_memory (body, length);
		if (g_list_length (packages) == 0) {
			trilobite_debug ("No url for file");
		} else if (g_list_length (packages) > 1) {
			trilobite_debug ("Ugh, more then one match, using first");
		}
		
		if (g_list_length (packages) > 0) {
			/* Get the first package returned */
			PackageData *pack;
			
			g_assert (packages->data != NULL);
			pack = (PackageData*)packages->data;
			out_package->filename = g_strdup (pack->filename);
			url = g_strdup (pack->filename);
			out_package->md5 = g_strdup (pack->md5);
			
			g_list_foreach (packages, 
					(GFunc)packagedata_destroy, 
					GINT_TO_POINTER (TRUE));
		}						
#else /* EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI */
		if (body) {
			body[length] = 0;
			url = g_strdup (body);
		}
#endif /* EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI */
		g_free (body);				
	} else {
		switch (entry) {
		case RPMSEARCH_ENTRY_NAME:
			g_warning (_("Could not retrieve a URL for %s"), 
				   rpmfilename_from_packagedata ((PackageData*)data));
			break;
		case RPMSEARCH_ENTRY_PROVIDES:
			g_warning (_("Could not retrieve a URL for %s"),
				   (char*)data);
			break;
		}
	}
	
	g_free (search_url);
	return url;
}


char* get_search_url_for_package (EazelInstall *service, 
				  RpmSearchEntry entry,
				  const gpointer data)
{
	char *url;
	DistributionInfo dist;
	url = g_strdup_printf (CGI_BASE,
			       eazel_install_get_server (service),
			       eazel_install_get_server_port (service));

	dist = trilobite_get_distribution ();

	switch (entry) {
	case RPMSEARCH_ENTRY_NAME: {
		PackageData *pack;

		pack = (PackageData*)data;
		add_to_url (&url, "?name=", pack->name);
		add_to_url (&url, "&arch=", pack->archtype);
		add_to_url (&url, "&version=", pack->version);
		if (pack->distribution.name != DISTRO_UNKNOWN) {
			dist = pack->distribution;
		} 
	}
	break;
	case RPMSEARCH_ENTRY_PROVIDES: {
		struct utsname buf;
		uname (&buf);
		add_to_url (&url, "?provides=", (char*)data);
		add_to_url (&url, "&arch=", buf.machine);
	}
	break;
	}

#ifndef EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI
	if (dist.name != DISTRO_UNKNOWN) {
		char *distro;
		distro = g_strdup_printf ("%s", 
					  trilobite_get_distribution_name (dist, TRUE, TRUE));
		add_to_url (&url, "&distro=", distro);
		g_free (distro);
	}
#endif /* EAZEL_INSTALL_PROTOCOL_USE_OLD_CGI */

	add_to_url (&url, "&protocol=", protocol_as_string (eazel_install_get_protocol (service)));

	return url;
}
