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
#include "eazel-softcat.h"

#include "eazel-install-xml-package-list.h"
#include "eazel-softcat-private.h"
#include <eel/eel-glib-extensions.h>
#include <libtrilobite/trilobite-core-network.h>
#include <libtrilobite/trilobite-core-utils.h>

/* used for gnome_vfs_escape_string */
#ifndef EAZEL_INSTALL_SLIM
#include <libgnomevfs/gnome-vfs.h>
#endif /* EAZEL_INSTALL_SLIM */

#include <string.h>

/* This is the parent class pointer */
static GtkObjectClass *eazel_softcat_parent_class;

#undef EAZEL_SOFTCAT_SPAM_XML

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

	g_free (softcat->private->server);
	softcat->private->server = NULL;
	g_free (softcat->private->server_str);
	softcat->private->server_str = NULL;
	g_free (softcat->private->cgi_path);
	softcat->private->cgi_path = NULL;
	g_free (softcat->private->username);
	softcat->private->username = NULL;
	g_free (softcat->private->db_revision);
	softcat->private->db_revision = NULL;

	g_free (softcat->private);

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
eazel_softcat_initialize (EazelSoftCat *softcat)
{
	g_assert (softcat != NULL);
	g_assert (EAZEL_IS_SOFTCAT (softcat));

	softcat->private = g_new0 (EazelSoftCatPrivate, 1);
	softcat->private->retries = 3;
	softcat->private->delay = 100;
	softcat->private->db_revision = NULL;
	softcat->private->packages_per_query = 1;
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
eazel_softcat_set_packages_per_query (EazelSoftCat *softcat, int number)
{
	softcat->private->packages_per_query = number;
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

void 
eazel_softcat_reset_server_update_flag (EazelSoftCat *softcat)
{
	g_free (softcat->private->db_revision);
	softcat->private->db_revision = NULL;
}

const char *
eazel_softcat_error_string (EazelSoftCatError err)
{
	/* No need to translate these strings, since this is only used
	 * in test code.
	 */
	switch (err) {
	case EAZEL_SOFTCAT_SUCCESS:
		return "(no error)";
	case EAZEL_SOFTCAT_ERROR_BAD_MOJO:
		return "internal error";
	case EAZEL_SOFTCAT_ERROR_SERVER_UNREACHABLE:
		return "softcat server is unreachable";
	case EAZEL_SOFTCAT_ERROR_MULTIPLE_RESPONSES:
		return "softcat server returned multiple responses to a single-package query";
	case EAZEL_SOFTCAT_ERROR_SERVER_UPDATED:
		return "softcat has been updated since last request";
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

/* return a softcat query URL that would find this package: either by name, by eazel-id, or by what it features */
static char *
get_search_url_for_package (EazelSoftCat *softcat, GList *packages, int sense_flags)
{
	GString *url;
	TrilobiteDistributionInfo dist;
	char *arch;
	char *dist_name;
	char *url_str;
	PackageData *package;

	g_assert (packages != NULL);
	package = PACKAGEDATA (packages->data);

	/* bail out early if there's not enough info to go with */
	if ((package->eazel_id == NULL) && (package->suite_id == NULL) && (package->name == NULL) &&
	    (package->features == NULL)) {
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
		/* More than one package, add the remained to the url */
		if (g_list_length (packages) >= 2) {
			GList *iterator;
			for (iterator = g_list_nth (packages, 1); iterator; iterator = g_list_next (iterator)) {
				PackageData *next_package = PACKAGEDATA (iterator->data);
				add_to_url (url, "&rpm_id=", next_package->eazel_id);
			}
		}
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
		/* find by features list! */
		g_assert ((package->features != NULL) && (g_list_length (package->features) > 0));
		arch = trilobite_get_distribution_arch ();
		add_to_url (url, "?provides=", (char *)(package->features->data));
		/* More than one package, add the remained to the url */
		if (g_list_length (packages) >= 2) {
			GList *iterator;
			for (iterator = g_list_nth (packages, 1); iterator; iterator = g_list_next (iterator)) {
				PackageData *next_package = PACKAGEDATA (iterator->data);
				add_to_url (url, "&provides=", next_package->features->data);
			}
		}
		add_to_url (url, "&arch=", arch);
		g_free (arch);
	} else {
		/* find by package name! */
		g_assert (package->name != NULL);
		add_to_url (url, "?name=", package->name);
		/* More than one package, add the remained to the url */
		if (g_list_length (packages) >= 2) {
			GList *iterator;
			for (iterator = g_list_nth (packages, 1); iterator; iterator = g_list_next (iterator)) {
				PackageData *next_package = PACKAGEDATA (iterator->data);
				add_to_url (url, "&name=", next_package->name);
			}
		}
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


/* directories will end with '/' */
/* TEMPORARY FIXME: to work around a very odd bug in softcat, throw away duplicate filenames */

static void
remove_directories_from_provides_list (PackageData *pack)
{
	GList *iter, *next_iter;
	GList *newlist;
	char *filename;

	newlist = NULL;
	for (iter = g_list_first (pack->provides); iter != NULL; ) {
		filename = (char *)(iter->data);

		if ((filename != NULL) && (filename[0] != '\0') &&
		    (filename[strlen (filename)-1] == '/')) {
			next_iter = iter->prev;
			g_free (iter->data);
			pack->provides = g_list_remove (pack->provides, iter->data);
			iter = next_iter;
			if (iter == NULL) {
				iter = g_list_first (pack->provides);
			}
		} else {
			if (g_list_find_custom (newlist, filename, (GCompareFunc)strcmp) == NULL) {
				newlist = g_list_prepend (newlist, g_strdup (filename));
			}
			iter = g_list_next (iter);
		}
	}

	/* replace old pack->provides with newlist */
	g_list_foreach (pack->provides, (GFunc)g_free, NULL);
	g_list_free (pack->provides);
	pack->provides = newlist;
}

/*
  This functions displays a "could not fetch" warning for the given packages
 */
static void
warn_about_packages_failing (EazelSoftCat *softcat, 
			     GList *packages)
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *package = PACKAGEDATA (iterator->data);
		if (package->eazel_id != NULL) {
			g_warning ("could not fetch info about package id %s", package->eazel_id);
		} else if (package->suite_id != NULL) {
			g_warning ("could not fetch info about suite id %s", package->suite_id);
		} else if (package->name != NULL) {
			g_warning ("could not fetch info about package '%s'", package->name);
		} else if ((package->features != NULL) && (package->features->data != NULL)) {
			g_warning ("could not fetch info about package that provides feature '%s'",
				   (char *)package->features->data);
		} else {
			g_warning ("could not fetch info about a MYSTERY PACKAGE!");
		}
	}
}

EazelSoftCatError
eazel_softcat_query (EazelSoftCat *softcat, GList *packages, int sense_flags, int fill_flags, GList **result)
{
	char *search_url;
	char *body = NULL;
	int length;
	int tries_left;
	gboolean got_happy;
	GList *result_packages = NULL;
	char *db_revision;
	int err;

	g_assert (result);
	g_assert (*result == NULL);

	db_revision = NULL;
	search_url = get_search_url_for_package (softcat, packages, sense_flags);
	if (search_url == NULL) {
		trilobite_debug ("no search url :(");
		return EAZEL_SOFTCAT_ERROR_BAD_MOJO;
	}
	trilobite_debug ("package search url: %s", search_url);

	eel_setenv ("GNOME_VFS_HTTP_USER_AGENT", trilobite_get_useragent_string (NULL), TRUE);

	for (got_happy = FALSE, tries_left = softcat->private->retries;
	     !got_happy && (tries_left > 0);
	     tries_left--) {

		got_happy = trilobite_fetch_uri (search_url, &body, &length);
		
#ifdef EAZEL_SOFTCAT_SPAM_XML
		{
			char **strs;
			int i;
			body [length] = 0;
			strs = g_strsplit (body, "\n", 0);
			for (i = 0; strs[i] != NULL; i++) {
				trilobite_debug ("xml spam: %s", strs[i]);
			}			
			g_strfreev (strs);
		}
#endif /* EAZEL_SOFTCAT_SPAM_XML */

		if (got_happy) {
			got_happy = eazel_install_packagelist_parse (&result_packages, body, length, &db_revision);
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
		warn_about_packages_failing (softcat, packages);
		g_free (search_url);
		return EAZEL_SOFTCAT_ERROR_SERVER_UNREACHABLE;
	}

	if ((db_revision != NULL) && (softcat->private->db_revision == NULL)) {
		softcat->private->db_revision = db_revision;
		db_revision = NULL;
	} else if (db_revision != NULL) {
		if (strcmp (softcat->private->db_revision, db_revision) != 0) {
			g_warning ("SoftCat has been updated since last request!");
			err = EAZEL_SOFTCAT_ERROR_SERVER_UPDATED;
			goto out;
		}
	}

	if (g_list_length (result_packages) == 0) {
		trilobite_debug ("no matches for that package.");
		err = EAZEL_SOFTCAT_ERROR_NO_SUCH_PACKAGE;
		goto out;
	}

	trilobite_debug ("package info ok.");
	{
		/* FIXME: forseti.eazel.com 1922
		   Hack to circumvent the abovementioned bug
		   Once fixed, remove this scope and replace with 
		   *result = result_packages;
		   */
		GList *iterator;
		for (iterator = result_packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *p = PACKAGEDATA (iterator->data);
			if (! (strlen (p->name)==0 &&
			       strlen (p->version)==0 &&
			       strlen (p->minor)==0 &&
			       strlen (p->md5)==0)) {
				(*result) = g_list_prepend ((*result), p);
			} else {
				gtk_object_unref (GTK_OBJECT (p));
			}
		}
	}
	
	err = EAZEL_SOFTCAT_SUCCESS;

out:
	g_free (body);
	g_free (search_url);
	g_free (db_revision);

	return err;
}

/* Given a partially filled packagedata object, 
   check softcat, and fill it with the desired info */
EazelSoftCatError
eazel_softcat_get_info (EazelSoftCat *softcat, PackageData *package, int sense_flags, int fill_flags)
{
	GList *result_packages = NULL;
	PackageData *full_package;
	GList *packages = NULL;
	EazelSoftCatError err;

	packages = g_list_prepend (packages, package);
	err = eazel_softcat_query (softcat, packages, sense_flags, fill_flags, &result_packages);
	g_list_free (packages);
	if (err != EAZEL_SOFTCAT_SUCCESS) {
		return err;
	}

	if (package->suite_id) {
		/* More than one package returned and we queried on a suite Id.
		   Make deps and put into "package", remember to strip dirs in 
		   provides if needed */
		GList *iterator;

		trilobite_debug ("softcat query returned suite with %d elements", 
				 g_list_length (result_packages));
		for (iterator = result_packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = PACKAGEDATA (iterator->data);
			PackageDependency *dep = packagedependency_new ();

			if (fill_flags & PACKAGE_FILL_NO_DIRS_IN_PROVIDES) {
				remove_directories_from_provides_list (pack);
			}
			gtk_object_ref (GTK_OBJECT (pack));
			pack->fillflag = fill_flags;

			dep->package = pack;
			dep->version = g_strdup (pack->version);

			if (dep->version) {
				/* FIXME: should a suite be EQ or GE ? If GE, any newer version
				   that's already installed will be ok, if EQ, the suites depends
				   on an exact version */
				dep->sense = EAZEL_SOFTCAT_SENSE_GE;
			} else {
				dep->sense = EAZEL_SOFTCAT_SENSE_ANY;
			}

			packagedata_add_pack_to_depends (package, dep);
		}
	} else if (g_list_length (packages) > 1) {
		g_warning ("softcat query returned %d results!", g_list_length (result_packages));
		err = EAZEL_SOFTCAT_ERROR_MULTIPLE_RESPONSES;
		g_list_foreach (result_packages, (GFunc)gtk_object_unref, NULL);
		g_list_free (result_packages);
		return err;
	} else {	/* 1 package, not a suite */
		full_package = PACKAGEDATA (result_packages->data);
		packagedata_fill_in_missing (package, full_package, fill_flags);
		if (fill_flags & PACKAGE_FILL_NO_DIRS_IN_PROVIDES) {
			remove_directories_from_provides_list (package);
		}
	}

	g_list_foreach (result_packages, (GFunc)gtk_object_unref, NULL);
	g_list_free (result_packages);
	return err;
}

/*
  Hold on to your brain...

  This takes as input a list of GList*<PackageData*> "packages", a
  pointer to a GList*<GList*> "massives" and a pointer to a
  GList*<PackageData*> "singles"

  It does its majick, and stuffs some lists into "massives". These
  lists are series of packages that can be queried for in one massive
  query.

  This can easily be extended to eg. handle feature (?provides=X)
  requests, it actually did, but I removed it as I really didn't feel
  like testing it.

  The rest go into "singles" 

  Current shortcoming : it uses the distribution/archtype of the first
  package from "packages" as base requirement for all the following
  packages.

*/

static void
split_by_multiple (EazelSoftCat *softcat, 
		   GList *packages,
		   GList **massives,
		   GList **singles)
{
	GList *iterator;
	TrilobiteDistributionInfo distinfo;
	const char *arch;
	GList *names = NULL, *ids = NULL;

	/* Use the distribution and arch of the first pacakge as common
	   demoninator. If a package differs from there, it won't
	   go into any massives list (not optimal, I know) */
	distinfo = PACKAGEDATA (packages->data)->distribution;
	arch = PACKAGEDATA (packages->data)->archtype;

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *p = PACKAGEDATA (iterator->data);
		gboolean result = TRUE;

		/* Is it the same architecture ? */
		if (p->archtype && arch && strcmp (p->archtype, arch)) {
			result = FALSE;
		}

		/* Same distribution ? */
		if (trilobite_distribution_compare (p->distribution, distinfo) != 0) {
			result = FALSE;
		}

		/* Does it have a version */
		if (p->version) {
			result = FALSE;
		}

		/* If package passed initial tests, 
		   add to appropriate list */
		if (result) {
			if (p->eazel_id) {
				ids = g_list_prepend (ids, p);
			} else if (p->name) {
				names = g_list_prepend (names, p);
			} else {
				/* Woops, add it to singles */
				(*singles) = g_list_prepend (*singles, p);
			}
		} else {
			(*singles) = g_list_prepend (*singles, p);
		}
		
	}
	
	/* Add lists to output massive list */
	if (ids) {
		(*massives) = g_list_prepend ((*massives), ids);
	}
	if (names) {
		(*massives) = g_list_prepend ((*massives), names);
	}
}

/* Helper for get_info

   If executes a query, and thereafter tries for match the resulting
   packages against ther results depending on what was set in the input
   packages.
*/
static EazelSoftCatError
eazel_softcat_get_info_plural_helper (EazelSoftCat *softcat, 
				      GList *packages, 
				      GList **out, GList **error,
				      int sense_flags, int fill_flags)
{
	GList *result_packages = NULL;
	EazelSoftCatError err;
	GList *a;

	err = eazel_softcat_query (softcat, packages, sense_flags, fill_flags, &result_packages);
	if (err != EAZEL_SOFTCAT_SUCCESS) {
		return err;
	}

	/* Now lets match against input packages */
	for (a = packages; a; a = g_list_next (a)) {
		PackageData *package = PACKAGEDATA (a->data);
		GList *full_package_list = NULL;
		PackageData *full_package;
		GCompareFunc compare_func = NULL;
		char *compare_with = NULL;

		/* Find appropriate comparison func */
		if (package->eazel_id) {
			compare_func = (GCompareFunc)eazel_install_package_id_compare;
			compare_with = package->eazel_id;
		} else if (package->features) {
			compare_func = (GCompareFunc)eazel_install_package_feature_compare;
			compare_with = (char*)package->features->data;
		} else if (package->name) {
			compare_func = (GCompareFunc)eazel_install_package_name_compare;
			compare_with = package->name;
		}

		/* If found, try and find a matching package */
		if (compare_func) {
			full_package_list = g_list_find_custom (result_packages, 
								(gpointer)compare_with, 
								compare_func);
		}
		
		/* If match was found, fill it input, otherwise add package to error list */
		if (full_package_list == NULL) {
			(*error) = g_list_prepend (*error, package);
		} else {
			full_package = PACKAGEDATA (full_package_list->data);			
			packagedata_fill_in_missing (package, full_package, fill_flags);
			if (fill_flags & PACKAGE_FILL_NO_DIRS_IN_PROVIDES) {
				remove_directories_from_provides_list (package);
			}
			(*out) = g_list_prepend (*out, package);
		}
	}

	g_list_foreach (result_packages, (GFunc)gtk_object_unref, NULL);
	g_list_free (result_packages);

	return err;
}

/* Given a partially filled packagedata object, check softcat, and
   fill it with the desired info. Output values will be in
   &GList*<PackageData*> out and &GList<PackageData*> error.  Packages
   in these two lists will be pointers into the original
   GList*<PackageData*> packages argument

   Heres the pseudokode :

   pakker i error & out peger ind i input. dem i out er ok, dem i error fejlede.

   Create two lists, GList*<GList*> "massives" and
   GList*<PackageData*>"singles". "massives" will hold lists of
   packages with same query signature (id or name).

   foreach L in massives {
        foreach subL in L (subL will be the "next" packages_per_query elements in L) {
	     (voodoo to maintain the lists)
	     do a get_info_plural_helper (L, out, error)
	     (voodoo to maintain the lists)
        }
   }
   foreach P in singles { 
     get_info (P)
   }

   So basically this function will accept any weird combo of crackass
   packages and try and make the minimal amount of queries (depending
   on packages_per_query)

*/

EazelSoftCatError
eazel_softcat_get_info_plural (EazelSoftCat *softcat, 
			       GList *packages, 
			       GList **out, GList **error,
			       int sense_flags, int fill_flags)
{
	EazelSoftCatError err = EAZEL_SOFTCAT_SUCCESS;
	GList *org_massive = NULL;
	GList *massive = NULL;
	GList *massives = NULL;
	GList *singles = NULL;
	GList *partial = NULL;

	split_by_multiple (softcat, packages, &massives, &singles);

	/* Get first list from massives */
	if (massives) {
		massive = g_list_first (massives)->data;
		org_massive = massive;
	}

	while (massives && massive) {
		int i;
		/* Only put softcat->private->packages_per_query into one query */
		for (i = 0; i < softcat->private->packages_per_query; i++) {
			gpointer p;

			/* Move head to partial */
			p = (g_list_first (massive)->data);
			partial = g_list_prepend (partial, p);
			massive = g_list_remove (massive, p);

			if (g_list_length (massive)==0) {
				break;
			}
		}
		/* Fire of query */
		if (eazel_softcat_get_info_plural_helper (softcat, partial, 
							  out, error, 
							  sense_flags, fill_flags) != EAZEL_SOFTCAT_SUCCESS) {
			err = EAZEL_SOFTCAT_ERROR_BAD_MOJO;
		}
		g_list_free (partial);
		partial = NULL;

		/* If we hit end of the massive list, step to next list */
		if (g_list_length (massive)==0) {
			/* Remove current from massives */
			massives = g_list_remove (massives, org_massive);

			/* Destroy current */
			g_list_free (massive);
			massive = NULL;

			if (g_list_length (massives) == 0) {
				/* Destroy the massives lists */
				g_list_free (massives);
				massives = NULL;
			} else {
				/* get next massive list */
				massive = g_list_first (massives)->data;
				org_massive = massive;
			}
		}

	}

	/* While there's elements in singles, get them */
	while (singles) {
		PackageData *p = PACKAGEDATA (singles->data);
		trilobite_debug ("Processing single %s", packagedata_get_readable_name (p));
		if (eazel_softcat_get_info (softcat, p, sense_flags, fill_flags) != EAZEL_SOFTCAT_SUCCESS) {
			err = EAZEL_SOFTCAT_ERROR_BAD_MOJO;
			(*error) = g_list_prepend ((*error), p);
		} else {
			(*out) = g_list_prepend ((*out), p);
		}
		singles = g_list_remove (singles, p);
	}

	g_list_free (singles);
/*
	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);
	g_list_free (packages);
*/
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
