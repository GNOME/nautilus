#include <config.h>
#include <gnome.h>
#include <zlib.h>

#include "hyperbola-filefmt.h"
#include <dirent.h>
#include <regex.h>
#include <limits.h>
#include <ctype.h>

#include <string.h>

typedef struct {
  const char *name;
  void (*populate_tree)(HyperbolaDocTree *tree);
} FormatHandler;

static void fmt_man_populate_tree(HyperbolaDocTree *tree);

static void fmt_info_populate_tree(HyperbolaDocTree *tree);

static void fmt_help_populate_tree(HyperbolaDocTree *tree);

static void make_treesection(HyperbolaDocTree *tree, char **path);

static FormatHandler format_handlers[] = {
  {"help", fmt_help_populate_tree},
  {"man", fmt_man_populate_tree},
  {"info", fmt_info_populate_tree},
  {NULL, NULL}
};

static gboolean
tree_node_destroy(gpointer key, gpointer data, gpointer user_data)
{
  HyperbolaTreeNode *node = data;

  g_free(node->title);
  g_free(node->uri);

  if(node->children)
    {
      g_tree_traverse(node->children, tree_node_destroy, G_IN_ORDER, NULL);
      g_tree_destroy(node->children);
    }

  return FALSE;
}

static gint
tree_key_compare(gconstpointer k1, gconstpointer k2)
{
  return g_strcasecmp(k1, k2);
}

HyperbolaDocTree *
hyperbola_doc_tree_new(void)
{
  HyperbolaDocTree *retval = g_new0(HyperbolaDocTree, 1);

  retval->global_by_uri = g_hash_table_new(g_str_hash, g_str_equal);
  retval->children = g_tree_new(tree_key_compare);

  return retval;
}

void
hyperbola_doc_tree_destroy(HyperbolaDocTree *tree)
{
  g_hash_table_destroy(tree->global_by_uri);
  g_tree_traverse(tree->children, tree_node_destroy, G_IN_ORDER, NULL);
  g_tree_destroy(tree->children);
}

void
hyperbola_doc_tree_add(HyperbolaDocTree *tree, HyperbolaTreeNodeType type, const char **path,
		       const char *title, const char *uri)
{
  HyperbolaTreeNode *node = NULL, *newnode;
  int i;
  gboolean do_insert = TRUE;

  if(path && path[0])
    {
      node = g_tree_lookup(tree->children, (char *)path[0]);

      for(i = 1; node && path[i]; i++)
	{
	  if(!node->children)
	    {
	      node = NULL;
	      break;
	    }

	  node = g_tree_lookup(node->children, (char *)path[i]);
	}

      if(!node)
	goto nopath_out;
    }

  newnode = NULL;
  if(node)
    {
      if(node->children)
	newnode = g_tree_lookup(node->children, (char *)title);
    }
  else
    newnode = g_tree_lookup(tree->children, (char *)title);

  if(newnode)
    {
      if(newnode->uri)
	g_hash_table_remove(tree->global_by_uri, newnode->uri);
      g_tree_remove(node?node->children:tree->children, newnode->title);
      g_free(newnode->title);
      g_free(newnode->uri);
    }
  else
    {
      newnode = g_new0(HyperbolaTreeNode, 1);
    }

  newnode->type = type;
  newnode->uri = g_strdup(uri);
  newnode->title = g_strdup(title);
  newnode->up = node;

  if(do_insert)
    {
      if(newnode->uri)
	g_hash_table_insert(tree->global_by_uri, newnode->uri, newnode);

      if(node)
	{
	  if(!node->children)
	    node->children = g_tree_new(tree_key_compare);
	  
	  g_tree_insert(node->children, newnode->title, newnode);
	}
      else
	g_tree_insert(tree->children, newnode->title, newnode);
    }

  return;

 nopath_out:
  g_warning("Couldn't find full path for new node");
}

void
hyperbola_doc_tree_populate(HyperbolaDocTree *tree)
{
  int i;
  for(i = 0; format_handlers[i].name; i++)
    {
      if(format_handlers[i].populate_tree)
	format_handlers[i].populate_tree(tree);
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
fmt_read_mapping(TreeInfo *ti, const char *srcfile)
{
  FILE *fh;
  char aline[LINE_MAX];

  fh = fopen(srcfile, "r");

  if(!fh)
    return;

  while(fgets(aline, sizeof(aline), fh))
    {
      char **line_pieces;
      ManpageMapping *new_mapent;
      int regerr;
      char real_regbuf[LINE_MAX];

      if(aline[0] == '#' || isspace(aline[0]))
	continue;

      g_strstrip(aline);

      line_pieces = g_strsplit(aline, " ", 3);

      if(!line_pieces
	 || (!line_pieces[0] || !line_pieces[1] || !line_pieces[2])
	 || strlen(line_pieces[0]) != 1)
	{
	  g_strfreev(line_pieces);
	  continue;
	}

      new_mapent = g_new0(ManpageMapping, 1);

      g_snprintf(real_regbuf, sizeof(real_regbuf), "^%s$", line_pieces[1]);

      if((regerr = regcomp(&new_mapent->regex_comp, real_regbuf, REG_EXTENDED|REG_NOSUB)))
	{
	  char errbuf[128];
	  regerror(regerr, &new_mapent->regex_comp, errbuf, sizeof(errbuf));
	  g_warning("Compilation of regex %s failed: %s", real_regbuf, errbuf);
	  g_free(new_mapent);
	  continue;
	}

      new_mapent->path = g_strsplit(line_pieces[2], "/", -1);

      ti->mappings[(int)line_pieces[0][0]] = g_slist_prepend(ti->mappings[(int)line_pieces[0][0]], new_mapent);

      g_strfreev(line_pieces);
    }

  fclose(fh);
}

static void
fmt_free_tree_info(HyperbolaDocTree *tree)
{
  TreeInfo *tinfo;
  int i;

  tinfo = tree->user_data;
  if(!tinfo)
    return;

  for(i = 0; i < sizeof(tinfo->mappings)/sizeof(tinfo->mappings[0]); i++)
    {
      GSList *cur;

      for(cur = tinfo->mappings[i]; cur; cur = cur->next)
	{
	  ManpageMapping *mapent = cur->data;
	  regfree(&mapent->regex_comp);
	  g_strfreev(mapent->path);
	  g_free(mapent);
	}

      g_slist_free(tinfo->mappings[i]);
    }

  g_free(tinfo);
  tree->user_data = NULL;
}

static char **
fmt_map_entry(HyperbolaDocTree *tree, const char *name, char section)
{
  TreeInfo *tinfo;
  GSList *cur_slist;

  tinfo = tree->user_data;
  if(!tinfo)
    {
      GList *langlist, *cur;
      char mapfile[PATH_MAX];
      const char *tmapfile;

      tinfo = tree->user_data = g_new0(TreeInfo, 1);

      /* Because mapping entries are prepended, we have to read the items in reverse order of preference */

      tmapfile = HYPERBOLA_DATADIR "/maps/pages.map";
      fmt_read_mapping(tinfo, g_file_exists(tmapfile)?tmapfile:"pages.map");

      for(cur = langlist = g_list_reverse(g_list_copy(gnome_i18n_get_language_list(NULL)));
	  cur; cur = cur->next)
	{
	  g_snprintf(mapfile, sizeof(mapfile),
		     HYPERBOLA_DATADIR "/maps/pages.map.%s",
		     (char *)cur->data);
	  fmt_read_mapping(tinfo, g_file_exists(mapfile)?mapfile:g_basename(mapfile));
	}
      g_list_free(langlist);
    }

  for(cur_slist = tinfo->mappings[(int)section]; cur_slist; cur_slist = cur_slist->next)
    {
      ManpageMapping *mapent = cur_slist->data;

      if(!regexec(&mapent->regex_comp, name, 0, NULL, 0))
	return mapent->path;
    }

  return NULL;
}

/******** Man page format ********/
static void
fmt_man_populate_tree_for_subdir(HyperbolaDocTree *tree, const char *basedir, char **defpath, char secnum)
{
  DIR *dirh;
  struct dirent *dent;
  char uribuf[128], namebuf[128], titlebuf[128];
  char **thispath;

  dirh = opendir(basedir);
  if(!dirh)
    return;

  readdir(dirh); /* skip . & .. */
  readdir(dirh);

  while((dent = readdir(dirh)))
    {
      char *ctmp;

      if(dent->d_name[0] == '.')
	continue;

      ctmp = strrchr(dent->d_name, '.');
      if(!ctmp)
	continue;

      if(ctmp[1] != secnum)
	continue; /* maybe the extension was '.gz' or something, which we most definitely don't handle right now */      

      g_snprintf(namebuf, sizeof(namebuf), "%.*s", (int)(ctmp - dent->d_name), dent->d_name);
      strcpy(titlebuf, namebuf);
      strcat(titlebuf, " (man)");

      g_snprintf(uribuf, sizeof(uribuf), "man:%s.%c", namebuf, secnum);

      thispath = fmt_map_entry(tree, titlebuf, secnum);

      if(thispath)
	make_treesection(tree, thispath);

      hyperbola_doc_tree_add(tree, HYP_TREE_NODE_PAGE, (const char **)(thispath?thispath:defpath), titlebuf, uribuf);
    }

  closedir(dirh);
}

static void
translate_array(char **array)
{
  int i;

  for(i = 0; array[i]; i++)
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
static char *man_path[] = {N_("Manual"), NULL};
static char *cfg_path[] = {N_("System"), N_("Configuration"), N_("Config files"), NULL};
static char *app_path[] = {N_("Applications"), N_("Command Line"), NULL};
static char *dev_path[] = {N_("Development"), N_("APIs"), N_("Miscellaneous"), NULL };
static char *syscall_path[] = {N_("Development"), N_("APIs"), N_("System Calls"), NULL };

static void
fmt_man_populate_tree_for_dir(HyperbolaDocTree *tree, const char *basedir)
{
  char cbuf[1024];

  g_snprintf(cbuf, sizeof(cbuf), "%s/man1", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, app_path, '1');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man2", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, syscall_path, '2');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man3", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, dev_path, '3');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man4", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, man_path, '4');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man5", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, cfg_path, '5');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man6", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, app_path, '6');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man7", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, man_path, '7');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man8", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, app_path, '8');

  g_snprintf(cbuf, sizeof(cbuf), "%s/man9", basedir);
  fmt_man_populate_tree_for_subdir(tree, cbuf, man_path, '9');
}

static void
make_treesection(HyperbolaDocTree *tree, char **path)
{
  int i, j;
  char *tmp_array[20];

  for(i = 0; path[i]; i++)
    {
      for(j = 0; j < i; j++)
	tmp_array[j] = path[j];
      tmp_array[j] = NULL;

      hyperbola_doc_tree_add(tree, HYP_TREE_NODE_FOLDER, (const char **)tmp_array, path[i], NULL);
    }
}

static void
fmt_man_populate_tree(HyperbolaDocTree *tree)
{
  FILE *fh;
  char aline[1024];
  char **manpath = NULL;
  int i;
  /* Go through all the man pages:
     1. Determine the places to search (run 'manpath').
     2. Go through all subdirectories to find individual files.
     3. For each file, add it onto the tree at the right place.
  */

  translate_array(man_path);
  translate_array(cfg_path);
  translate_array(app_path);
  translate_array(dev_path);
  translate_array(syscall_path);

  make_treesection(tree, man_path);
  make_treesection(tree, cfg_path);
  make_treesection(tree, app_path);
  make_treesection(tree, dev_path);
  make_treesection(tree, syscall_path);

  fh = popen("manpath", "r");
  g_return_if_fail(fh);

  if(fgets(aline, sizeof(aline), fh))
    {
      g_strstrip(aline);
      manpath = g_strsplit(aline, ":", -1);
    }
  else
    {
      g_warning("Couldn't get manpath");
    }
  pclose(fh);

  i = 0;
  if(manpath)
    {
      for(; manpath[i]; i++)
	fmt_man_populate_tree_for_dir(tree, manpath[i]);
    }
  if(!manpath || !manpath[i])
    fmt_man_populate_tree_for_dir(tree, "/usr/man");

  fmt_free_tree_info(tree);
}

/***** info pages *****/
static void
fmt_info_populate_tree_for_subdir(HyperbolaDocTree *tree, const char *basedir, char **defpath)
{
  DIR *dirh;
  struct dirent *dent;

  dirh = opendir(basedir);
  if(!dirh)
    return;

  readdir(dirh); /* skip . & .. */
  readdir(dirh);

  while((dent = readdir(dirh)))
    {
      char *ctmp = NULL;
      char **thispath;
      char uribuf[128], titlebuf[128];

      if(dent->d_name[0] == '.')
	continue;

      do {
	if(ctmp) *ctmp = '\0';
	ctmp = strrchr(dent->d_name, '.');
      } while(ctmp && strcmp(ctmp, ".info"));

      if(!ctmp)
	continue;

      *ctmp = '\0';

      strcpy(titlebuf, dent->d_name);
      strcat(titlebuf, " (info)");

      g_snprintf(uribuf, sizeof(uribuf), "info:%s", dent->d_name);

      thispath = fmt_map_entry(tree, dent->d_name, '!'); /* Yes, we use the manpage mapping stuff just
							    because it is easier. */

      if(thispath)
	make_treesection(tree, thispath);

      hyperbola_doc_tree_add(tree, HYP_TREE_NODE_PAGE, (const char **)(thispath?thispath:defpath), titlebuf, uribuf);
    }

  closedir(dirh);

}

static void
fmt_info_populate_tree(HyperbolaDocTree *tree)
{
  char *defpath[] = { N_("Info"), NULL };

  translate_array(defpath);
  make_treesection(tree, defpath);

  fmt_info_populate_tree_for_subdir(tree, "/usr/info", defpath);
  if(strcmp(INFODIR, "/usr/info"))
    fmt_info_populate_tree_for_subdir(tree, INFODIR, defpath);

  fmt_free_tree_info(tree);
}

/******* help: ******/
static void
fmt_help_populate_tree_from_subdir(HyperbolaDocTree *tree, const char *dirname, char **defpath)
{
  DIR *dirh;
  struct dirent *dent;
  char *subpath[10];
  int i;
  GList *langlist;

  dirh = opendir(dirname);

  if(!dirh)
    return;

  readdir(dirh); /* skip . & .. */
  readdir(dirh);

  for(i = 0; defpath[i]; i++)
    subpath[i] = defpath[i];
  subpath[i+1] = NULL;

  langlist = gnome_i18n_get_language_list(NULL);

  while((dent = readdir(dirh)))
    {
      char afile[PATH_MAX], aline[LINE_MAX], uribuf[128];
      FILE *fh;
      GList *cur;

      g_snprintf(afile, sizeof(afile), "%s/%s/C/index.html", dirname, dent->d_name);
      if(!g_file_exists(afile))
	continue;

      g_snprintf(uribuf, sizeof(uribuf), "help:%s", dent->d_name);

      hyperbola_doc_tree_add(tree, HYP_TREE_NODE_BOOK, (const char **)defpath, dent->d_name, uribuf);

      subpath[i] = dent->d_name;

      for(cur = langlist; cur; cur = cur->next)
	{
	  /* XXX fixme for gnome-libs 2.0 */
	  g_snprintf(afile, sizeof(afile), "%s/%s/%s/topic.dat", dirname, dent->d_name, (char *)cur->data);
	  if(g_file_exists(afile))
	    break;
	}
      if(!cur)
	g_snprintf(afile, sizeof(afile), "%s/%s/C/topic.dat", dirname, dent->d_name);

      fh = fopen(afile, "r");
      if(!fh)
	continue;

      while(fgets(aline, sizeof(aline), fh))
	{
	  char **pieces;

	  g_strstrip(aline);
	  if(!aline[0] || aline[0] == '#')
	    continue;

	  pieces = g_strsplit(aline, " ", 2);

	  if(pieces && pieces[0] && pieces[1])
	    {
	      char *ctmp = strrchr(pieces[0], '.'), *ctmp2;

	      if(ctmp && !strcmp(ctmp, ".html"))
		{
		  *ctmp = '\0';
		  ctmp++;
		}
	      else
		ctmp = NULL;

	      g_snprintf(uribuf, sizeof(uribuf), "help:%s/%s", dent->d_name, pieces[0]);

	      if(ctmp)
		{
		  ctmp2 = strchr(ctmp, '#');
		  if(ctmp2)
		    {
		      ctmp2++;
		      strcat(uribuf, "/");
		      strcat(uribuf, ctmp2);
		    }
		}

	      hyperbola_doc_tree_add(tree, HYP_TREE_NODE_PAGE, (const char **)subpath, pieces[1], uribuf);
	    }
	  g_strfreev(pieces);
	}

      fclose(fh);
    }

  closedir(dirh);
}

static void
fmt_help_populate_tree(HyperbolaDocTree *tree)
{
  char *app_path[] = {N_("Applications"), NULL};
  char *dirname;

  translate_array(app_path);
  make_treesection(tree, app_path);

  dirname = gnome_datadir_file("gnome/help");

  if(dirname)
    fmt_help_populate_tree_from_subdir(tree, dirname, app_path);
  g_free(dirname);
}
