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
 * Authors: Robey Pointer <robey@eazel.com>
 *	    Eskil Heyn Olsen <eskil@eazel.com>
 */

#include <config.h>
#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/trilobite-core-network.h>
#include "eazel-softcat.h"
#include "eazel-softcat-private.h"
#include "eazel-install-xml-package-list.h"

/* used for gnome_vfs_escape_string */
#ifndef EAZEL_INSTALL_SLIM
#include <libgnomevfs/gnome-vfs.h>
#endif /* EAZEL_INSTALL_SLIM */

/* This is the parent class pointer */
static GtkObjectClass *eazel_softcat_parent_class;


/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_softcat_finalize (GtkObject *object)
{
	EazelSoftCat *softcat;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_SOFTCAT (object));

	softcat = EAZEL_SOFTCAT (object);

	if (GTK_OBJECT_CLASS (eazel_softcat_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_softcat_parent_class)->finalize (object);
	}
}

static void
eazel_softcat_class_initialize (EazelSoftCatClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_softcat_finalize;
#if 0
	object_class->set_arg = eazel_softcat_set_arg;
#endif
	
	eazel_softcat_parent_class = gtk_type_class (gtk_object_get_type ());

#if 0
	signals[START] = 
		gtk_signal_new ("start",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, start),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);	
	signals[END] = 
		gtk_signal_new ("end",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, end),
				gtk_marshal_BOOL__POINTER_INT_INT,
				GTK_TYPE_BOOL, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[PROGRESS] = 
		gtk_signal_new ("progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, progress),
				eazel_softcat_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[FAILED] = 
		gtk_signal_new ("failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
#endif
}

static void
eazel_softcat_initialize (EazelSoftCat *softcat) {
	g_assert (softcat != NULL);
	g_assert (EAZEL_IS_SOFTCAT (softcat));

	softcat->private = g_new0 (EazelSoftCatPrivate, 1);
	softcat->private->retries = 3;
	softcat->private->delay = 100;
}

GtkType
eazel_softcat_get_type() {
	static GtkType softcat_type = 0;

	/* First time it's called ? */
	if (!softcat_type)
	{
		static const GtkTypeInfo softcat_info =
		{
			"EazelSoftCat",
			sizeof (EazelSoftCat),
			sizeof (EazelSoftCatClass),
			(GtkClassInitFunc) eazel_softcat_class_initialize,
			(GtkObjectInitFunc) eazel_softcat_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		softcat_type = gtk_type_unique (gtk_object_get_type (), &softcat_info);
	}

	return softcat_type;
}

EazelSoftCat *
eazel_softcat_new (void)
{
	EazelSoftCat *softcat;

	softcat = EAZEL_SOFTCAT (gtk_object_new (TYPE_EAZEL_SOFTCAT, NULL));
	gtk_object_ref (GTK_OBJECT (softcat));
	gtk_object_sink (GTK_OBJECT (softcat));
	
	return softcat;
}

void
eazel_softcat_unref (GtkObject *object)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (EAZEL_SOFTCAT (object));
        gtk_object_unref (object);
}

void
eazel_softcat_set_server (EazelSoftCat *softcat, const char *server)
{
	char *p;

	g_free (softcat->private->server);
	softcat->private->server = g_strdup (server);

	p = strchr (softcat->private->server, ':');
	if (p != NULL) {
		softcat->private->port = atoi (p+1);
		*p = '\0';
	} else {
		softcat->private->port = SOFTCAT_DEFAULT_PORT;
	}

	g_free (softcat->private->server_str);
	softcat->private->server_str = g_strdup_printf ("%s:%d", softcat->private->server, softcat->private->port);
        trilobite_debug ("SOFTCAT host/port: %s", server);
}

void
eazel_softcat_set_server_host (EazelSoftCat *softcat, const char *server)
{
        g_free (softcat->private->server);
        softcat->private->server = g_strdup (server);
        trilobite_debug ("SOFTCAT host: %s", server);
}

void
eazel_softcat_set_server_port (EazelSoftCat *softcat, int port)
{
        softcat->private->port = port;
        trilobite_debug ("SOFTCAT port: %d", port);
}

const char *
eazel_softcat_get_server (EazelSoftCat *softcat)
{
	if (softcat->private->server_str == NULL) {
		softcat->private->server_str = g_strdup_printf ("%s:%d", SOFTCAT_DEFAULT_SERVER, SOFTCAT_DEFAULT_PORT);
	}
	return softcat->private->server_str;
}

const char *
eazel_softcat_get_server_host (EazelSoftCat *softcat)
{
        return softcat->private->server;
}

int
eazel_softcat_get_server_port (EazelSoftCat *softcat)
{
        return softcat->private->port;
}

void
eazel_softcat_set_cgi_path (EazelSoftCat *softcat, const char *cgi_path)
{
	g_free (softcat->private->cgi_path);
	if (cgi_path == NULL) {
		softcat->private->cgi_path = NULL;
	} else {
		softcat->private->cgi_path = g_strdup (cgi_path);
	}
}

const char *
eazel_softcat_get_cgi_path (const EazelSoftCat *softcat)
{
	return (softcat->private->cgi_path != NULL) ? softcat->private->cgi_path : SOFTCAT_DEFAULT_CGI_PATH;
}

void
eazel_softcat_set_authn (EazelSoftCat *softcat, gboolean use_authn, const char *username)
{
	g_free (softcat->private->username);
	softcat->private->use_authn = use_authn;
	if (username == NULL) {
		softcat->private->username = NULL;
	} else {
		softcat->private->username = g_strdup (username);
	}
}

void
eazel_softcat_set_authn_flag (EazelSoftCat *softcat, gboolean use_authn)
{
        softcat->private->use_authn = use_authn;
}

void
eazel_softcat_set_username (EazelSoftCat *softcat, const char *username)
{
        eazel_softcat_set_authn (softcat, softcat->private->use_authn, username);
}

gboolean
eazel_softcat_get_authn (const EazelSoftCat *softcat, const char **username)
{
	if (username != NULL) {
		*username = softcat->private->username;
	}
	return softcat->private->use_authn;
}

void
eazel_softcat_set_retry (EazelSoftCat *softcat, unsigned int retries, unsigned int delay_us)
{
	if (retries == 0) {
		retries = 1;
	}
	softcat->private->retries = retries;
	softcat->private->delay = delay_us;
}

const char *
eazel_softcat_error_string (EazelSoftCatError err)
{
	switch (err) {
	case EAZEL_SOFTCAT_SUCCESS:
		return "(no error)";
	case EAZEL_SOFTCAT_ERROR_BAD_MOJO:
		return "internal error";
	case EAZEL_SOFTCAT_ERROR_SERVER_UNREACHABLE:
		return "softcat server is unreachable";
	case EAZEL_SOFTCAT_ERROR_MULTIPLE_RESPONSES:
		return "softcat server returned multiple responses to a single-package query";
	case EAZEL_SOFTCAT_ERROR_NO_SUCH_PACKAGE:
		return "no such package";
	}
	return "???";
}


/*****************************************
  actual real implementation stuff
*****************************************/

/* can be OR'd together for "greater than or equal" etc -- this happens often. */
/* --- private to me.  everyone else should use the real sense flags in eazel-softcat.h. */
/* these are the numbers that the softcat server uses. */
typedef enum {
	SOFTCAT_SENSE_FLAG_LESS = 2,
	SOFTCAT_SENSE_FLAG_GREATER = 4,
	SOFTCAT_SENSE_FLAG_EQUAL = 8
} SoftcatSenseFlag;

static char *
sense_flags_to_softcat_flags (EazelSoftCatSense sense)
{
	int flags = 0;

	if (sense & EAZEL_SOFTCAT_SENSE_EQ) {
		flags |= SOFTCAT_SENSE_FLAG_EQUAL;
	}
	if (sense & EAZEL_SOFTCAT_SENSE_GT) {
		flags |= SOFTCAT_SENSE_FLAG_GREATER;
	}
	if (sense & EAZEL_SOFTCAT_SENSE_LT) {
		flags |= SOFTCAT_SENSE_FLAG_LESS;
	}

	return g_strdup_printf ("%d", flags);
}

EazelSoftCatSense
eazel_softcat_convert_sense_flags (int flags)
{
	EazelSoftCatSense out = 0;

	if (flags & SOFTCAT_SENSE_FLAG_LESS) {
		out |= EAZEL_SOFTCAT_SENSE_LT;
	}
	if (flags & SOFTCAT_SENSE_FLAG_GREATER) {
		out |= EAZEL_SOFTCAT_SENSE_GT;
	}
	if (flags & SOFTCAT_SENSE_FLAG_EQUAL) {
		out |= EAZEL_SOFTCAT_SENSE_EQ;
	}
	return out;
}

char *
eazel_softcat_sense_flags_to_string (EazelSoftCatSense flags)
{
	char *out, *p;

	out = g_malloc (5);
	p = out;
	if (flags & EAZEL_SOFTCAT_SENSE_LT) {
		*p++ = '<';
	}
	if (flags & EAZEL_SOFTCAT_SENSE_GT) {
		*p++ = '>';
	}
	if (flags & EAZEL_SOFTCAT_SENSE_EQ) {
		*p++ = '=';
	}
	*p = '\0';
	return out;
}

EazelSoftCatSense
eazel_softcat_string_to_sense_flags (const char *str)
{
	EazelSoftCatSense out = 0;
	const char *p;

	for (p = str; *p != '\0'; p++) {
		switch (*p) {
		case '<':
			out |= EAZEL_SOFTCAT_SENSE_LT;
			break;
		case '>':
			out |= EAZEL_SOFTCAT_SENSE_GT;
			break;
		case '=':
			out |= EAZEL_SOFTCAT_SENSE_EQ;
			break;
		default:
			/* ignore */
			break;
		}
	}
	return out;
}

#ifdef EAZEL_INSTALL_SLIM
/* wow, i had no idea all these chars were evil.  they must be stopped! */
static char _bad[] = {
        1,0,1,1,1,1,1,1,0,0,0,1,1,0,0,1,	/*  !"#$%&'()*+,-./ */
        0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,	/* 0123456789:;<=>? */
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* @ABCDEFGHIJKLMNO */
        0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,	/* PQRSTUVWXYZ[\]^_ */
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* `abcdefghijklmno */
        0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,1		/* pqrstuvwxyz{|}~del */
};
#define EVILCHAR(c)	(((c) < 0x20) || ((c) > 0x7F) || (_bad[c-0x20]))

static char *
gnome_vfs_escape_string (const char *in)
{
	int needs_quoting = 0;
	const char *p;
	char *quoted, *q;

	for (p = in; p && *p; p++) {
		if (EVILCHAR ((unsigned char)*p)) {
			needs_quoting++;
		}
	}
	if (! needs_quoting) {
		return g_strdup (in);
	}

	q = quoted = g_malloc (strlen (in) + (needs_quoting * 2) + 1);
	for (p = in; p && *p; p++) {
		if (EVILCHAR ((unsigned char)*p)) {
			*q++ = '%';
			*q++ = "0123456789ABCDEF"[(unsigned char)*p / 16];
			*q++ = "0123456789ABCDEF"[(unsigned char)*p % 16];
		} else {
			*q++ = *p;
		}
	}
        *q = '\0';

	return quoted;
}
#endif	/* EAZEL_INSTALL_SLIM */

static void
add_to_url (GString *url, const char *cgi_string, const char *val)
{
	char *quoted_val;

	g_string_append (url, cgi_string);
	if (val) {
		quoted_val = gnome_vfs_escape_string (val);
		g_string_append (url, quoted_val);
		g_free (quoted_val);
	}
}

/* make sure there are items filled in for the required fields.
 * if anything is missing, fill it with a "default", which may or may not be what you want,
 * but since you didn't bother to specify, tough cookies.
 */
static void
verify_softcat_fields (EazelSoftCat *softcat)
{
	if (softcat->private->server == NULL) {
		softcat->private->server = g_strdup (SOFTCAT_DEFAULT_SERVER);
	}
	if (softcat->private->port == 0) {
		softcat->private->port = SOFTCAT_DEFAULT_PORT;
	}
	if (softcat->private->cgi_path == NULL) {
		softcat->private->cgi_path = g_strdup (SOFTCAT_DEFAULT_CGI_PATH);
	}
}

/* return a softcat query URL that would find this package: either by name, by eazel-id, or by what it provides */
static char *
get_search_url_for_package (EazelSoftCat *softcat, const PackageData *package, int sense_flags)
{
	GString *url;
	DistributionInfo dist;
	char *arch;
	char *dist_name;
	char *url_str;

	g_assert (package != NULL);

	/* bail out early if there's not enough info to go with */
	if ((package->eazel_id == NULL) && (package->suite_id == NULL) && (package->name == NULL) &&
	    (package->provides == NULL)) {
		trilobite_debug ("softcat: no search url for completely-empty package");
		return NULL;
	}

	verify_softcat_fields (softcat);
	dist = trilobite_get_distribution ();

	url = g_string_new ("");
	if (softcat->private->use_authn) {
		if (softcat->private->username != NULL) {
			g_string_sprintfa (url, "eazel-services://%s%s", softcat->private->username,
					   softcat->private->cgi_path);
		} else {
			g_string_sprintfa (url, "eazel-services:%s", softcat->private->cgi_path);
		}
	} else {
		g_string_sprintfa (url, "http://%s:%d%s",
				   softcat->private->server,
				   softcat->private->port,
				   softcat->private->cgi_path);
	}

	if (package->eazel_id != NULL) {
		/* find by eazel-id! */
		arch = trilobite_get_distribution_arch ();
		add_to_url (url, "?rpm_id=", package->eazel_id);
		add_to_url (url, "&arch=", arch);
		g_free (arch);
	} else if (package->suite_id != NULL) {
		/* find by suite-id! */
		/* this devolves into several different cases.  softcat cares
		 * about the differences between them, but we don't.
		 */
		if (package->suite_id[0] == 'P') {
			add_to_url (url, "?product_id=", package->suite_id+2);
		} else if (package->suite_id[0] == 'S') {
			add_to_url (url, "?suite_id=", package->suite_id+2);
		} else if (package->suite_id[0] == 'N') {
			add_to_url (url, "?product_name=", package->suite_id+2);
		} else if (package->suite_id[0] == 'X') {
			add_to_url (url, "?suite_name=", package->suite_id+2);
		} else {
			g_assert_not_reached ();
		}
		arch = trilobite_get_distribution_arch ();
		add_to_url (url, "&arch=", arch);
		g_free (arch);
	} else if (package->name == NULL) {
		/* find by provides list! */
		g_assert ((package->provides != NULL) && (g_list_length (package->provides) > 0));
		arch = trilobite_get_distribution_arch ();
		add_to_url (url, "?provides=", (char *)(package->provides->data));
		add_to_url (url, "&arch=", arch);
		g_free (arch);
	} else {
		/* find by package name! */
		g_assert (package->name != NULL);
		add_to_url (url, "?name=", package->name);
		if (package->archtype != NULL) {
			add_to_url (url, "&arch=", package->archtype);
		}
		if (package->version != NULL) {
			add_to_url (url, "&version=", package->version);
			add_to_url (url, "&flags=", sense_flags_to_softcat_flags (sense_flags));
		}
		if (package->distribution.name != DISTRO_UNKNOWN) {
			dist = package->distribution;
		}
	}

	if (dist.name != DISTRO_UNKNOWN) {
		dist_name = trilobite_get_distribution_name (dist, TRUE, TRUE);
		add_to_url (url, "&distro=", dist_name);
		g_free (dist_name);
	}
	/* FIXME: should let them specify a protocol other than http, someday */
	add_to_url (url, "&protocol=", "http");

	url_str = url->str;
	g_string_free (url, FALSE);
	return url_str;
}


static gboolean
is_filename_probably_a_directory (const char *filename, const GList *provides)
{
	const GList *iter;
	gboolean is_dir = FALSE;

	for (iter = g_list_first ((GList *)provides); iter != NULL; iter = g_list_next (iter)) {
		const char *filename2 = (const char *)(iter->data);
		/* substring match is not sufficient (think of "libfoo.so.0" & "libfoo.so.0.0") */
		if ((strlen (filename2) > strlen (filename)) &&
		    (strncmp (filename, filename2, strlen (filename)) == 0) &&
		    (filename2[strlen (filename)] == '/')) {
			is_dir = TRUE;
			break;
		}
	}
	return is_dir;
}

/* this doesn't completely workaround forseti bug 1279 :( */
static void
remove_directories_from_provides_list (PackageData *pack)
{
	GList *iter, *next_iter;

	for (iter = g_list_first (pack->provides); iter != NULL; ) {
		if (is_filename_probably_a_directory ((char *)(iter->data), pack->provides)) {
			next_iter = iter->prev;

			g_free (iter->data);
			pack->provides = g_list_remove (pack->provides, iter->data);
			iter = next_iter;
			if (iter == NULL) {
				iter = g_list_first (pack->provides);
			}
		} else {
			iter = g_list_next (iter);
		}
	}
}

EazelSoftCatError
eazel_softcat_query (EazelSoftCat *softcat, PackageData *package, int sense_flags, int fill_flags, GList **result)
{
	char *search_url;
	char *body = NULL;
	int length;
	int tries_left;
	gboolean got_happy;
	GList *packages;
	int err;

	search_url = get_search_url_for_package (softcat, package, sense_flags);
	if (search_url == NULL) {
		trilobite_debug ("no search url :(");
		return EAZEL_SOFTCAT_ERROR_BAD_MOJO;
	}
	trilobite_debug ("package search url: %s", search_url);

	trilobite_setenv ("GNOME_VFS_HTTP_USER_AGENT", trilobite_get_useragent_string (NULL), TRUE);

	for (got_happy = FALSE, tries_left = softcat->private->retries;
	     !got_happy && (tries_left > 0);
	     tries_left--) {
		got_happy = trilobite_fetch_uri (search_url, &body, &length);
		if (got_happy) {
			got_happy = eazel_install_packagelist_parse (&packages, body, length);
			if (! got_happy) {
				/* boo.  bogus xml.  long live softcat! */
				trilobite_debug ("bogus xml.");
				g_free (body);
			}
		}

		if (! got_happy && (tries_left > 1)) {
			trilobite_debug ("retry...");
			usleep (softcat->private->delay);
		}
	}

	if (! got_happy) {
		if (package->eazel_id != NULL) {
			g_warning ("couldn't fetch info about package id %s", package->eazel_id);
		} else if (package->suite_id != NULL) {
			g_warning ("couldn't fetch info about suite id %s", package->suite_id);
		} else if (package->name != NULL) {
			g_warning ("couldn't fetch info about package '%s'", package->name);
		} else if ((package->provides != NULL) && (package->provides->data != NULL)) {
			g_warning ("couldn't fetch info about package that provides '%s'",
				   (char *)package->provides->data);
		} else {
			g_warning ("couldn't fetch info about a MYSTERY PACKAGE!");
		}
		g_free (search_url);
		return EAZEL_SOFTCAT_ERROR_SERVER_UNREACHABLE;
	}

	if (g_list_length (packages) == 0) {
		trilobite_debug ("no matches for that package.");
		err = EAZEL_SOFTCAT_ERROR_NO_SUCH_PACKAGE;
		goto out;
	}

	trilobite_debug ("package info ok.");
	*result = packages;
	err = EAZEL_SOFTCAT_SUCCESS;

out:
	g_free (body);
	g_free (search_url);

	return err;
}

/* Given a partially filled packagedata object, 
   check softcat, and fill it with the desired info */
EazelSoftCatError
eazel_softcat_get_info (EazelSoftCat *softcat, PackageData *package, int sense_flags, int fill_flags)
{
	GList *packages;
	PackageData *full_package;
	EazelSoftCatError err;

	err = eazel_softcat_query (softcat, package, sense_flags, fill_flags, &packages);
	if (err != EAZEL_SOFTCAT_SUCCESS) {
		return err;
	}

	if (g_list_length (packages) > 1) {
		g_warning ("softcat query returned %d results!", g_list_length (packages));
		g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);
		g_list_free (packages);
		err = EAZEL_SOFTCAT_ERROR_MULTIPLE_RESPONSES;
		return err;
	}

	full_package = PACKAGEDATA (packages->data);
	packagedata_fill_in_missing (package, full_package, fill_flags);
	remove_directories_from_provides_list (package);
	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);
	g_list_free (packages);
	return err;
}

/* Check if there's a newer version in SoftCat.
 * Returns TRUE and fills in 'newpack' if there is, returns FALSE otherwise.
 */
gboolean
eazel_softcat_available_update (EazelSoftCat *softcat, 
				PackageData *oldpack, 
				PackageData **newpack, 
				int fill_flags)
{
	PackageData *tmp_pack;
	gboolean result = TRUE;

	tmp_pack = packagedata_new ();
	tmp_pack->name = g_strdup (oldpack->name);
	tmp_pack->version = g_strdup (oldpack->version);
	tmp_pack->distribution = oldpack->distribution;
	tmp_pack->archtype = g_strdup (oldpack->archtype);

	if (eazel_softcat_get_info (softcat, tmp_pack, EAZEL_SOFTCAT_SENSE_GT, fill_flags) != EAZEL_SOFTCAT_SUCCESS) {
		result = FALSE;
	}

	if (newpack!=NULL && result==TRUE) {
		(*newpack) = tmp_pack;
	} else {
		gtk_object_unref (GTK_OBJECT (tmp_pack));
		/* Null in case it's givin */
		if (newpack != NULL) {
			(*newpack) = NULL;
		}
	}

	return result;
}
