#include <libnautilus/libnautilus.h>
#include <gnome.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <string.h>
#include <limits.h>
#include <gnome-xml/parser.h>
#include <dirent.h>

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

typedef struct {
  HyperbolaNavigationIndex *hni;
  char *words[50];
  int nwords;
} CListCreationInfo;

static gint
hyperbola_navigation_index_show_item(const char *key, IndexItem *ii, CListCreationInfo *cci)
{
  int rownum, indent;
  char rowtext[512], *textptr;
  HyperbolaNavigationIndex *hni = cci->hni;

  if(ii->type != PRIMARY)
    {
      g_warning("Subitems not yet handled!");
      return 0;
    }

  switch(ii->type)
    {
    case PRIMARY:
      indent = 0;
      break;
    case SEEALSO:
    case SEE:
    case SECONDARY:
      indent = 2;
      break;
    default:
      g_assert_not_reached();
      break;
    }

  g_snprintf(rowtext, sizeof(rowtext), "%*s%s", indent * 2, "", ii->text); /* Lame way of indenting entries */
  textptr = rowtext;
  rownum = gtk_clist_append(GTK_CLIST(hni->clist), &textptr);
  gtk_clist_set_row_data(GTK_CLIST(hni->clist), rownum, ii);

  if(cci->nwords) /* highlight this row as a match */
    {
      GdkColor c;

      c.red = c.green = 65535;
      c.blue = 20000;
      gdk_color_alloc(gdk_rgb_get_cmap(), &c);
      gtk_clist_set_background(GTK_CLIST(hni->clist), rownum, &c);
    }

  return 0;
}

static void
hyperbola_navigation_index_update_clist(HyperbolaNavigationIndex *hni)
{
  CListCreationInfo cci;
  char *stxt, *tmp_stxt;
  char *ctmp = NULL;
  int tmp_len;

  stxt = gtk_entry_get_text(GTK_ENTRY(hni->ent));

  memset(&cci, 0, sizeof(cci));
  cci.hni = hni;

  tmp_len = strlen(stxt)+1;
  tmp_stxt = alloca(tmp_len);
  memcpy(tmp_stxt, stxt, tmp_len);
  for(cci.nwords = 0; (ctmp = strtok(tmp_stxt, ", \t")) && cci.nwords < sizeof(cci.words)/sizeof(cci.words[0]); cci.nwords++)
    cci.words[cci.nwords] = ctmp;

  gtk_clist_freeze(GTK_CLIST(hni->clist));
  gtk_clist_clear(GTK_CLIST(hni->clist));

  g_tree_traverse(hni->all_items, (GTraverseFunc)hyperbola_navigation_index_show_item, G_IN_ORDER, &cci);

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

      for(i = 0; attrs[i]; i++)
	{
	  if(!strcasecmp(attrs[i], "id"))
	    {
	      i++;
	      break;
	    }
	}

      g_return_if_fail(attrs[i]);

      spi->idx_ref = g_strdup(attrs[i]);
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
  if(spi->sub_type == NONE)
    return;

  g_string_sprintfa(spi->sub_text, "%.*s", len, chars);
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
	      g_tree_insert(spi->idx->all_items, parent_ii->text, parent_ii);
	    }

	  if(!parent_ii->subitems)
	    parent_ii->subitems = g_tree_new((GCompareFunc)strcasecmp);
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
		ii->subitems = g_tree_new((GCompareFunc)strcasecmp);
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
	  char *txt;

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
	      ii->text = g_strdup(txt);
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
      g_free(spi->idx_ref); spi->idx_ref = NULL;

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
  spi.appname = (char *)appname;
  spi.sub_text = g_string_new(NULL);

  dirh = opendir(topdir);
  if(!dirh)
    return;

  while((dent = readdir(dirh)))
    {
      char *ctmp;
      char buf[PATH_MAX];
      ctmp = strrchr(dent->d_name, '.');
      if(!ctmp || strcmp(ctmp, ".sgml"))
	continue;

      g_snprintf(buf, sizeof(buf), "%s/%s", topdir, dent->d_name);
      g_message("Let's try %s", buf);
      *ctmp = '\0';
      spi.filename = dent->d_name;

      xmlSAXUserParseFile(&sax, &spi, buf);
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
	  g_snprintf(buf, sizeof(buf), "%s/%s/%s", topdir, dent->d_name, (char *)ltmp->data);
	  if(g_file_test(buf, G_FILE_TEST_ISDIR))
	    break;
	}

      if(!ltmp)
	continue;

      hyperbola_navigation_index_read_app(idx, buf, dent->d_name);
    }

  closedir(dirh);
}

BonoboObject *hyperbola_navigation_index_new(void)
{
  HyperbolaNavigationIndex *hni;
  GtkWidget *wtmp, *vbox;

  hni = g_new0(HyperbolaNavigationIndex, 1);
  hni->all_items = g_tree_new((GCompareFunc)strcasecmp);

  hyperbola_navigation_index_read(hni, "/gnome/share/gnome/help");

  vbox = gtk_vbox_new(FALSE, GNOME_PAD);

  hni->ent = gtk_entry_new();
  gtk_signal_connect(GTK_OBJECT(hni->ent), "changed", hyperbola_navigation_index_ent_changed, hni);
  gtk_signal_connect(GTK_OBJECT(hni->ent), "activate", hyperbola_navigation_index_ent_activate, hni);
  gtk_container_add(GTK_CONTAINER(vbox), hni->ent);

  hni->clist = gtk_clist_new(1);
  gtk_clist_freeze(GTK_CLIST(hni->clist));
  gtk_clist_set_selection_mode(GTK_CLIST(hni->clist), GTK_SELECTION_BROWSE);
  gtk_clist_set_column_widget(GTK_CLIST(hni->clist), 0, hni->ent);
  gtk_signal_connect(GTK_OBJECT(hni->clist), "select_row", hyperbola_navigation_index_select_row, hni);

  wtmp = gtk_scrolled_window_new(gtk_clist_get_hadjustment(GTK_CLIST(hni->clist)),
				 gtk_clist_get_vadjustment(GTK_CLIST(hni->clist)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(wtmp), hni->clist);
  gtk_container_add(GTK_CONTAINER(vbox), wtmp);

  hyperbola_navigation_index_update_clist(hni);

  gtk_clist_columns_autosize(GTK_CLIST(hni->clist));
  gtk_clist_thaw(GTK_CLIST(hni->clist));
  gtk_widget_show_all(vbox);

  hni->view_frame = NAUTILUS_VIEW_FRAME (nautilus_meta_view_frame_new (vbox));
  nautilus_meta_view_frame_set_label(NAUTILUS_META_VIEW_FRAME(hni->view_frame), _("Help Index"));

  return BONOBO_OBJECT (hni->view_frame);
}
