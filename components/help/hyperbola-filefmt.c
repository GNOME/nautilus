/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Copyright (C) 2000 Sun Microsystems, Inc. 
 * Copyright (C) 2001 Eazel, Inc. 
 *
 * This module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public 
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this module; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <config.h>
#include "hyperbola-filefmt.h"

#include <ctype.h>
#include <dirent.h>
#include <eel/eel-glib-extensions.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <limits.h>
#include <parser.h>
#include <regex.h>
#include <string.h>
#include <tree.h>
#include <unistd.h>
#include <xmlmemory.h>
#include <zlib.h>

typedef struct {
	const char *name;
	void (*populate_tree) (HyperbolaDocTree * tree);
} FormatHandler;

static void fmt_man_populate_tree (HyperbolaDocTree * tree);

static void fmt_info_populate_tree (HyperbolaDocTree * tree);

#ifndef ENABLE_SCROLLKEEPER_SUPPORT
static void fmt_help_populate_tree (HyperbolaDocTree * tree);
#endif

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
static int fmt_toplevel_populate_tree (HyperbolaDocTree * tree);
static void fmt_scrollkeeper_populate_tree (HyperbolaDocTree * tree);
static xmlDocPtr fmt_scrollkeeper_get_xml_tree_of_locale (char *locale);
#endif

static void make_treesection (HyperbolaDocTree * tree, char **path);

static FormatHandler format_handlers[] = {
#ifndef ENABLE_SCROLLKEEPER_SUPPORT
	{"help", fmt_help_populate_tree},
#endif
	{"man", fmt_man_populate_tree},
	{"info", fmt_info_populate_tree},
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	{"xml", fmt_scrollkeeper_populate_tree},
#endif
	{NULL, NULL}
};

static gboolean
tree_node_destroy (gpointer key, gpointer data, gpointer user_data)
{
	HyperbolaTreeNode *node = data;

	g_free (node->title);
	g_free (node->uri);

	if (node->children) {
		g_tree_traverse (node->children, tree_node_destroy,
				 G_IN_ORDER, NULL);
		g_tree_destroy (node->children);
	}

	return FALSE;
}

static gint
tree_key_compare (gconstpointer k1, gconstpointer k2)
{
	return g_strcasecmp (k1, k2);
}

HyperbolaDocTree *
hyperbola_doc_tree_new (void)
{
	HyperbolaDocTree *retval = g_new0 (HyperbolaDocTree, 1);

	retval->global_by_uri = g_hash_table_new (g_str_hash, g_str_equal);
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	/* The all_index_files hash table stores the index file names (incl.
	 * absolute path). The key to the hash table is the uri for the
	 * corresponding  document.
	 */ 
	retval->all_index_files = g_hash_table_new (g_str_hash, g_str_equal);
#endif
	retval->children = g_tree_new (tree_key_compare);
	return retval;
}

void
hyperbola_doc_tree_destroy (HyperbolaDocTree * tree)
{
	g_hash_table_destroy (tree->global_by_uri);
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	g_hash_table_destroy (tree->all_index_files);
#endif
	g_tree_traverse (tree->children, tree_node_destroy, G_IN_ORDER, NULL);
	g_tree_destroy (tree->children);
}

void
hyperbola_doc_tree_add (HyperbolaDocTree * tree, HyperbolaTreeNodeType type,
			const char **path, const char *title, const char *uri)
{
	HyperbolaTreeNode *node = NULL, *newnode;
	int i;
	gboolean do_insert = TRUE;

	if (path && path[0]) {
		node = g_tree_lookup (tree->children, (char *) path[0]);

		for (i = 1; node && path[i]; i++) {
			if (!node->children) {
				node = NULL;
				break;
			}

			node =
				g_tree_lookup (node->children,
					       (char *) path[i]);
		}

		if (!node)
			goto nopath_out;
	}

	newnode = NULL;
	if (node) {
		if (node->children)
			newnode =
				g_tree_lookup (node->children,
					       (char *) title);
	} else
		newnode = g_tree_lookup (tree->children, (char *) title);

	if (newnode) {
		if (newnode->uri)
			g_hash_table_remove (tree->global_by_uri,
					     newnode->uri);
		g_tree_remove (node ? node->children : tree->children,
			       newnode->title);
		g_free (newnode->title);
		g_free (newnode->uri);
	} else {
		newnode = g_new0 (HyperbolaTreeNode, 1);
	}

	newnode->type = type;
	newnode->uri = g_strdup (uri);
	newnode->title = g_strdup (title);
	newnode->up = node;

	if (do_insert) {
		if (newnode->uri)
			g_hash_table_insert (tree->global_by_uri,
					     newnode->uri, newnode);

		if (node) {
			if (!node->children)
				node->children =
					g_tree_new (tree_key_compare);

			g_tree_insert (node->children, newnode->title,
				       newnode);
		} else
			g_tree_insert (tree->children, newnode->title,
				       newnode);
	}

	return;

      nopath_out:
	g_warning ("Couldn't find full path for new node");
}

void
hyperbola_doc_tree_populate (HyperbolaDocTree * tree)
{
	int i;
	for (i = 0; format_handlers[i].name; i++) {
		if (format_handlers[i].populate_tree)
			format_handlers[i].populate_tree (tree);
	}
}

typedef struct {
	GSList *mappings[128];
} TreeInfo;

typedef struct {
	regex_t regex_comp;
	char **path;
} ManpageMapping;

static void
fmt_read_mapping (TreeInfo * ti, const char *srcfile)
{
	FILE *fh;
	char aline[LINE_MAX];

	fh = fopen (srcfile, "r");

	if (!fh)
		return;

	while (fgets (aline, sizeof (aline), fh)) {
		char **line_pieces;
		ManpageMapping *new_mapent;
		int regerr;
		char real_regbuf[LINE_MAX];

		if (aline[0] == '#' || isspace (aline[0]))
			continue;

		g_strstrip (aline);

		line_pieces = g_strsplit (aline, " ", 3);

		if (!line_pieces
		    || (!line_pieces[0] || !line_pieces[1] || !line_pieces[2])
		    || strlen (line_pieces[0]) != 1) {
			g_strfreev (line_pieces);
			continue;
		}

		new_mapent = g_new0 (ManpageMapping, 1);

		g_snprintf (real_regbuf, sizeof (real_regbuf), "^%s$",
			    line_pieces[1]);

		if (
		    (regerr =
		     regcomp (&new_mapent->regex_comp, real_regbuf,
			      REG_EXTENDED | REG_NOSUB))) {
			char errbuf[128];
			regerror (regerr, &new_mapent->regex_comp, errbuf,
				  sizeof (errbuf));
			g_warning ("Compilation of regex %s failed: %s",
				   real_regbuf, errbuf);
			g_free (new_mapent);
			continue;
		}

		new_mapent->path = g_strsplit (line_pieces[2], "/", -1);

		ti->mappings[(int) line_pieces[0][0]] =
			g_slist_prepend (ti->mappings
					 [(int) line_pieces[0][0]],
					 new_mapent);

		g_strfreev (line_pieces);
	}

	fclose (fh);
}

static void
fmt_free_tree_info (HyperbolaDocTree * tree)
{
	TreeInfo *tinfo;
	guint i;

	tinfo = tree->user_data;
	if (!tinfo)
		return;

	for (i = 0;
	     i < sizeof (tinfo->mappings) / sizeof (tinfo->mappings[0]); i++) {
		GSList *cur;

		for (cur = tinfo->mappings[i]; cur; cur = cur->next) {
			ManpageMapping *mapent = cur->data;
			regfree (&mapent->regex_comp);
			g_strfreev (mapent->path);
			g_free (mapent);
		}

		g_slist_free (tinfo->mappings[i]);
	}

	g_free (tinfo);
	tree->user_data = NULL;
}

static char **
fmt_map_entry (HyperbolaDocTree * tree, const char *name, char section)
{
	TreeInfo *tinfo;
	GSList *cur_slist;

	tinfo = tree->user_data;
	if (!tinfo) {
		GList *langlist, *cur;
		char mapfile[PATH_MAX];
		const char *tmapfile;

		tinfo = tree->user_data = g_new0 (TreeInfo, 1);

		/* Because mapping entries are prepended, we have to read the items in reverse order of preference */

		tmapfile = HYPERBOLA_DATADIR "/maps/pages.map";
		fmt_read_mapping (tinfo,
				  g_file_exists (tmapfile) ? tmapfile :
				  "pages.map");

		for (cur = langlist =
		     g_list_reverse (g_list_copy
				     (gnome_i18n_get_language_list (NULL)));
		     cur; cur = cur->next) {
			g_snprintf (mapfile, sizeof (mapfile),
				    HYPERBOLA_DATADIR "/maps/pages.map.%s",
				    (char *) cur->data);
			fmt_read_mapping (tinfo,
					  g_file_exists (mapfile) ? mapfile :
					  g_basename (mapfile));
		}
		g_list_free (langlist);
	}

	for (cur_slist = tinfo->mappings[(int) section]; cur_slist;
	     cur_slist = cur_slist->next) {
		ManpageMapping *mapent = cur_slist->data;

		if (!regexec (&mapent->regex_comp, name, 0, NULL, 0))
			return mapent->path;
	}

	return NULL;
}

/******** Man page format ********/

/* Caller must free this */
static char *
extract_secnum_from_filename (const char *filename)
{
	char *end_string;

#ifdef HAVE_LIBBZ2
	end_string = strstr (filename, ".bz2");
	if (end_string) 
		/* Its a bzipped man file so we need to return the
		 * secnum */
		return g_strndup (end_string - 3, 3);
	else
		free(end_string);
#endif
	end_string = strstr (filename, ".gz");

	if (!end_string) {
		/* Not a gzipped man file */
		return g_strdup (strrchr (filename, '.'));
	} else {
		/* Its a gzipped man file so we need to return the
		 * secnum */
		return g_strndup (end_string - 2, 2);
	}

}

/* Caller must free this */
static char *
man_name_without_suffix (const char *filename)
{
	char *end_string, *new_string, *ptr;

#ifdef HAVE_LIBBZ2
	end_string = strstr (filename, ".bz2");
	if (end_string) 
		/* e.g. manfile.1.bz2  would return manfile */
		return g_strndup (filename, end_string - filename - 3);
	else
		free(end_string);
#endif

	end_string = strstr (filename, ".gz");
	if (end_string) {
		/* e.g. manfile.1.gz  would return manfile */
		return g_strndup (filename, end_string - filename - 2);
	} else {
		/* e.g. manfile.1 would return manfile */
		ptr = strrchr (filename, (int) ('.'));
		if (ptr == NULL)
			new_string = g_strdup (filename);
		else
			new_string = g_strndup (filename, ptr - filename);
		return new_string;
	}
}

static void
fmt_man_populate_tree_for_subdir (HyperbolaDocTree *tree,
				  const char *basedir, 
				  char **defpath,
				  char secnum)
{
	DIR *dirh;
	struct dirent *dent;
	char uribuf[128], namebuf[128], titlebuf[128];
	char **thispath;

	dirh = opendir (basedir);
	if (!dirh)
		return;

	readdir (dirh);		/* skip . & .. */
	readdir (dirh);

	while ((dent = readdir (dirh))) {
		char *ctmp;
		char *manname;

		if (dent->d_name[0] == '.')
			continue;

		ctmp = extract_secnum_from_filename (dent->d_name);
		if (!ctmp)
			continue;

		if (ctmp[1] != secnum) {
			g_free (ctmp);
			continue;
		}

		manname = man_name_without_suffix (dent->d_name);

#ifdef HAVE_LIBBZ2
		if (strstr (dent->d_name, ".bz2")) {
			g_snprintf (namebuf, sizeof (namebuf), "%.*s",
				    (int) strlen (manname), manname);
		} else
#endif
		if (strstr (dent->d_name, ".gz")) {
			g_snprintf (namebuf, sizeof (namebuf), "%.*s",
				    (int) strlen (manname), manname);
		} else {
			/*  g_snprintf (namebuf, sizeof(namebuf), "%.*s", (int)(ctmp - dent->d_name), dent->d_name); */
			g_snprintf (namebuf, sizeof (namebuf), "%.*s",
				    (int) strlen (manname), manname);
		}
		g_free (manname);
		strcpy (titlebuf, namebuf);
		strcat (titlebuf, " (man)");

		g_snprintf (uribuf, sizeof (uribuf), "man:%s.%c", namebuf,
			    secnum);

		thispath = fmt_map_entry (tree, titlebuf, secnum);

		if (thispath)
			make_treesection (tree, thispath);

		hyperbola_doc_tree_add (tree, HYP_TREE_NODE_PAGE,
					(const char **) (thispath ? thispath :
							 defpath), titlebuf,
					uribuf);
		g_free (ctmp);
	}

	closedir (dirh);
}

static void
translate_array (char **array)
{
	int i;

	for (i = 0; array[i]; i++)
		array[i] = _(array[i]);
}


/** From 'man(7)':

       The manual sections are traditionally defined as follows:

              1 Commands
                      Those  commands that can be executed by the
                      user from within a shell.

              2 System calls
                      Those functions which must be performed  by
                      the kernel.

              3 Library calls
                      Most   of   the  libc  functions,  such  as
                      sort(3))

              4 Special files
                      Files found in /dev)

              5 File formats and conventions
                      The format for /etc/passwd and other human-
                      readable files.

              6 Games

              7 Macro packages and conventions
                      A  description  of the standard file system
                      layout, this man page, and other things.

              8 System management commands
                      Commands like mount(8), which only root can
                      execute.

              9 Kernel routines
                      This  is  a non-standard manual section and
                      is included because the source code to  the
                      Linux  kernel is freely available under the
                      GNU Public  License  and  many  people  are
                      working on changes to the kernel)
***/

static char *app_path[] = { N_("Manual"), N_("Applications"), NULL };
static char *syscall_path[] =
	{ N_("Manual"), N_("Development"), N_("System Calls"), NULL };
static char *libs_path[] =
	{ N_("Manual"), N_("Development"), N_("Library Functions"), NULL };
static char *devs_path[] =
	{ N_("Manual"), N_("Development"), N_("Devices"), NULL };
static char *cfg_path[] = { N_("Manual"), N_("Configuration Files"), NULL };
static char *games_path[] = { N_("Manual"), N_("Games"), NULL };
static char *convs_path[] = { N_("Manual"), N_("Conventions"), NULL };
static char *sysadm_path[] =
	{ N_("Manual"), N_("System Administration"), NULL };
static char *kern_path[] =
	{ N_("Manual"), N_("Development"), N_("Kernel Routines"), NULL };

static void
fmt_man_populate_tree_for_dir (HyperbolaDocTree * tree, const char *basedir)
{
	char cbuf[1024];

	g_snprintf (cbuf, sizeof (cbuf), "%s/man1", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, app_path, '1');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man2", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, syscall_path, '2');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man3", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, libs_path, '3');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man4", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, devs_path, '4');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man5", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, cfg_path, '5');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man6", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, games_path, '6');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man7", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, convs_path, '7');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man8", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, sysadm_path, '8');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man9", basedir);
	fmt_man_populate_tree_for_subdir (tree, cbuf, kern_path, '9');
}

static void
make_treesection (HyperbolaDocTree * tree, char **path)
{
	int i, j;
	char *tmp_array[20];

	for (i = 0; path[i]; i++) {
		for (j = 0; j < i; j++)
			tmp_array[j] = path[j];
		tmp_array[j] = NULL;

		hyperbola_doc_tree_add (tree, HYP_TREE_NODE_FOLDER,
					(const char **) tmp_array, path[i],
					NULL);
	}
}

static void
fmt_man_populate_tree (HyperbolaDocTree * tree)
{
	FILE *fh;
	char aline[1024];
	char **manpath = NULL;
	int i;
	char ***p;
	char **paths[] = {
		app_path, syscall_path, libs_path, devs_path, cfg_path,
		games_path, convs_path, sysadm_path, kern_path, NULL
	};

	/* Go through all the man pages:
	 * 1. Determine the places to search (run 'manpath').
	 * 2. Go through all subdirectories to find individual files.
	 * 3. For each file, add it onto the tree at the right place.
	 */

	for (p = paths; *p; p++) {
		translate_array (*p);
	}

	for (p = paths; *p; p++) {
		make_treesection (tree, *p);
	}

	fh = popen ("manpath", "r");
	g_return_if_fail (fh);

	if (fgets (aline, sizeof (aline), fh)) {
		g_strstrip (aline);
		manpath = g_strsplit (aline, ":", -1);
	} else {
		g_warning ("Couldn't get manpath");
	}
	pclose (fh);

	i = 0;
	if (manpath) {
		for (; manpath[i]; i++)
			fmt_man_populate_tree_for_dir (tree, manpath[i]);
	}
	if (!manpath || !manpath[0]) {
		fmt_man_populate_tree_for_dir (tree, "/usr/man");
		fmt_man_populate_tree_for_dir (tree, "/usr/share/man");
	}

	fmt_free_tree_info (tree);
}

/***** info pages *****/
static void
fmt_info_populate_tree_for_subdir (HyperbolaDocTree * tree,
				   const char *basedir, char **defpath)
{
	DIR *dirh;
	struct dirent *dent;

	dirh = opendir (basedir);
	if (!dirh)
		return;

	readdir (dirh);		/* skip . & .. */
	readdir (dirh);

	while ((dent = readdir (dirh))) {
		char *ctmp = NULL;
		char **thispath;
		char uribuf[128], titlebuf[128];

		if (dent->d_name[0] == '.')
			continue;

		do {
			if (ctmp)
				*ctmp = '\0';
			ctmp = strrchr (dent->d_name, '.');
		} while (ctmp && strcmp (ctmp, ".info"));

		if (!ctmp)
			continue;

		*ctmp = '\0';

		strcpy (titlebuf, dent->d_name);
		strcat (titlebuf, " (info)");

		g_snprintf (uribuf, sizeof (uribuf), "info:%s", dent->d_name);

		thispath = fmt_map_entry (tree, dent->d_name, '!');	/* Yes, we use the manpage mapping stuff just
									 * because it is easier. */

		if (thispath)
			make_treesection (tree, thispath);

		hyperbola_doc_tree_add (tree, HYP_TREE_NODE_PAGE,
					(const char **) (thispath ? thispath :
							 defpath), titlebuf,
					uribuf);
	}

	closedir (dirh);
}

static void
fmt_info_populate_tree (HyperbolaDocTree * tree)
{
	char *defpath[] = { N_("Info"), NULL };

	translate_array (defpath);
	make_treesection (tree, defpath);

	fmt_info_populate_tree_for_subdir (tree, "/usr/info", defpath);
	fmt_info_populate_tree_for_subdir (tree, "/usr/share/info", defpath);
	if (strcmp (INFODIR, "/usr/info"))
		fmt_info_populate_tree_for_subdir (tree, INFODIR, defpath);

	fmt_free_tree_info (tree);
}

/******* help: ******/
#ifndef ENABLE_SCROLLKEEPER_SUPPORT
static void
fmt_help_populate_tree_from_subdir (HyperbolaDocTree * tree,
				    const char *dirname, char **defpath)
{
	DIR *dirh;
	struct dirent *dent;
	char *subpath[10];
	int i;
	GList *langlist;

	dirh = opendir (dirname);

	if (!dirh)
		return;

	readdir (dirh);		/* skip . & .. */
	readdir (dirh);

	for (i = 0; defpath[i]; i++)
		subpath[i] = defpath[i];
	subpath[i + 1] = NULL;

	langlist = gnome_i18n_get_language_list (NULL);

	while ((dent = readdir (dirh))) {
		char afile[PATH_MAX], aline[LINE_MAX], uribuf[128];
		FILE *fh;
		GList *cur;

		/* first, try to find xml doc... */
		g_snprintf (afile, sizeof (afile), "%s/%s/C/%s.xml", dirname,
			    dent->d_name, dent->d_name);
		if (!g_file_exists (afile)) {

			/* then, try to find sgml doc... */
			g_snprintf (afile, sizeof (afile), "%s/%s/C/%s.sgml",
				    dirname, dent->d_name, dent->d_name);
			if (!g_file_exists (afile)) {

				/* last, html... */
				g_snprintf (afile, sizeof (afile),
					    "%s/%s/C/index.html", dirname,
					    dent->d_name);
				if (!g_file_exists (afile)) {
					continue;
				}
			}
		}

		g_snprintf (uribuf, sizeof (uribuf), "help:%s", dent->d_name);

		hyperbola_doc_tree_add (tree, HYP_TREE_NODE_BOOK,
					(const char **) defpath, dent->d_name,
					uribuf);

		subpath[i] = dent->d_name;

		for (cur = langlist; cur; cur = cur->next) {
			/* XXX fixme for gnome-libs 2.0 */
			g_snprintf (afile, sizeof (afile),
				    "%s/%s/%s/topic.dat", dirname,
				    dent->d_name, (char *) cur->data);
			if (g_file_exists (afile))
				break;
		}
		if (!cur)
			g_snprintf (afile, sizeof (afile),
				    "%s/%s/C/topic.dat", dirname,
				    dent->d_name);

		fh = fopen (afile, "r");
		if (!fh)
			continue;

		while (fgets (aline, sizeof (aline), fh)) {
			char **pieces;

			g_strstrip (aline);
			if (!aline[0] || aline[0] == '#')
				continue;

			pieces = g_strsplit (aline, " ", 2);

			if (pieces && pieces[0] && pieces[1]) {
				char *ctmp = strrchr (pieces[0], '.'), *ctmp2;

				if (ctmp && !strcmp (ctmp, ".html")) {
					*ctmp = '\0';
					ctmp++;
				} else
					ctmp = NULL;

				g_snprintf (uribuf, sizeof (uribuf),
					    "help:%s/%s", dent->d_name,
					    pieces[0]);

				if (ctmp) {
					ctmp2 = strchr (ctmp, '#');
					if (ctmp2) {
						ctmp2++;
						strcat (uribuf, "/");
						strcat (uribuf, ctmp2);
					}
				}

				hyperbola_doc_tree_add (tree,
							HYP_TREE_NODE_PAGE,
							(const char **)
							subpath, pieces[1],
							uribuf);
			}
			g_strfreev (pieces);
		}

		fclose (fh);
	}

	closedir (dirh);
}

#endif

#ifndef ENABLE_SCROLLKEEPER_SUPPORT
static GList *
prepend_help_dir_if_exists (GList *list,
			   char  *prefix)
{
	char *help_dir;

	help_dir = g_concat_dir_and_file (prefix, "share/gnome/help");

	if (g_file_test (help_dir, G_FILE_TEST_ISDIR)) {
		list = g_list_prepend (list, help_dir);
	} else {
		g_free (help_dir);
	}

	return list;
}

static void
fmt_help_populate_tree (HyperbolaDocTree * tree)
{
	char *app_path[] = { N_("Applications"), NULL };
	char *dirname;
	char *gnome_path; 
	char **pathdirs;
	int i;
	GList *node;
	GList *help_directories;

	translate_array (app_path);
	make_treesection (tree, app_path);

	help_directories = NULL;

	dirname = gnome_datadir_file ("gnome/help");

	if (dirname != NULL) {
		help_directories = g_list_prepend (help_directories, dirname);
	}

	help_directories = prepend_help_dir_if_exists (help_directories, PREFIX);

	gnome_path = g_getenv ("GNOME_PATH");

	if (gnome_path != NULL) {
		pathdirs = g_strsplit (gnome_path, ":", INT_MAX);

		for (i = 0; pathdirs[i] != NULL; i++) {
			help_directories = prepend_help_dir_if_exists (help_directories, 
								      pathdirs[i]);
		}
		g_strfreev (pathdirs);
	}

	help_directories = g_list_reverse (help_directories);

	for (node = help_directories; node != NULL; node = node->next) {
		fmt_help_populate_tree_from_subdir (tree, node->data, app_path);
	}

	eel_g_list_free_deep (help_directories);
}
#endif



#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/* Code for the ScrollKeeper XML Tree */

/*
 * fmt_scrollkeeper_expand_ancestor_list
 *
 * Utility function which copies the ancestors list of strings into
 * a new list.  The new list is created with an extra space to allow
 * for the addition of a further entry in the list.
 */
static char **
fmt_scrollkeeper_expand_ancestor_list (char **ancestors, int *next_free)
{
	int i;
	char **new_list;

	/* Find out how many strings are in the list */
	for (i = 0; ancestors[i] != NULL; i++);

	/* Add space for one more and a NULL to indicate the tail */
	new_list = g_new0 (char *, i + 2);

	/* Just copy the pointers */
	for (i = 0; ancestors[i] != NULL; i++) {
		new_list[i] = ancestors[i];
	}

	new_list[i] = new_list[i + 1] = NULL;

	*next_free = i;

	return new_list;
}


/*
 * fmt_scrollkeeper_parse_toc_section
 *
 * This function is intended to process an individual tocsect tag from 
 * a ScrollKeeper Table of Contents XML file.
 *
 * It is similar to the fmt_scrollkeeper_parse_section function.  It first
 * inserts information about the current node into the tree, and then 
 * recursively process other tocsect nodes beneath it.
 *
 * The GTree which stores the node names and their path within the tree
 * is alphabetically sorted.  Such sorting is inappropriate for a TOC.
 * To get around this, the relative position of each node is prepended
 * to its title.  This number is removed for insertion into the GTK CTree.
 *
 *     base_uri:  String specifying the URI of the document whose TOC is
 *                being processed.  All section tags are appended to this
 *                to create the URI of a particular section in the doc.
 *
 *     this_pos:  Specifies the position of this tag within its section,
 *     	          i.e. first, second, third, etc. 
 */
static void
fmt_scrollkeeper_parse_toc_section (HyperbolaDocTree * tree, char **ancestors,
				    xmlNodePtr node, char *base_uri,
				    int this_pos, unsigned int go_deeper)
{
	xmlNodePtr next_child;

	char *content;
	char **section;
	char *sect_uri;
	int i, pos;

	char separator[2] = { '\0' };


	next_child = node->xmlChildrenNode;

	/* Set up the positioning information for the HyperbolaDocTree */
	section = fmt_scrollkeeper_expand_ancestor_list (ancestors, &i);
	content = xmlNodeGetContent (next_child);
	section[i] = g_strdup_printf ("%03d. %s", this_pos, content);
	xmlFree (content);
	section[i + 1] = NULL;

	/* 
	 * Set the correct separator token.  SGML requires ? but
	 * the other use #
	 */
	if (!g_strncasecmp ("help", base_uri, 4)) {
		separator[0] = '?';
	} else {
		separator[0] = '#';
	}

	sect_uri = g_strconcat (base_uri, separator,
				xmlGetProp (node, "linkid"), NULL);

	/* Process this node and insert it into the tree */
	hyperbola_doc_tree_add (tree, HYP_TREE_NODE_SECTION,
				(const char **) ancestors, section[i],
				sect_uri);


	/* Process subsections of this node */
	if (go_deeper > 0) {
		for (pos = 1, next_child = next_child->next;
		     next_child != NULL; next_child = next_child->next) {
			if (!g_strncasecmp (next_child->name, "tocsect", 7)) {
				fmt_scrollkeeper_parse_toc_section (tree,
								    section,
								    next_child,
								    base_uri,
								    pos,
								    (go_deeper
								     - 1));
				pos++;
			}
		}
	}

	g_free (sect_uri);
	g_free (section[i]);
	g_free (section);
}

/*
 * fmt_scrollkeeper_parse_doc_toc
 *
 * This function is the entry point into the processing of a Table of Contents
 * XML file.  The TOC file presented by ScrollKeeper is less complex than
 * the main contents file.  It simply consists of a root node under which
 * are tocsect tags, which may be nested.  
 *
 * Usually they have a number appended to them to indicate which level they 
 * are at in the TOC.  This is ignored here as under the root should only
 * contain tocsect1 tags.  The fmt_scrollkeeper_parse_toc_section ignores them
 * also so that it may be recursive.
 *
 * The first two parameters are the same as the other functions.
 *
 * 	toc_file:	String containing the name of the XML file which
 * 			contains the table of contents information.
 *
 * 	base_uri:	String containing the fully qualified URI of the
 * 			document file.  All contents references will be
 * 			appended to this.
 */
static void
fmt_scrollkeeper_parse_doc_toc (HyperbolaDocTree * tree, char **ancestors,
				char *toc_file, char *base_uri)
{
	xmlDocPtr toc_doc;
	xmlNodePtr next_child;
	errorSAXFunc xml_error_handler;
 	warningSAXFunc xml_warning_handler;
   	fatalErrorSAXFunc xml_fatal_error_handler;

	int pos;

	unsigned int levels_to_process = 5;

	xml_error_handler = xmlDefaultSAXHandler.error;
	xmlDefaultSAXHandler.error = NULL;
	xml_warning_handler = xmlDefaultSAXHandler.warning;
	xmlDefaultSAXHandler.warning = NULL;
	xml_fatal_error_handler = xmlDefaultSAXHandler.fatalError;
	xmlDefaultSAXHandler.fatalError = NULL;
	toc_doc = xmlParseFile (toc_file);
	xmlDefaultSAXHandler.error = xml_error_handler;
   	xmlDefaultSAXHandler.warning = xml_warning_handler;
   	xmlDefaultSAXHandler.fatalError = xml_fatal_error_handler;

	if (!toc_doc) {
		/*
		g_warning ("Unable to parse ScrollKeeper TOC XML file:\n\t%s",
			   toc_file);
		*/
		return;
	}

	/* Process the top-level tocsect nodes in the file */
	for (pos = 1, next_child = toc_doc->xmlRootNode->xmlChildrenNode; next_child != NULL;
	     next_child = next_child->next) {
		if (!g_strncasecmp (next_child->name, "tocsect", 7)) {
			fmt_scrollkeeper_parse_toc_section (tree, ancestors,
							    next_child,
							    base_uri, pos,
							    levels_to_process);
			pos++;
		}
	}

	xmlFreeDoc (toc_doc);
}


/*
 * fmt_scrollkeeper_parse_document
 *
 * This function inserts relevant information into the hyperbola tree for
 * an individual document.  
 *
 * It must establish what format the document is in in order to prepend the 
 * appropriate prefix to the URI.  This URI is a combination of the prefix 
 * and the document's location.
 *
 * It also attempts to locate a Table of Contents for the particular document
 * it is processing.  If it can, it calls fmt_scrollkeeper_parse_doc_toc to
 * process that Table of Contents file.
 *
 * It also attempts to locate an index for the particular document it is 
 * processing.  If the index exists it is entered into the all_index_files
 * hashtable (member of the HyperbolaDocTree). The uri of the document is
 * the key to the hashtable, and the index file name (incl. path) is the
 * value.
 *
 * 	ancestors:	The textual path in the tree at which to root changes
 *
 * 	node:		The XML node representing the document to be processed.
 */
static void
fmt_scrollkeeper_parse_document (HyperbolaDocTree * tree, char **ancestors,
				 xmlNodePtr node)
{
	xmlNodePtr next_child;
	char **section;

	FILE *pipe;
	int i;
	int bytes_read;


	char *doc_uri;
	char *doc_data[3] = { NULL };


	next_child = node->xmlChildrenNode;

	/* Obtain info about the document from the XML node describing it */
	for (; next_child != NULL; next_child = next_child->next) {

		if (!g_strcasecmp (next_child->name, "doctitle")) {
			doc_data[0] = xmlNodeGetContent (next_child->xmlChildrenNode);
		} else if (!g_strcasecmp (next_child->name, "docsource")) {
			doc_data[1] = xmlNodeGetContent (next_child->xmlChildrenNode);
		} else if (!g_strcasecmp (next_child->name, "docformat")) {
			doc_data[2] = xmlNodeGetContent (next_child->xmlChildrenNode);
		}
	}

	/* Expand the ancestor list and append this documents name to it */
	section = fmt_scrollkeeper_expand_ancestor_list (ancestors, &i);
	section[i] = doc_data[0];
	section[i + 1] = NULL;


	/* Decide on the appropriate prefix */
	if (!g_strcasecmp ("text/html", doc_data[2])) {
		doc_uri = g_strconcat ("file://", doc_data[1], NULL);
	} else if (!g_strcasecmp ("text/sgml", doc_data[2])) {
		doc_uri = g_strconcat ("gnome-help:", doc_data[1], NULL);
	} else if (!g_strcasecmp ("info", doc_data[2])) {
		doc_uri = g_strconcat ("info:", doc_data[1], NULL);
	} else if (!g_strcasecmp ("applications/x-troff", doc_data[2])) {
		/* Man Pages */
		doc_uri = g_strconcat ("man:", doc_data[1], NULL);
	} else {
		/* If not a type we deal with then don't do anything else */
		g_free (section);
		xmlFree (doc_data[0]);
		xmlFree (doc_data[1]);
		xmlFree (doc_data[2]);
		return;
	}

	/* Insert info for this document into the tree */
	hyperbola_doc_tree_add (tree, HYP_TREE_NODE_BOOK,
				(const char **) ancestors, doc_data[0],
				doc_uri);
	/*
	 *  Only do the following if creating the contents tree,
	 *  for the contents page, ie get TOCs and a list
	 *  of index files
	 */
	if(tree->contents_tree_type){

                char *toc_location;
                toc_location = g_new0 (char, 1024);
                /* Get the TOC, if there is one, for the document */
                g_snprintf (toc_location, 1024,
                            "scrollkeeper-get-toc-from-docpath %s", doc_data[1]);

                pipe = popen (toc_location, "r");
                bytes_read = fread ((void *) toc_location, sizeof (char), 1024, pipe);

                if (bytes_read > 0) {
                        toc_location[bytes_read - 1] = '\0';


                        /* Exit code of 0 indicates ScrollKeeper returned a TOC file path */
                        if (!pclose (pipe)) {
                                fmt_scrollkeeper_parse_doc_toc (tree, section,
                                                                toc_location,
                                                                doc_uri);
                        }
                }

                g_free (toc_location);
        }else{
		char *index_location;
		index_location = g_new0 (char, 1024);
		/* Get the index file, if it exists, for the document */
		g_snprintf (index_location, 1024,
			    "scrollkeeper-get-index-from-docpath %s", doc_data[1]);
		pipe = popen (index_location, "r");
		bytes_read = fread ((void *) index_location, sizeof (char), 1024, pipe);

		if (bytes_read > 0) {
			index_location[bytes_read - 1] = '\0';
			/* Exit code of 0 indicates ScrollKeeper returned an index file */
			if (!pclose (pipe)) {
				char *key, *index;
				key = g_strdup(doc_uri);
				index = g_strdup(index_location);
				g_hash_table_insert(tree->all_index_files, key,
							index);
			}
		}
		g_free (index_location);
	}
	g_free (doc_uri);
	g_free (section);
	xmlFree (doc_data[0]);
	xmlFree (doc_data[1]);
	xmlFree (doc_data[2]);
}


/*
 * fmt_scrollkeeper_parse_section
 * 
 * This function does the actual processing of a sect tag as encountered
 * in a ScrollKeeper Contents List file.  Each section may contain a mix
 * of children, either other sections or document info.
 *
 * This function may be called recursively as each section's children are
 * processed.
 *
 * 	ancestors:	The path in the tree at which additions will be
 * 			rooted.  Points to the titles of all parent
 * 			sections.  
 *
 * 	node:		Pointer to the node in the XML file to be orocessed.
 */
static void
fmt_scrollkeeper_parse_section (HyperbolaDocTree * tree, char **ancestors,
				xmlNodePtr node)
{
	xmlNodePtr next_child;
	char **section;
	int i;

	next_child = node->xmlChildrenNode;

	/* Make space for this level and add the title of this node to the path */
	section = fmt_scrollkeeper_expand_ancestor_list (ancestors, &i);
	section[i] = xmlNodeGetContent (next_child->xmlChildrenNode);
	section[i + 1] = NULL;

	/* There is no URI so use this function instead */
	make_treesection (tree, section);

	for (; next_child->next != NULL; next_child = next_child->next) {

		if (!g_strcasecmp (next_child->next->name, "sect")) {
			fmt_scrollkeeper_parse_section (tree, section,
							next_child->next);
		} else if (!g_strcasecmp (next_child->next->name, "doc")) {
			fmt_scrollkeeper_parse_document (tree, section,
							 next_child->next);
		}
	}

	xmlFree (section[i]);
	g_free (section);
}


/* 
 * fmt_scrollkeeper_parse_xml
 * 
 * This function ensure that the XML file is a valid ScrollKeeper Contents
 * List file and then parses all top level sections of the file.  Each sect
 * tag represents a section of documentation.  
 *
 * 	tree:	    The Hyperbola tree which maintains the doc info
 * 	defpath:    The position in the tree at which to root additions.
 * 	doc:	    The pointer to the XML Document.  
 */
static void
fmt_scrollkeeper_parse_xml (HyperbolaDocTree * tree, char **defpath,
			    xmlDocPtr doc)
{
	xmlNodePtr node;

	/* Ensure the document is valid and a real ScrollKeeper document */
	if (!doc->xmlRootNode || !doc->xmlRootNode->name ||
	    g_strcasecmp (doc->xmlRootNode->name, "ScrollKeeperContentsList")) {
		g_warning ("Invalid ScrollKeeper XML Contents List!");
		return;
	}

	/* Start parsing the list and add to the tree */
	for (node = doc->xmlRootNode->xmlChildrenNode; node != NULL; node = node->next) {
		if (!g_strcasecmp (node->name, "sect"))
			fmt_scrollkeeper_parse_section (tree, defpath, node);
	}
}

/* trim empty (the ones that don't have docs in them ) branches from 
   the xml tree 
   
   cl_node is the <sect> subsection to be checked; if it has
   only one child node that is the title of the section then 
   it has to be removed
*/
static void
fmt_scrollkeeper_trim_empty_branches (xmlNodePtr cl_node)
{
	xmlNodePtr node, next;

	if (cl_node == NULL)
		return;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!strcmp (node->name, "sect") &&
		    node->xmlChildrenNode->next !=
		    NULL) fmt_scrollkeeper_trim_empty_branches (node->
								xmlChildrenNode->next);

		if (!strcmp (node->name, "sect") &&
		    node->xmlChildrenNode->next == NULL) {
			xmlUnlinkNode (node);
			xmlFreeNode (node);
		}
	}
}

/* checks if there is any doc in the tree */
static int
fmt_scrollkeeper_tree_empty (xmlNodePtr cl_node)
{
	xmlNodePtr node, next;
	int ret_val;

	if (cl_node == NULL)
		return 1;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!strcmp (node->name, "sect") &&
		    node->xmlChildrenNode->next != NULL) {
			ret_val = fmt_scrollkeeper_tree_empty (node->xmlChildrenNode->next);
			if (!ret_val)
				return ret_val;
		}

		if (!strcmp (node->name, "doc")) {
			return 0;
		}
	}
	
	return 1;
}

/* retrieve the XML tree of a certain locale */
static xmlDocPtr
fmt_scrollkeeper_get_xml_tree_of_locale (char *locale)
{
	xmlDocPtr doc;
	FILE *pipe;

	char *xml_location;
	int bytes_read;
	
	if (locale == NULL)
	    return NULL;

	xml_location = g_new0 (char, 1024);

	/* Use g_snprintf here because we don't know how long the location will be */
	g_snprintf (xml_location, 1024, "scrollkeeper-get-content-list %s",
		    locale);

	pipe = popen (xml_location, "r");
	bytes_read = fread ((void *) xml_location, sizeof (char), 1024, pipe);

	/* Make sure that we don't end up out-of-bunds */
	if (bytes_read < 1) {
		pclose (pipe);
		g_free (xml_location);
		return NULL;
	}

	/* Make sure the string is properly terminated */
	xml_location[bytes_read - 1] = '\0';
	
	doc = NULL;

	/* Exit code of 0 means we got a path back from ScrollKeeper */
	if (!pclose (pipe))
		doc = xmlParseFile (xml_location);

	g_free (xml_location);

	return doc;
}


/*
 * fmt_scrollkeeper_populate_tree
 * 
 * Entry point into the ScrollKeeper specific code for Hyperbola.
 *
 * This function obtains the location of the ScrollKeeper Contents List 
 * XML file, if it exists.  If the XML file is found and can be parsed
 * by the libXml library, the parsing of that file continues.
 * There is a fallback to the C locale tree if the current locale is
 * not there.
 * It populates the main category tree that holds all the docs installed
 * through Scrollkeeper. 
 *
 * 	tree:	Pointer to the tree structure used by Hyperbola to
 * 		maintain the list of documentation available.
 */
static void
fmt_scrollkeeper_populate_tree (HyperbolaDocTree * tree)
{
	xmlDocPtr doc;
	GList *node;
	
	char *tree_path[] = { NULL };

	doc = NULL;
	for (node = gnome_i18n_get_language_list ("LC_MESSAGES");
	     node != NULL;
	     node = node->next) {
		doc = fmt_scrollkeeper_get_xml_tree_of_locale (node->data);
		if (doc != NULL) {
			if (doc->xmlRootNode != NULL && !fmt_scrollkeeper_tree_empty(doc->xmlRootNode->xmlChildrenNode)) {
				break;
			} else {
				xmlFreeDoc (doc);
				doc = NULL;
			}
		}
	}
		
	if (doc) {
		fmt_scrollkeeper_trim_empty_branches (doc->xmlRootNode->xmlChildrenNode);
		fmt_scrollkeeper_parse_xml (tree, tree_path, doc);
		xmlFreeDoc (doc);
	}
}



static char *
remove_leading_and_trailing_white_spaces (char * str)
{
    	int i, len;
   
    	len = strlen(str);
   
    	for(i = len-1; i >= 0; i--) {
        	if (str[i] == ' ' || str[i] == '\t' ||
            	    str[i] == '\n' || str[i] == '\r')
            		str[i] = '\0';
        	else
            		break;
    	}

    	while (*str == ' ' || *str == '\t' ||
               *str == '\n' || *str == '\r')
           	str++;

    	return str;
}

static void
fmt_toplevel_add_doc (HyperbolaDocTree * tree, char * omf_name)
{
    	xmlDocPtr doc;
    	xmlNodePtr node;
    	char *uri, *title, *str, *doc_path, prefix[4];
	
	uri = NULL;
    	title = NULL;
    	prefix[0] = '\1';
    	prefix[1] = '.';
    	prefix[2] = ' ';
    	prefix[3] = '\0';
	doc_path = NULL;
        
    	doc = xmlParseFile(omf_name);
    	if (doc == NULL)
        	return;
	
    	if (doc->xmlRootNode == NULL || doc->xmlRootNode->xmlChildrenNode == NULL ||
            doc->xmlRootNode->xmlChildrenNode->xmlChildrenNode == NULL) {
        	xmlFreeDoc(doc);
		return;
    	}
        
    	for(node = doc->xmlRootNode->xmlChildrenNode->xmlChildrenNode; node != NULL;
            node = node->next) {
        	if (!strcmp(node->name, "identifier")) {
	    		doc_path = xmlGetProp(node, "url");
	    		uri = g_strconcat("gnome-help:", doc_path, NULL); 
		}
	    
		if (!strcmp(node->name, "title")) {
	    		char *ptr;
	    
	    		ptr = xmlNodeGetContent (node->xmlChildrenNode);
			if (ptr != NULL) {
				str = remove_leading_and_trailing_white_spaces (ptr);
				title = g_strconcat (prefix, str, NULL);
				xmlFree (ptr);
			}
		}   
    	}
    
    	if (uri != NULL && title != NULL) {
	
       	char toc_location[1024];
		FILE *pipe;
		int bytes_read, i;
		char **section;
		char *tree_path[1];
	
		tree_path[0] = NULL;
    
       	hyperbola_doc_tree_add (tree, HYP_TREE_NODE_BOOK,
	             	        	(const char **)tree_path, title, uri);
				
		section = fmt_scrollkeeper_expand_ancestor_list(tree_path, &i);
		section[i] = title;
				
		/* Get the TOC, if there is one, for the document */ 
       	g_snprintf (toc_location, 1024, "scrollkeeper-get-toc-from-docpath %s", 
		  		doc_path);

	  	pipe = popen (toc_location, "r");
  		bytes_read = fread ((void *)toc_location, sizeof(char), 1024, pipe);

	  	if (bytes_read > 0) {
	    	toc_location[bytes_read - 1] = '\0';
 
    		/* Exit code of 0 indicates ScrollKeeper returned a TOC file path */
   	    		if(!pclose(pipe))
  	    			fmt_scrollkeeper_parse_doc_toc (tree, section, toc_location, uri);
			
    		}

		g_free(uri);
		g_free(title);
		g_free(section);

    	}
}


static int
get_locale_score (GList *locales, const xmlChar *locale) 
{
	GList *node;
	int score;

	score = 0;
	for (node = locales; node != NULL; node = node->next) {
		if (strcmp (locale, (char *) node->data) == 0) {
                        return score;
		}
		score++;
	}

	return -1;
}



/* returns -1 on invalid locale (not found), or the position
 * in the locale list.  the lower the position the better, that
 * is, the lower number should get precedence */
static int
locale_score (GList *locales, xmlNode *doc_node)
{
	xmlChar *locale;
	int score;

	if (doc_node == NULL) {
		return -1;
	}

	locale = xmlGetProp (doc_node, "locale");
	if (locale == NULL) {
		score = get_locale_score (locales, "C");
	} else {
                score = get_locale_score (locales, locale);
                xmlFree (locale);
        }

        return score;
}

/* do we want to use new_doc rather then current_doc.  That is, is new_doc
 * a better translation then current_doc */
static gboolean
is_new_locale_better (GList *locales, xmlNode *current_doc, xmlNode *new_doc)
{
	int current_score, new_score;

	new_score = locale_score (locales, new_doc);

	/* if new document is not in our list, forget it */
	if (new_score == -1) {
		return FALSE;
	}

	current_score = locale_score (locales, current_doc);

	/* if new document is better (has lower score) then we want to use
	 * that, or if the current document is bogus (-1) */
	if (new_score < current_score
	    || current_score == -1) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static char *
get_path_from_node (const char *omf_dir, xmlNode *docpath_node)
{
	char *str, *omf_path, *omf_name;

	str = xmlNodeGetContent (docpath_node->xmlChildrenNode);
	omf_name = remove_leading_and_trailing_white_spaces (str);
	omf_path = g_strdup_printf ("%s/%s", omf_dir, omf_name);

	xmlFree (str);

	return omf_path;
}

static gboolean
has_content (xmlNodePtr node)
{
	xmlChar *content;

	content = xmlNodeGetContent (node);
	xmlFree (content);
	return content != NULL;
}

/* Note: Locales should include "C" as the last element.  Basically for use
 * with locales lists returned by gnome_i18n_get_language_list. */
static gboolean
fmt_toplevel_parse_xml_tree (HyperbolaDocTree * tree,
			     xmlDocPtr doc,
			     GList *locales)
{
	xmlNodePtr doc_node, docpath_node, best_path_node;
    	char omf_dir[256], *omf_path;
	FILE *pipe;
	int bytes_read;
	gboolean node_added;

    	if (doc == NULL || doc->xmlRootNode == NULL)
        	return FALSE;

	if (locales == NULL)
		return FALSE;
		
	pipe = popen ("scrollkeeper-config --omfdir", "r");
	bytes_read = fread ((void *) omf_dir, sizeof (char), 128, pipe);

	/* Make sure that we don't end up out-of-bunds */
	if (bytes_read < 1) {
		pclose (pipe);
		return FALSE;
	}

	/* Make sure the string is properly terminated */
	omf_dir[bytes_read - 1] = '\0';

	/* Exit code of 0 means we got a path back from ScrollKeeper */
	if (pclose (pipe))
		return FALSE;

	node_added = FALSE;
	
    	for(doc_node = doc->xmlRootNode->xmlChildrenNode; doc_node != NULL; 
            doc_node = doc_node->next) {

		/* nothing found yet */
		best_path_node = NULL;

		/* check out the doc for the current locale */

        	for(docpath_node = doc_node->xmlChildrenNode; 
	    	    docpath_node != NULL;
	    	    docpath_node = docpath_node->next) {	
			/* check validity of the node first */
			if (has_content (docpath_node->xmlChildrenNode)
			    && is_new_locale_better (locales,
						     best_path_node,
						     docpath_node)) {
				omf_path = get_path_from_node (omf_dir,
							       docpath_node);

				if (access (omf_path, R_OK) == 0)
					/* found a good one */
					best_path_node = docpath_node;

				g_free (omf_path);
			}
		}

		if (best_path_node != NULL) {
			omf_path = get_path_from_node (omf_dir, best_path_node);
			fmt_toplevel_add_doc (tree, omf_path);
			g_free (omf_path);

			node_added = TRUE;
		}
    	}

	return node_added;
}

/* entry point for filling the toplevel tree that holds the very 
 * important docs listed in 
 * $prefix/share/nautilus/components/hyperbola/topleveldocs.xml
 * it falls back to the C locale doc if the translated one is 
 * not there.
 */
static int fmt_toplevel_populate_tree (HyperbolaDocTree * tree)
{
    	xmlDocPtr toplevel_doc;
    	char *toplevel_file;
	int retval;
	GList *locales;
  	
	toplevel_file = HYPERBOLA_DATADIR "/topleveldocs.xml";

    	toplevel_doc = xmlParseFile (toplevel_file); 
	
    	if (toplevel_doc != NULL) {
		locales = gnome_i18n_get_language_list ("LC_MESSAGES");
		retval = fmt_toplevel_parse_xml_tree (tree, toplevel_doc,
						      locales);
    	}
    	else {
		retval = 0;
		/*
        	g_warning ("Unable to locate toplevel XML file:\n\t%s", toplevel_file);
		*/
    	}

    	xmlFreeDoc (toplevel_doc);
	
	return retval;
}

int
hyperbola_top_doc_tree_populate (HyperbolaDocTree * tree)
{
	return fmt_toplevel_populate_tree(tree);
}

#endif /* ENABLE_SCROLLKEEPER_SUPPORT */
