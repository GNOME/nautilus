#include <libnautilus/libnautilus.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <string.h>
#include <limits.h>

#if 0
typedef struct {
  NautilusViewFrame *view_frame;

  GtkWidget *clist, *ent;

  GTree *all_items;

  gint8 notify_count;
} HyperbolaNavigationIndex;

typedef enum { PRIMARY, SECONDARY, SEE, SEEALSO, NONE, T_LAST=NONE } ItemType;

typedef struct {
  char *text, *motext, *uri;
  GTree *subitems;
  ItemType type : 7;
  gboolean shown : 1;
} IndexItem;

static void
hyperbola_navigation_index_update_clist(HyperbolaNavigationIndex *hni)
{
  char *stxt, *tmp_stxt;
  char *words[100];
  int nwords;
  char *ctmp = NULL;
  int tmp_len;
  int i;

  stxt = gtk_entry_get_text(GTK_ENTRY(hni->ent));

  tmp_len = strlen(stxt)+1;
  tmp_stxt = alloca(tmp_len);
  memcpy(tmp_stxt, stxt, tmp_len);
  for(nwords = 0; (ctmp = strtok(tmp_stxt, ", \t")) && nwords < sizeof(words)/sizeof(words[0]); nwords++)
    words[nwords] = ctmp;

  gtk_clist_freeze(GTK_CLIST(hni->clist));
  gtk_clist_clear(GTK_CLIST(hni->clist));

  for(i = 0; i < hni->items->len; i++)
    {
      int j, rownum, uplevel;
      char rowtext[512];
      IndexItem *ii = g_ptr_array_index(hni->items, i);

      for(j = 0; j < nwords; j++)
	{
	  if(strstr(ii->text, words[i]))
	    break;
	}

      ii->shown = !(nwords && j >= nwords);

      if(!ii->shown)
	continue;

      j = i;
      for(uplevel = ii->indent - 1; uplevel >= 0; uplevel--)
	{
	  IndexItem *previi;

	  for(; j >= 0; j--)
	    {
	      previi = &g_array_index(hni->items, IndexItem, j);
	      if(previi->indent == uplevel)
		break;
	    }

	  if(j < 0)
	    break;

	  if(!previi->shown)
	    {
	      /* Figure out the right place to insert the row */
	    }
	}

      g_snprintf(rowtext, sizeof(rowtext), "%*s%s", ii->indent * 2, "", ii->text); /* Lame way of indenting entries */
      rownum = gtk_clist_append(GTK_CLIST(hni->clist), (char **)&rowtext);
      gtk_clist_set_row_data(GTK_CLIST(hni->clist), rownum, ii);

      if(nwords) /* highlight this row as a match */
	{
	  GdkColor c;

	  c.red = c.green = 65535;
	  c.blue = 20000;
	  gdk_color_alloc(gdk_rgb_get_cmap(), &c);
	  gtk_clist_set_background(GTK_CLIST(hni->clist), rownum, &c);
	}
    }

  gtk_clist_thaw(GTK_CLIST(hni->clist));
}

static void
hyperbola_navigation_index_ent_changed(GtkWidget *ent, HyperbolaNavigationIndex *hni)
{
  hyperbola_navigation_index_update_clist(hni);
}

static void
hyperbola_navigation_index_ent_activate(GtkWidget *ent, HyperbolaNavigationIndex *hni)
{
}

static void
hyperbola_navigation_index_select_row(GtkWidget *clist, gint row, gint column, GdkEvent *event, HyperbolaNavigationIndex *hni)
{
  IndexItem *ii;
  Nautilus_NavigationRequestInfo loc;

  if(!event || event->type != GDK_2BUTTON_PRESS) /* we only care if the user has double-clicked on an item...? */
    return;

  ii = gtk_clist_get_row_data(GTK_CLIST(clist), row);
  memset(&loc, 0, sizeof(loc));
  loc.requested_uri = ii->uri;
  loc.new_window_default = loc.new_window_suggested = loc.new_window_enforced = Nautilus_V_UNKNOWN;
  nautilus_view_frame_request_location_change(NAUTILUS_VIEW_FRAME(hni->view_frame), &loc);
}

typedef struct {
  HyperbolaNavigationIndex *idx;
  char *appname, *filename;

  char *idx_ref;
  int in_term;
  GString *sub_text;
  ItemType sub_type;
  char *stinfo[NONE];
} SAXParseInfo;

static xmlEntityPtr
get_entity (SAXParseInfo *spi, const gchar *name)
{
#ifdef ERROR_OUTPUT
	g_print ("in getEntity:%s\n", name);
#endif

	return xmlGetPredefinedEntity (name);
}

static void
start_document (SAXParseInfo *spi)
{
}

static void
end_document (SAXParseInfo *spi)
{
}

static void
start_element(SAXParseInfo *spi,
	      const gchar *name,
	      const xmlChar **attrs)
{
  if(!strcasecmp(name, "indexterm"))
    {
      int i;
      char *ctmp;

      for(i = 0; attrs[i]; i++)
	{
	  if(!strncasecmp(attrs[i], "id=", 3))
	    break;
	}

      g_return_if_fail(attrs[i]);

      spi->in_term++;
      g_string_assign(spi->sub_text, "");
      for(i = PRIMARY; i < NONE; i++)
	spi->stinfo[i] = NULL;
      spi->sub_type = NONE;

      return;
    }
  if(!spi->in_term || spi->sub_type != NONE)
    return;

  if(!strcasecmp(name, "primary"))
    {
      spi->sub_type = PRIMARY;
    }
  else if(!strcasecmp(name, "secondary"))
    {
      spi->sub_type = SECONDARY;
    }
  else if(!strcasecmp(name, "seealso"))
    {
      spi->sub_type = SEEALSO;
    }
  else if(!strcasecmp(name, "see"))
    {
      spi->sub_type = SEE;
    }
  else
    spi->sub_type = NONE;

  g_return_if_fail(spi->sub_type == NONE || !spi->stinfo[spi->sub_type]);
}

static void
characters (SAXParseInfo *spi,
	    const gchar *chars,
	    int len)
{
  g_return_if_fail(spi->sub_type != NONE);

  g_string_sprintfa(spi->sub_text, "%.*s", len, chars);
}

static gint
index_item_compare(IndexItem *i1, IndexItem *i2)
{
  return strcasecmp(i1->text, i2->text);
}

static void
end_element (SAXParseInfo *spi,
	     const gchar *name)
{
  int this_type;

  if(!strcasecmp(name, "indexterm"))
    {
      int i;
      IndexItem *parent_ii = NULL, *ii = NULL;
      ItemType it;
      GTree *parent_tree = NULL;

      /* Algorithm:
	 If this is a see (also), we have to list it on the top level, so make the node.
	 If this is a secondary, we have to first find/make the primary.
       */

      spi->in_term--;

      g_return_if_fail(spi->stinfo[PRIMARY]);

      if(spi->stinfo[SECONDARY] || spi->stinfo[SEE] || spi->stinfo[SEEALSO])
	{
	  parent_ii = g_tree_lookup(spi->idx->all_items, spi->stinfo[PRIMARY]);

	  if(!parent_ii)
	    {
	      parent_ii = g_new0(IndexItem, 1);
	      parent_ii->type = PRIMARY;
	      parent_ii->text = g_strdup(spi->stinfo[PRIMARY]);
	      g_tree_insert(spi->idx->items_by_text, parent_ii->text, parent_ii);
	    }

	  if(!parent_ii->subitems)
	    parent_ii->subitems = g_tree_new(strcasecmp);
	  parent_tree = parent_ii->subitems;

	  it = SECONDARY;

	  if(spi->stinfo[SECONDARY] && (spi->stinfo[SEE] || spi->stinfo[SEEALSO]))
	    {
	      /* Make a second layer */

	      ii = g_tree_lookup(parent_tree, spi->stinfo[PRIMARY]);

	      if(!ii)
		{
		  ii = g_new0(IndexItem, 1);
		  ii->type = SECONDARY;
		  ii->text = g_strdup(spi->stinfo[SECONDARY]);
		  g_tree_insert(parent_tree, ii->text, ii);
		}

	      if(!ii->subitems)
		ii->subitems = g_tree_new(strcasecmp);
	      parent_ii = ii;
	      parent_tree = parent_ii->subitems;

	      it = spi->stinfo[SEE]?SEE:SEEALSO;
	    }
	}
      else
	{
	  it = PRIMARY;
	  parent_tree = spi->idx->all_items;
	}

      ii = g_tree_lookup(parent_tree, spi->stinfo[it]);
      if(!ii)
	{
	  ii = g_new0(IndexItem, 1);
	  ii->text = g_strdup(spi->stinfo[it]);
	  g_tree_insert(parent_tree, ii->text, ii);
	  ii->type = it;
	}
      if(!ii->uri)
	ii->uri = g_strdup_printf("help:%s/%s/%s", spi->appname, spi->filename, spi->idx_ref);
      g_assert(ii->type == it);

      if(spi->stinfo[SECONDARY])
	{
	  /* Also insert a top-level node that gives info on this secondary node */
	  
	  char buf[512];

	  g_snprintf(buf, sizeof(buf), "%s, %s",
		     spi->stinfo[SECONDARY],
		     spi->stinfo[PRIMARY]);
	  if(spi->stinfo[SEE] || spi->stinfo[SEEALSO])
	    {
	      if(spi->stinfo[SEE])
		{
		  it = SEE;
		  txt = spi->stinfo[SEE];
		  strcat(buf, _(" (see \""));
		}
	      else
		{
		  txt = spi->stinfo[SEEALSO];
		  strcat(buf, _(" (see also \""));
		}

	      strcat(buf, spi->stinfo[it]);
	      strcat(buf, ")");
	    }
	  parent_tree = spi->idx->all_items;

	  ii = g_tree_lookup(parent_tree, spi->stinfo[it]);
	  if(!ii)
	    {
	      ii = g_new0(IndexItem, 1);
	      ii->text = g_strdup(buf);
	      g_tree_insert(parent_tree, ii->text, ii);
	      ii->type = it;
	    }

	  if(!ii->uri)
	    ii->uri = g_strdup_printf("help:%s/%s/%s", spi->appname, spi->filename, spi->idx_ref);
	  g_assert(ii->type == it);
	}

      for(i = PRIMARY; i < NONE; i++)
	{
	  g_free(spi->stinfo[i]);
	  spi->stinfo[i] = NULL;
	}

      return;
    }

  if(!spi->in_term || spi->sub_type == NONE)
    return;

  if(!strcasecmp(name, "primary"))
    {
      this_type = PRIMARY;
    }
  else if(!strcasecmp(name, "secondary"))
    {
      this_type = SECONDARY;
    }
  else if(!strcasecmp(name, "seealso"))
    {
      this_type = SEEALSO;
    }
  else if(!strcasecmp(name, "see"))
    {
      this_type = SEE;
    }
  else
    this_type = NONE;

  g_return_if_fail(this_type != NONE && !spi->stinfo[this_type] && this_type == spi->sub_type);

  spi->stinfo[this_type] = g_strdup(spi->sub_text->str);
  spi->sub_type = NONE;
}

static void
handle_error (SAXParseInfo *spi, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
#ifdef ERROR_OUTPUT
	g_logv("XML", G_LOG_LEVEL_CRITICAL, msg, args);
#endif
	va_end(args);
}

static void
handle_fatal_error (SAXParseInfo *spi, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
#ifdef ERROR_OUTPUT
	g_logv("XML", G_LOG_LEVEL_ERROR, msg, args);
#endif
	va_end(args);
}

static xmlSAXHandler sax = {
  NULL,  /* internalSubset */
  NULL, /* isStandalone */
  NULL, /* hasInternalSubset */
  NULL, /* hasExternalSubset */
  NULL, /* resolveEntity */
  (getEntitySAXFunc) get_entity, /* getEntity */
  NULL, /* entityDecl */
  NULL, /* notationDecl */
  NULL, /* attributeDecl */
  NULL, /* elementDecl */
  NULL, /* unparsedEntityDecl */
  NULL, /* setDocumentLocator */
  (startDocumentSAXFunc) start_document, /* startDocument */
  (endDocumentSAXFunc) end_document, /* endDocument */
  (startElementSAXFunc) start_element, /* startElement */
  (endElementSAXFunc) end_element, /* endElement */
  NULL, /* reference */
  (charactersSAXFunc) characters, /* characters */
  NULL, /* ignorableWhitespace */
  NULL, /* processingInstruction */
  NULL, /* comment */
  NULL, /* warning */
  (errorSAXFunc) handle_error, /* error */
  (fatalErrorSAXFunc) handle_fatal_error, /* fatalError */
  NULL, /* parameterEntity */
  NULL, /* cdata block */
};

static void
hyperbola_navigation_index_read_app(HyperbolaNavigationIndex *idx, const char *topdir, const char *appname)
{
  SAXParseInfo spi;
  DIR *dirh;
  struct dirent *dent;

  memset(&spi, 0, sizeof(spi));
  spi.idx = idx;
  spi.appname = appname;
  spi.sub_text = g_string_new(NULL);

  dirh = opendir(topdir);
  if(!dirh)
    return;

  while((dent = readdir(dirh)))
    {
      char *ctmp;
      char buf[PATH_MAX];
      ctmp = strrchr(dent->d_name, '.');
      if(!ctmp || strcmp(ctmp, '.sgml'))
	continue;

      g_snprintf(buf, sizeof(buf), "%s/%s", topdir, dent->d_name);
      *ctmp = '\0';
      spi.filename = dent->d_name;
      xmlSAXUserParseFile(sax, &spi, buf);
    }
  closedir(dirh);

  g_string_free(spi.sub_text, TRUE);
}

static void
hyperbola_navigation_index_read(HyperbolaNavigationIndex *idx, const char *topdir)
{
  DIR *dirh;
  struct dirent *dent;
  GList *langlist;

  langlist = gnome_i18n_get_language_list(NULL);

  dirh = opendir(topdir);
  if(!dirh)
    return;

  while((dent = readdir(dirh)))
    {
      char buf[PATH_MAX];
      GList *ltmp;

      if(dent->d_name[0] == '.')
	continue;

      for(ltmp = langlist; ltmp; ltmp = ltmp->next)
	{
	  g_snprintf(buf, sizeof(buf), "%s/%s/%s", topdir, dent->d_name, ltmp->data);
	  if(g_file_test(buf, G_FILE_TEST_ISDIR))
	    break;
	}

      if(!ltmp)
	continue;

      hyperbola_navigation_index_read_app(idx, buf, dent->d_name);
    }

  closedir(dirh);
}
#endif

BonoboObject *hyperbola_navigation_index_new(void)
{
#if 0
  HyperbolaNavigationIndex *hni;
  GtkWidget *wtmp;

  hni = g_new0(HyperbolaNavigationIndex, 1);
  hni->items = g_array_new(FALSE, FALSE, sizeof(IndexItem));

  hyperbola_navigation_index_read();

  hni->ent = gtk_entry_new();
  gtk_signal_connect(GTK_OBJECT(hni->ent), "changed", hyperbola_navigation_index_ent_changed, hni);
  gtk_signal_connect(GTK_OBJECT(hni->ent), "activate", hyperbola_navigation_index_ent_activate, hni);
  gtk_widget_show(hni->ent);

  hni->clist = gtk_clist_new(1);
  gtk_clist_freeze(GTK_CLIST(hni->clist));
  gtk_clist_set_selection_mode(GTK_CLIST(hni->clist), GTK_SELECTION_BROWSE);
  gtk_clist_set_column_widget(GTK_CLIST(hni->clist), 0, hni->ent);
  gtk_signal_connect(GTK_OBJECT(hni->clist), "select_row", hyperbola_navigation_index_select_row, hni);

  wtmp = gtk_scrolled_window_new(gtk_clist_get_hadjustment(GTK_CLIST(hni->clist)),
				 gtk_clist_get_vadjustment(GTK_CLIST(hni->clist)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(wtmp), hni->clist);
  hyperbola_navigation_index_update_clist(hni);
  gtk_clist_columns_autosize(GTK_CLIST(hni->clist));
  gtk_clist_thaw(GTK_CLIST(hni->clist));
  gtk_widget_show(hni->clist);
  gtk_widget_show(wtmp);

  hni->view_frame = NAUTILUS_VIEW_FRAME (nautilus_meta_view_frame_new (wtmp));
  nautilus_meta_view_frame_set_label(NAUTILUS_META_VIEW_FRAME(hni->view_frame), _("Help Index"));

  return BONOBO_OBJECT (hni->view_frame);
#else
  return NULL;
#endif
}
