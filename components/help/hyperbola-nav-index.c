#include <libnautilus/libnautilus.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <gnome.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <string.h>
#include <limits.h>
#include <libxml/parser.h>
#include <dirent.h>
#include <ctype.h>
#include "hyperbola-nav.h"

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
#include <gtk/gtkhseparator.h>


typedef enum { PRIMARY, SECONDARY, TERTIARY, SEE, SEEALSO, NONE } ItemType;

#else
typedef struct {
        NautilusView *view_frame;

        GtkWidget *clist, *ent;

        GTree *all_items;

        gint8 notify_count;
} HyperbolaNavigationIndex;

typedef enum { PRIMARY, SECONDARY, SEE, SEEALSO, NONE, T_LAST =

                NONE } ItemType;
#endif

typedef struct {
	char *text, *uri; 
	GTree *subitems;
        gboolean shown:1;
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	GTree *ref_subitems;
	GTree *duplicates;
	ItemType type;
	gboolean xref;
	char *index_id;
#else
	ItemType type:6;
        gboolean xref:1;
#endif
} IndexItem;

typedef struct {
	HyperbolaNavigationIndex *hni;
	char *words[50];
	int nwords;
	gboolean did_select;
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	char *indexid;
	int current_rownum;
#endif
} CListCreationInfo;

typedef struct {
	CListCreationInfo *cci;
	int indent, matches;
	int super_matches[50];
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	int full_matches;
	gboolean highlight;
#endif
} MoreCListInfo;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT

typedef struct {
	HyperbolaNavigationIndex *hni;
	HyperbolaIndexSelection index_type;
} IndexFiles;

typedef struct {
	IndexFiles *idx_files;
} MoreIndexFiles;
#endif

typedef struct {
	HyperbolaNavigationIndex *idx;
	GString *sub_text;
	ItemType sub_type;
	char *stinfo[NONE];
	int in_term;
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	char *doc_uri, *indexfile, *separator;
	char *index_ids[NONE];
	GSList *idx_refs[NONE];
	GTree *parent_tree;
	GTree *index_items;
	int in_indexitem;
#else
	char *idx_ref, *appname, *filename;
#endif
} SAXParseInfo;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
static int select_handler_id;
static void
make_index_select_page(GtkWidget *button, HyperbolaNavigationIndex *hni);
static void
create_index_select_page(HyperbolaNavigationIndex *hni);
static GSList *
hash_table_get_keys (GHashTable *hash_table);
static void
clear_index_hashtable (HyperbolaNavigationIndex *hni); 
static void
hyperbola_navigation_index_create_clist (HyperbolaNavigationIndex * hni);
static void
hyperbola_navigation_index_OK_button(GtkWidget *button, HyperbolaNavigationIndex *hni);
static void
hyperbola_navigation_index_select_all_button(GtkWidget *button, HyperbolaNavigationIndex *hni);
static void
hyperbola_navigation_index_select_none_button(GtkWidget *button, HyperbolaNavigationIndex *hni);
static void
hyperbola_navigation_index_cancel_button(GtkWidget *button, HyperbolaNavigationIndex *hni);
static void
redraw_index_display_page(HyperbolaNavigationIndex *hni);
static gboolean 
index_ctree_populate_subnode (gpointer key, gpointer value, gpointer user_data);
static void 
index_ctree_populate(HyperbolaNavigationIndex *hni);
static void 
process_index_term(SAXParseInfo *spi, ItemType it);
static void 
hyperbola_navigation_index_show_selected_terms (HyperbolaNavigationIndex *hni);
static void
hyperbola_navigation_index_parse_index_files (HyperbolaNavigationIndex *hni, 
						HyperbolaIndexSelection index_type);
static void
hyperbola_navigation_index_find_contents_selected_index_files(HyperbolaNavigationIndex *hni);
static void
hyperbola_navigation_index_tree_select_row (GtkCTree * ctree,
			   								GtkCTreeNode * node,
											gint column,
											HyperbolaNavigationIndex * hni);
static void
hyperbola_navigation_index_tree_unselect_row (GtkCTree * ctree, 
										GtkCTreeNode * node,
										gint column,
										HyperbolaNavigationIndex * hni);

/*
 * hyperbola_navigation_index_show_button
 * @GtkWidget *button : the button pointer
 * @HyperbolaNavigationIndex *hni: the pointer to the HyperbolaNavigationIndex
 * activated when the "Show" button is clicked on index_display_page.
 * it displays the specified index terms for the specified documents.
 */

static void 
hyperbola_navigation_index_show_button(GtkWidget *button, HyperbolaNavigationIndex *hni)
{

      /*
       * determines which document collection we are dealing with by 
       * the value of hni->index_type.
       * If hni->show_all_terms is true
       */
      switch(hni->index_type){
		case CONTENTS_SELECTED_INDEX_FILES:
			if(hni->show_all_terms){
                       		hyperbola_navigation_index_create_clist(hni);
			} else{
				hyperbola_navigation_index_show_selected_terms
				(hni);
			}
                       	break;
                case INDEX_SELECTED_INDEX_FILES:
			if(hni->show_all_terms){
                       		hyperbola_navigation_index_create_clist(hni);
			} else{
				hyperbola_navigation_index_show_selected_terms
				(hni);
			}
                       	break;
               	case ALL_INDEX_FILES:
			if(hni->show_all_terms){
                       		hyperbola_navigation_index_create_clist(hni);
			} else{
				hyperbola_navigation_index_show_selected_terms
				(hni);
			}
                       	break;
        };

}

/*
 * hyperbola_navigation_index_OK_button
 * @GtkWidget *button : pointer to the OK button on the index_select_page 
 * @HyperbolaNavigationIndex *hni
 * Reads the index files for the selected documents.
 * If the "Specific documents" radio button is on, then new index list is
 * created. 
 * Changes display to index_display_page.
 */
static void 
hyperbola_navigation_index_OK_button(GtkWidget *button, HyperbolaNavigationIndex *hni)
{
	hyperbola_navigation_index_parse_index_files(hni,
						    INDEX_SELECTED_INDEX_FILES);
	if(hni->index_type == INDEX_SELECTED_INDEX_FILES){
		hyperbola_navigation_index_create_clist(hni);
	}
	redraw_index_display_page(hni);
}

/*
 * hyperbola_navigation_index_select_all_button
 * @GtkWidget *button : pointer to the "Select All" button on the index_select_page 
 * @HyperbolaNavigationIndex *hni
 * Selects all the rows in the index_select_page display.
 * Finds all the index files (from
 * hni->index_contents->doc_tree->all_index_files), and adds each file to 
 * hni->index_selected_index_files (if not already present).
 */
static void 
hyperbola_navigation_index_select_all_button(GtkWidget *button, HyperbolaNavigationIndex *hni)
{ 
	GSList *keys = NULL, *tmp = NULL;
	char *index_file;
	
	gtk_clist_select_all(GTK_CLIST(hni->index_contents->ctree));
	keys = hash_table_get_keys(hni->index_contents->doc_tree->all_index_files);
	tmp = keys;

	while (tmp != NULL){
		if(!g_hash_table_lookup(hni->index_selected_index_files,
					          (char *)tmp->data)){
			index_file = g_strdup(g_hash_table_lookup(hni->index_contents->doc_tree->all_index_files,
					          (char *)tmp->data));
			g_hash_table_insert(hni->index_selected_index_files,
						g_strdup((char *)tmp->data), index_file);
		}
		tmp = g_slist_next(tmp);
	}
}

/*
 * hyperbola_navigation_index_select_none_button
 * @GtkWidget *button : pointer to the "Select None" button on the index_select_page 
 * @HyperbolaNavigationIndex *hni
 * Unselects all the rows in the index_select_page display.
 * Clears all the entries in hni->index_selected_index_files.
 */
static void 
hyperbola_navigation_index_select_none_button(GtkWidget *button, HyperbolaNavigationIndex *hni)
{
	gtk_clist_unselect_all(GTK_CLIST(hni->index_contents->ctree));
	clear_index_hashtable(hni);
}

/*
 * clear_index_hashtable
 * @HyperbolaNavigationIndex *hni
 * Finds all the keys to the hni->index_selected_index_files.
 * Uses these keys to lookup and deleted all entries, leaving
 * hni->index_selected_index_files empty.
 */
static void
clear_index_hashtable(HyperbolaNavigationIndex *hni)
{
	GSList *keys = NULL, *tmp = NULL;
	char *doc_uri, *value;
	keys = hash_table_get_keys(hni->index_selected_index_files);
	tmp = keys;

	while (tmp != NULL){
		doc_uri = (char *)tmp->data;
		value = g_hash_table_lookup(hni->index_selected_index_files,
						doc_uri);
		if(value != NULL){
			g_hash_table_remove(hni->index_selected_index_files,
						doc_uri);
			g_free(value);
		}
		g_free(doc_uri);
		tmp = g_slist_next(tmp);
	}
}
/*
 * hyperbola_navigation_index_cancel_button
 * @GtkWidget *button : pointer to the "Cancel" button on the index_select_page 
 * @HyperbolaNavigationIndex *hni
 * Unselects all rows on the index_select_page display.
 * Clears all entries in hni->index_select_index_page
 * Displays the index_display_page
 */

static void 
hyperbola_navigation_index_cancel_button(GtkWidget *button, HyperbolaNavigationIndex *hni)
{
	gtk_clist_unselect_all(GTK_CLIST(hni->index_contents->ctree));
	clear_index_hashtable(hni);
	redraw_index_display_page(hni);
}
#endif 

static gboolean
text_matches (const char *text, const char *check_for)
{
	const char *ctmp;
	int check_len;

	if (!check_for || !*check_for)
		return FALSE;

	ctmp = text;
	check_len = strlen (check_for);

	while (*ctmp) {
		if (tolower (*ctmp) == tolower (*check_for)) {
			if (!strncasecmp (ctmp, check_for, check_len))
				return TRUE;
		}

		ctmp++;
	}

	if (strstr (text, check_for)) {
		static volatile int barrier;
		while (barrier);
	}

	return FALSE;
}

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/*
 * hyperbola_navigation_index_show_duplicate_index_item
 * @const char *key: the key to the item in the subitems tree
 * @IndexItem *ii: the value of the item in the subitems tree
 * @ MoreCListInfo *mci: data required for processing
 * This function is called from hyperbola_navigation_index_show_index_item when
 * using g_tree_traverse to traverse an entry's subitems.
 * This function will display the subitem in the index terms display
 * Returns 0 (indicating to continue traversing tree)
 */
static gint
hyperbola_navigation_index_show_duplicate_index_item(const char *key, 
													IndexItem *ii,
													MoreCListInfo *mci)
{
	int rownum;
	char *rowtext, *tmp_text=NULL, *output_text=NULL;
	CListCreationInfo *cci=mci->cci;
	HyperbolaNavigationIndex *hni = cci->hni;

	tmp_text=g_strdup(ii->text);
	output_text = strtok(tmp_text, "#");
	if (output_text == NULL)
		output_text = g_strdup(ii->text);
	
	rowtext = g_strdup_printf ("%*s%s", (mci->indent - 1) * 5, "", output_text);
	rownum = gtk_clist_append (GTK_CLIST (hni->clist), &rowtext);
	gtk_clist_set_row_data (GTK_CLIST (hni->clist), rownum, ii);

	return 0;

}
/*
 * hyperbola_navigation_index_show_reference_item
 * @const char *key: the key to the item in the index tree
 * @IndexItem *ii: the value of the item in the index tree
 * @ MoreCListInfo *mci: data required for processing
 * This function is called from hyperbola_navigation_index_clist_select_row
 * when the user  clicks on a see/seealso reference. It is the traversal 
 * function used when traversing the relevant index tree.
 * This function will shift the display to the target of the see/seealso
 * reference.
 * Returns 1 when target row found or 0 to continue traversing tree
 */

static gint
hyperbola_navigation_index_show_reference_item(const char *key, IndexItem *ii,
                                        MoreCListInfo *mci)
{
	CListCreationInfo *cci=mci->cci;
	int rownum;
	HyperbolaNavigationIndex *hni = cci->hni;
	MoreCListInfo sub_mci = *mci;
	
	//printf("ii->index_id is %s, cci->indexid is %s\n", ii->index_id, cci->indexid);
	if(ii->index_id && !ii->xref && !strcmp(ii->index_id, cci->indexid)){
		rownum = gtk_clist_find_row_from_data(GTK_CLIST(hni->clist),
							(gpointer)ii);
		gtk_clist_select_row(GTK_CLIST(hni->clist), rownum, 0);
		gtk_clist_unselect_row(GTK_CLIST(hni->clist), cci->current_rownum, 0);
		gtk_clist_moveto(GTK_CLIST(hni->clist), rownum, 0, 0.5, 0.0);
		return 1;
	} else {
		if (ii->subitems)
			g_tree_traverse (ii->subitems, (GTraverseFunc)
							hyperbola_navigation_index_show_reference_item,
							G_IN_ORDER, &sub_mci);
	}
	return 0;


}

/*
 * hyperbola_navigation_index_show_index_item
 * @const char *key: the key to the item in the index tree
 * @IndexItem *ii: the value of the item in the index tree
 * @ MoreCListInfo *mci: data required for processing
 * This function is first called from
 * hyperbola_navigation_index_create_clist(), and then recursively
 * from here as traverse subtrees.
 * It creates index entries for the index display (hni->clist).
 * Returns 0 to indicate should continue traversing tree.
 */

static gint
hyperbola_navigation_index_show_index_item(const char *key, IndexItem *ii,
					MoreCListInfo *mci)
{
	int rownum;
	char *rowtext;
	CListCreationInfo *cci=mci->cci;
	HyperbolaNavigationIndex *hni = cci->hni;
	MoreCListInfo sub_mci = *mci;
	sub_mci.indent++;

	
	rowtext = g_strdup_printf ("%*s%s", mci->indent * 5, "", ii->text);
	rownum = gtk_clist_append (GTK_CLIST (hni->clist), &rowtext);
	hni->index_terms_found = TRUE;
	gtk_clist_set_row_data (GTK_CLIST (hni->clist), rownum, ii);
				
	if (ii->duplicates)
		g_tree_traverse (ii->duplicates, (GTraverseFunc)
				 hyperbola_navigation_index_show_duplicate_index_item,
				 G_IN_ORDER, &sub_mci);
	if (ii->ref_subitems)
		g_tree_traverse (ii->ref_subitems, (GTraverseFunc)
				 hyperbola_navigation_index_show_index_item,
				 G_IN_ORDER, &sub_mci);
	if (ii->subitems)
		g_tree_traverse (ii->subitems, (GTraverseFunc)
				 hyperbola_navigation_index_show_index_item,
				 G_IN_ORDER, &sub_mci);
	return 0;


}

/* hyperbola_navigation_index_create_clist
 * @HyperbolaNavigationIndex *hni
 * Function traverses relevant tree (according to value of hni->index_type),
 * and populates the index display (hni->clist) with these tree entries. 
 * If no entries are found (ie hni->index_terms_found remains FALSE, then a
 * suitable message is displayed in the hni->clist.
 */

static void
hyperbola_navigation_index_create_clist (HyperbolaNavigationIndex * hni) 
{
	CListCreationInfo cci;
	MoreCListInfo mci;
	GtkWidget *index_items = NULL;

	memset (&cci, 0, sizeof (cci));
	memset (&mci, 0, sizeof (mci));

	cci.hni = hni;
	mci.cci = &cci;
	mci.indent = 0;

	switch(hni->index_type){
		case CONTENTS_SELECTED_INDEX_FILES:
			index_items = (GtkWidget *) hni->contents_selected_index_items;
			break;
		case INDEX_SELECTED_INDEX_FILES:
			index_items = (GtkWidget *) hni->index_selected_index_items;
			break;
		case ALL_INDEX_FILES:
			index_items = (GtkWidget *) hni->all_index_items;
			break;
	};


	/* Set hni->index_terms_found to FALSE. If an index term is
	 * entered into the clist then this is set to TRUE.
	 */
	hni->index_terms_found = FALSE;
		
	gtk_clist_freeze (GTK_CLIST (hni->clist));
	gtk_clist_clear (GTK_CLIST (hni->clist));
	gtk_widget_set_sensitive(GTK_WIDGET(hni->clist), TRUE);
	g_tree_traverse ((GTree *)index_items,
				(GTraverseFunc) hyperbola_navigation_index_show_index_item,
                         G_IN_ORDER, &mci);
	/* if hni->index_terms_found is FALSE, then no entries for clist, so
	 * enter the following message there.
	*/
	if (hni->index_terms_found == FALSE){
		
		const char *message[] = { _("No index terms found for this selection")};
		gtk_clist_insert (GTK_CLIST (hni->clist), 0, (char **)message);
		gtk_clist_set_selectable(GTK_CLIST(hni->clist), 0, FALSE);
		/* We dont want to be able to select this row , however
		 * gtk_clist_set_selectable on its own , would not work on a 
		 * double-click so using this function instead. 
		 */ 
		gtk_widget_set_sensitive(GTK_WIDGET(hni->clist), FALSE);
	}
	
	gtk_clist_thaw (GTK_CLIST (hni->clist));
}

/*
 * hyperbola_navigation_index_show_selected_duplicate_item
 * @const char *key: the key to the item in the index tree
 * @IndexItem *ii: the value of the item in the index tree
 * @ MoreCListInfo *mci: data required for processing
 * This function is a traversal function called from
 * hyperbola_navigation_index_show_selected_item().
 * If an entry has a duplicates subtree (ie if ii->duplicates),
 * then this duplicates subtree is traversed and its entries are
 * displayed.
 * Returns 0 to indicate continued traversal of tree
 */
static gint
hyperbola_navigation_index_show_selected_duplicate_item(const char *key, IndexItem * ii,
                                      MoreCListInfo * mci)
{
        int rownum;
        char *rowtext, *tmp_text = NULL, *output_text = NULL;
        CListCreationInfo *cci = mci->cci;
        HyperbolaNavigationIndex *hni = cci->hni;
        MoreCListInfo dup_mci = *mci;
        GdkColor c;

	tmp_text = g_strdup(ii->text);
        output_text = strtok(tmp_text, "#");
        if (output_text == NULL)
                output_text = g_strdup(ii->text);

        rowtext = g_strdup_printf ("%*s%s", (mci->indent - 1) * 5,
                                   "", output_text);
        rownum = gtk_clist_append (GTK_CLIST (hni->clist), &rowtext);
        gtk_clist_set_row_data (GTK_CLIST (hni->clist), rownum, ii);
	  if (dup_mci.highlight){
        	/* highlight this row as a match */

        	c.red = c.green = 65535;
        	c.blue = 20000;
        	gdk_color_alloc (gdk_rgb_get_cmap (), &c);
        	gtk_clist_set_background (GTK_CLIST (hni->clist), rownum, &c);

        	if (!cci->did_select) {
       		         cci->did_select = TRUE;
                	gtk_clist_select_row (GTK_CLIST (hni->clist), rownum, 0);
        	}
	}
        /* Easiest way of showing the parents of matching items*/
        if (!dup_mci.matches && dup_mci.full_matches < cci->nwords)
                gtk_clist_remove (GTK_CLIST (hni->clist), rownum);

        return 0;
}

/*
 * hyperbola_navigation_index_show_selected_item
 * @const char *key: the key to the item in the index tree
 * @IndexItem *ii: the value of the item in the index tree
 * @ MoreCListInfo *mci: data required for processing
 * This function is the traversal function called from
 * hyperbola_navigation_index_show_selected_terms(), to
 * traverse the selected tree, and show only those index terms
 * that match text entered in the GtkEntry box.
 * Returns 0 to indicate continued traversal of tree
 */
static gint
hyperbola_navigation_index_show_selected_item (const char *key, IndexItem * ii,
                                      MoreCListInfo * mci)
{
        int rownum, i;
        char *rowtext;
        CListCreationInfo *cci = mci->cci;
        HyperbolaNavigationIndex *hni = cci->hni;
        MoreCListInfo sub_mci = *mci;
        MoreCListInfo dup_mci = sub_mci;
        int add_matches;        /* Whether this item itself contributed to a match */


        /* Three types of display:
         * shown - when it is part of a match, or is a parent of a matched item
         * shown + colored - when it has all the stuff needed for a match, but no more
         * hidden - when it is totally irrelevant
         */
        sub_mci.indent++;
        sub_mci.matches = 0;

        for (i = sub_mci.full_matches = add_matches = 0; i < cci->nwords; i++) {
                gboolean this_matches;

                this_matches = text_matches (ii->text, cci->words[i]);
                if (this_matches) {
                        add_matches++;
                        sub_mci.super_matches[i] = 1;

                }
                if (mci->super_matches[i] || this_matches)
                        sub_mci.full_matches++;
        }

        rowtext = g_strdup_printf ("%*s%s", mci->indent * 5,
                    	"", ii->text);
        rownum = gtk_clist_append (GTK_CLIST (hni->clist), &rowtext);
        gtk_clist_set_row_data (GTK_CLIST (hni->clist), rownum, ii);

        if (cci->nwords && sub_mci.full_matches >= cci->nwords && add_matches) { 
	        /* highlight this row as a match */
                GdkColor c;

                c.red = c.green = 65535;
                c.blue = 20000;
                gdk_color_alloc (gdk_rgb_get_cmap (), &c);
                gtk_clist_set_background (GTK_CLIST (hni->clist), rownum, &c);

                if (!cci->did_select) {
                        cci->did_select = TRUE;
                        gtk_clist_select_row (GTK_CLIST (hni->clist), rownum,
                                              0);
                }
		dup_mci.highlight = TRUE; /* if highlighting this term also need to 
					   * highlight duplicates 
					   */

        }
	if (ii->duplicates){
               	dup_mci.indent = sub_mci.indent;
		dup_mci.matches = sub_mci.matches;
		dup_mci.full_matches = sub_mci.full_matches;
               	g_tree_traverse (ii->duplicates,
                       	         (GTraverseFunc)
                               	 hyperbola_navigation_index_show_selected_duplicate_item,
                               	 G_IN_ORDER, &dup_mci);
	}
	dup_mci.highlight = FALSE;

        if (ii->subitems)
                g_tree_traverse (ii->subitems,
                                 (GTraverseFunc)
                                 hyperbola_navigation_index_show_selected_item,
                                 G_IN_ORDER, &sub_mci);

        /* Easiest way of showing the parents of matching items*/
        if (!sub_mci.matches && sub_mci.full_matches < cci->nwords)
                gtk_clist_remove (GTK_CLIST (hni->clist), rownum);

        if (sub_mci.matches || sub_mci.full_matches >= cci->nwords)
                mci->matches++;

        return 0;
}

#else

static gint
hyperbola_navigation_index_show_item (const char *key, IndexItem * ii,
                                      MoreCListInfo * mci)
{
        int rownum, i;
        char rowtext[512], *textptr, *see_start, *see_end;
        CListCreationInfo *cci = mci->cci;
        HyperbolaNavigationIndex *hni = cci->hni;
        MoreCListInfo sub_mci = *mci;
        int my_matches;         /* Whether this item is part of a match */
        int add_matches;        /* Whether this item itself contributed to a match */

        /* Three types of display:
         * shown - when it is part of a match, or is a parent of a matched item
         * shown + colored - when it has all the stuff needed for a match, but no more
         * hidden - when it is totally irrelevant
         */
        sub_mci.indent++;
        sub_mci.matches = 0;

        /* We ignore secondary terms that were displayed on the toplevel
         * using the "secondary, primary" form - looks nicer when searching */
        if (ii->xref && cci->nwords > 0)
                return 0;

        for (i = my_matches = add_matches = 0; i < cci->nwords; i++) {
                gboolean this_matches;

                this_matches = text_matches (ii->text, cci->words[i]);
                if (this_matches) {
                        add_matches++;
                        sub_mci.super_matches[i] = 1;

                }
                if (mci->super_matches[i] || this_matches)
                        my_matches++;
        }

        switch (ii->type) {
        default:
                see_start = see_end = "";
                break;
        case SEE:
                see_start = _("see ");
                see_end = ")";
                break;
        case SEEALSO:
                see_start = _("see also ");
                see_end = ")";
                break;
        }

        g_snprintf (rowtext, sizeof (rowtext), "%*s%s%s%s", mci->indent * 3,
                    "", see_start, ii->text, see_end);
        textptr = rowtext;
        rownum = gtk_clist_append (GTK_CLIST (hni->clist), &textptr);
        gtk_clist_set_row_data (GTK_CLIST (hni->clist), rownum, ii);

        if (cci->nwords && my_matches >= cci->nwords && add_matches) {  /* highlight this row as a match */
                GdkColor c;

                c.red = c.green = 65535;
                c.blue = 20000;
                gdk_color_alloc (gdk_rgb_get_cmap (), &c);
                gtk_clist_set_background (GTK_CLIST (hni->clist), rownum, &c);

                if (!cci->did_select) {
                        cci->did_select = TRUE;
                        gtk_clist_select_row (GTK_CLIST (hni->clist), rownum,
                                              0);
                }
        }

        if (ii->subitems)
                g_tree_traverse (ii->subitems,
                                 (GTraverseFunc)
                                 hyperbola_navigation_index_show_item,
                                 G_IN_ORDER, &sub_mci);

        /* Easiest way of showing the parents of matching items */
        if (!sub_mci.matches && my_matches < cci->nwords)
                gtk_clist_remove (GTK_CLIST (hni->clist), rownum);

        if (sub_mci.matches || my_matches >= cci->nwords)
                mci->matches++;

        return 0;
}
#endif

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/*
 * hyperbola_navigation_index_show_selected_terms
 * @HyperbolaNavigationIndex *hni
 * User has indicated that only those index terms matching the 
 * text entered in the GtkEntry box should be displayed.
 * The relevant tree is traversed and matching terms are displayed.
 */
static void
hyperbola_navigation_index_show_selected_terms (HyperbolaNavigationIndex *hni)
{
	CListCreationInfo cci;
	MoreCListInfo mci;
	char *stxt, *tmp_stxt;
	char *ctmp = NULL;
	int tmp_len;
	GtkWidget *index_items = NULL;

	switch (hni->index_type) {
		case ALL_INDEX_FILES:
			index_items = (GtkWidget *)hni->all_index_items;
			break;
		case CONTENTS_SELECTED_INDEX_FILES:
			index_items = (GtkWidget *)hni->contents_selected_index_items;
			break;
		case INDEX_SELECTED_INDEX_FILES:
			index_items = (GtkWidget *)hni->index_selected_index_items;
			break;
		}


	stxt = gtk_entry_get_text (GTK_ENTRY (hni->ent));

	memset (&cci, 0, sizeof (cci));
	memset (&mci, 0, sizeof (mci));

	cci.hni = hni;

	tmp_len = strlen (stxt) + 1;
	tmp_stxt = alloca (tmp_len);
	memcpy (tmp_stxt, stxt, tmp_len);
	ctmp = strtok (tmp_stxt, ", \t");
	cci.nwords = 0;

	if (ctmp) {
		do {
			cci.words[cci.nwords] = ctmp;
			cci.nwords++;
		}
		while ((ctmp = strtok (NULL, ", \t")) && (size_t) cci.nwords <
			sizeof (cci.words) / sizeof (cci.words[0]));
	}

	cci.did_select = FALSE;
	mci.cci = &cci;
	mci.indent = 0;

	gtk_clist_freeze (GTK_CLIST (hni->clist));
	gtk_clist_clear (GTK_CLIST (hni->clist));
	gtk_widget_set_sensitive(GTK_WIDGET(hni->clist), TRUE);
	
	g_tree_traverse ((GTree *)index_items,
		(GTraverseFunc) hyperbola_navigation_index_show_selected_item,
		G_IN_ORDER, &mci);

	if (!mci.matches && cci.nwords) {
		const char *nomatches[] = { _("No matches.") };

		gtk_clist_insert (GTK_CLIST (hni->clist),0, (char **) nomatches);
		gtk_clist_set_selectable(GTK_CLIST(hni->clist), 0, FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(hni->clist), FALSE);
	}

	gtk_clist_thaw (GTK_CLIST (hni->clist));
}

#endif

#ifndef  ENABLE_SCROLLKEEPER_SUPPORT
static void
hyperbola_navigation_index_update_clist (HyperbolaNavigationIndex * hni)
{
        CListCreationInfo cci;
        MoreCListInfo mci;
        char *stxt, *tmp_stxt;
        char *ctmp = NULL;
        int tmp_len;

        stxt = gtk_entry_get_text (GTK_ENTRY (hni->ent));

        memset (&cci, 0, sizeof (cci));
        memset (&mci, 0, sizeof (mci));

        cci.hni = hni;

        tmp_len = strlen (stxt) + 1;
        tmp_stxt = alloca (tmp_len);
        memcpy (tmp_stxt, stxt, tmp_len);
        ctmp = strtok (tmp_stxt, ", \t");
        cci.nwords = 0;
        if (ctmp) {
                do {
                        cci.words[cci.nwords] = ctmp;
                        g_print ("Word %d is %s\n", cci.nwords, ctmp);
                        cci.nwords++;
                }
                while ((ctmp = strtok (NULL, ", \t")) &&
                       (size_t) cci.nwords <
                       sizeof (cci.words) / sizeof (cci.words[0]));
        }

        cci.did_select = FALSE;
        mci.cci = &cci;
        mci.indent = 0;

        gtk_clist_freeze (GTK_CLIST (hni->clist));
        gtk_clist_clear (GTK_CLIST (hni->clist));

        g_tree_traverse (hni->all_items,
                         (GTraverseFunc) hyperbola_navigation_index_show_item,
                         G_IN_ORDER, &mci);

        if (!mci.matches && cci.nwords) {
                int rownum;
                const char *nomatches[] = { _("No matches.") };

                rownum =
                        gtk_clist_append (GTK_CLIST (hni->clist),
                                          (char **) nomatches);
                gtk_clist_set_selectable (GTK_CLIST (hni->clist), rownum,
                                          FALSE);
        }

        gtk_clist_thaw (GTK_CLIST (hni->clist));
}
#endif

static void
hyperbola_navigation_index_ent_changed (GtkWidget * ent,
                                        HyperbolaNavigationIndex * hni)
{
#ifndef ENABLE_SCROLLKEEPER_SUPPORT
        hyperbola_navigation_index_update_clist (hni);
#endif
}

/*
 * hyperbola_navigation_index_ent_activate
 * @GtkWidget *ent : GtkEntry box where user enters text of index
 *                   terms to be displayed
 * @HyperbolaNavigationIndex *hni
 * When the user presses return within the GtkEntry box,
 * the index display (hni->clist) is recreated to show matching terms only
 */
static void
hyperbola_navigation_index_ent_activate (GtkWidget * ent,
                                         HyperbolaNavigationIndex * hni)
{
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	hyperbola_navigation_index_show_selected_terms (hni);

#endif
}


static void
start_document (SAXParseInfo * spi)
{
}

static void
end_document (SAXParseInfo * spi)
{
}

static void
characters (SAXParseInfo * spi, const gchar * chars, int len)
{
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	if (spi->sub_type == NONE ||
		!spi->in_term) 
		return;
#else		
	if (spi->sub_type == NONE)
		return;
#endif

	g_string_sprintfa (spi->sub_text, "%.*s", len, chars);
}

/* Removes all duplicate spaces */
static void
despace (GString * s)
{
        char *ctmp, *ctmp_s = NULL;
        int i;

        g_assert (s->len == (int) strlen (s->str));
        for (ctmp = s->str, i = s->len; *ctmp; ctmp++, i--) {
                if (isspace (*ctmp)) {
                        if (*ctmp != ' ')
                                *ctmp = ' ';
                        if (!ctmp_s)
                                ctmp_s = ctmp;
                } else if (ctmp_s) {
                        if ((ctmp - ctmp_s) > 1) {
                                memmove (ctmp_s + 1, ctmp, i + 1);
                                ctmp = ctmp_s + 2;
                                i--;
                                if (i != (int) strlen (ctmp))
                                        g_error
                                                ("i (%d) != strlen(ctmp) (%ld)",
                                                 i, (long) strlen (ctmp));
                        }
                        ctmp_s = NULL;
                }
        }
        s->len = strlen (s->str);
}


static xmlEntityPtr
get_entity (SAXParseInfo * spi, const gchar * name)
{
#ifdef ERROR_OUTPUT
        g_print ("in getEntity:%s\n", name);
#endif

        return xmlGetPredefinedEntity (name);
}

#ifndef ENABLE_SCROLLKEEPER_SUPPORT
static void
start_element (SAXParseInfo * spi, const gchar * name, const xmlChar ** attrs)
{
        if (!g_strcasecmp (name, "indexterm")) {
                int i;

                for (i = 0; attrs[i]; i++) {
                        if (!g_strcasecmp (attrs[i], "id")) {
                                i++;
                                break;
                        }
                }

                g_return_if_fail (attrs[i]);

                spi->idx_ref = g_strdup (attrs[i]);
                spi->in_term++;
                g_string_assign (spi->sub_text, "");
                for (i = PRIMARY; i < NONE; i++)
                        spi->stinfo[i] = NULL;
                spi->sub_type = NONE;

                return;
        }
        if (!spi->in_term || spi->sub_type != NONE)
                return;

        if (!g_strcasecmp (name, "primary")) {
                spi->sub_type = PRIMARY;
        } else if (!g_strcasecmp (name, "secondary")) {
                spi->sub_type = SECONDARY;
        } else if (!g_strcasecmp (name, "seealso")) {
                spi->sub_type = SEEALSO;
        } else if (!g_strcasecmp (name, "see")) {
                spi->sub_type = SEE;
        } else
                spi->sub_type = NONE;

        g_return_if_fail (spi->sub_type == NONE ||
                          !spi->stinfo[spi->sub_type]);
}

static void
end_element (SAXParseInfo * spi, const gchar * name)
{
        int this_type;

        if (!g_strcasecmp (name, "indexterm")) {
                int i;
                IndexItem *parent_ii = NULL, *ii = NULL;
                ItemType it;
                GTree *parent_tree = NULL;

                /* Algorithm:
                 * If this is a see (also), we have to list it on the top level, so make the node.
                 * If this is a secondary, we have to first find/make the primary.
                 */

                spi->in_term--;

                g_return_if_fail (spi->stinfo[PRIMARY]);

                it = PRIMARY;
                if (spi->stinfo[SECONDARY] || spi->stinfo[SEE] ||
                    spi->stinfo[SEEALSO]) {
                        parent_ii =
                                g_tree_lookup (spi->idx->all_items,
                                               spi->stinfo[PRIMARY]);

                        if (!parent_ii) {
                                parent_ii = g_new0 (IndexItem, 1);
                                parent_ii->type = PRIMARY;
                                parent_ii->text =
                                        g_strdup (spi->stinfo[PRIMARY]);
                                g_tree_insert (spi->idx->all_items,
                                               parent_ii->text, parent_ii);
                        }

                        if (!parent_ii->subitems)
                                parent_ii->subitems =
                                        g_tree_new ((GCompareFunc)
                                                    g_strcasecmp);
                        parent_tree = parent_ii->subitems;

                        if (spi->stinfo[SECONDARY])
                                it = SECONDARY;
                        else if (spi->stinfo[SEE])
                                it = SEE;
                        else if (spi->stinfo[SEEALSO])
                                it = SEEALSO;

                        if ((it == SECONDARY) &&
                            (spi->stinfo[SEE] || spi->stinfo[SEEALSO])) {
                                /* Make a second layer */
                                ii =
                                        g_tree_lookup (parent_tree,
                                                       spi->stinfo[PRIMARY]);

                                if (!ii) {
                                        ii = g_new0 (IndexItem, 1);
                                        ii->type = SECONDARY;
                                        ii->text =
                                                g_strdup (spi->
                                                          stinfo[SECONDARY]);
                                        g_tree_insert (parent_tree, ii->text,
                                                       ii);
                                }

                                if (!ii->subitems)
                                        ii->subitems =
                                                g_tree_new ((GCompareFunc)
                                                            g_strcasecmp);
                                parent_ii = ii;
                                parent_tree = parent_ii->subitems;

                                it = spi->stinfo[SEE] ? SEE : SEEALSO;
                        }
                } else {
                        parent_tree = spi->idx->all_items;
                }

                g_assert (parent_tree != spi->idx->all_items ||
                          it == PRIMARY);

                ii = g_tree_lookup (parent_tree, spi->stinfo[it]);
                if (!ii) {
                        ii = g_new0 (IndexItem, 1);
                        ii->text = g_strdup (spi->stinfo[it]);
                        g_tree_insert (parent_tree, ii->text, ii);
                        ii->type = it;
                }
                if (!ii->uri)
                        ii->uri =
                                g_strdup_printf ("help:%s/%s/%s",
                                                 spi->appname, spi->filename,
                                                 spi->idx_ref);
                g_assert (ii->type == it);

                if (spi->stinfo[SECONDARY]) {
                        /* Also insert a top-level node that gives info on this secondary node */

                        char buf[512];
                        char *txt;

                        g_snprintf (buf, sizeof (buf), "%s, %s",
                                    spi->stinfo[SECONDARY],
                                    spi->stinfo[PRIMARY]);
                        if (spi->stinfo[SEE] || spi->stinfo[SEEALSO]) {
                                if (spi->stinfo[SEE]) {
                                        it = SEE;
                                        txt = spi->stinfo[SEE];
                                        strcat (buf, _(" (see \""));
                                } else {
                                        txt = spi->stinfo[SEEALSO];
                                        strcat (buf, _(" (see also \""));
                                }

                                strcat (buf, txt);
                                strcat (buf, ")");
                        }
                        parent_tree = spi->idx->all_items;

                        ii = g_tree_lookup (parent_tree, spi->stinfo[it]);
                        if (!ii) {
                                ii = g_new0 (IndexItem, 1);
                                ii->text = g_strdup (buf);
                                g_tree_insert (parent_tree, ii->text, ii);
                                ii->type = PRIMARY;
                                ii->xref = TRUE;
                        }

                        if (!ii->uri)
                                ii->uri =
                                        g_strdup_printf ("help:%s/%s/%s",
                                                         spi->appname,
                                                         spi->filename,
                                                         spi->idx_ref);
                }

                for (i = PRIMARY; i < NONE; i++) {
                        g_free (spi->stinfo[i]);
                        spi->stinfo[i] = NULL;
                }
                g_free (spi->idx_ref);
                spi->idx_ref = NULL;

                return;
        }

        if (!spi->in_term || spi->sub_type == NONE)
                return;

        if (!g_strcasecmp (name, "primary")) {
                this_type = PRIMARY;
        } else if (!g_strcasecmp (name, "secondary")) {
                this_type = SECONDARY;
        } else if (!g_strcasecmp (name, "seealso")) {
                this_type = SEEALSO;
        } else if (!g_strcasecmp (name, "see")) {
                this_type = SEE;
        } else
                this_type = NONE;

        g_return_if_fail (this_type != NONE && !spi->stinfo[this_type] &&
                          this_type == (int) spi->sub_type);

        if (spi->sub_text->len) {
                despace (spi->sub_text);
                spi->stinfo[this_type] = g_strdup (spi->sub_text->str);
                g_assert (spi->stinfo[this_type]);
        }
        g_message ("Set \"%s\" for %s (%d)", spi->stinfo[this_type], name,
                   this_type);
        g_string_assign (spi->sub_text, "");
        spi->sub_type = NONE;
}

#endif



#ifdef ENABLE_SCROLLKEEPER_SUPPORT
static void
start_element (SAXParseInfo * spi, const gchar * name, const xmlChar ** attrs)
{
	if (!g_strcasecmp (name, "indexitem") &&
				spi->sub_type==NONE) {
		int i;
		for (i = PRIMARY; i < NONE; i++){
			spi->idx_refs[i] = NULL;
			spi->stinfo[i] = NULL;
		}
		spi->sub_type = PRIMARY;
		spi->in_indexitem = 1;
		spi->parent_tree = spi->index_items;
		return;
	}

	if (!g_strcasecmp (name, "indexitem") &&
				spi->sub_type==PRIMARY) {
		process_index_term (spi, PRIMARY);
		spi->sub_type = SECONDARY;
		spi->in_indexitem++;
		return;
	}

	if (!g_strcasecmp (name, "indexitem") &&
				spi->sub_type==SECONDARY) {
		if(spi->in_indexitem > 1){
			process_index_term (spi, SECONDARY);
			spi->sub_type = TERTIARY;
		}
		spi->in_indexitem++;
		return;
	}

	if (!g_strcasecmp (name, "indexitem") &&
				spi->sub_type==TERTIARY) {
		if(spi->in_indexitem < 2)
			spi->sub_type = SECONDARY;
		spi->in_indexitem++;
		return;
	}

	if (!g_strcasecmp (name, "title")){
		spi->in_term++;
		g_string_assign (spi->sub_text, "");
		return;
	}

	
	if (!g_strcasecmp (name, "link")) {
		int i, j;
		for (i = 0; attrs[i]; i++) {
			if (!g_strcasecmp (attrs[i], "linkid")){
				i++;
				break;
			}
		}
		g_return_if_fail(attrs[i]);
		spi->idx_refs[spi->sub_type] = g_slist_prepend(spi->idx_refs[spi->sub_type],
							g_strdup(attrs[i])); 

		for(j=i+1; attrs[j]; j++){
			if (!g_strcasecmp (attrs[j], "indexid")){
				j++;
				break;
			}
		}
		if(attrs[j] && strlen(attrs[j]) > 1){
			spi->index_ids[spi->sub_type] = g_strdup(attrs[j]); 
		}
		return;
	}

	if (!g_strcasecmp (name, "see")) {
		int i;
		for (i = 0; attrs[i]; i++) {
			if (!g_strcasecmp (attrs[i], "indexid")){
				i++;
				break;
			}
		}
		g_return_if_fail(attrs[i]);
		spi->index_ids[SEE] = g_strdup(attrs[i]); 
		spi->in_term++;
		g_string_assign (spi->sub_text, "");
		return;
	}


	if (!g_strcasecmp (name, "seealso")) {
		int i;
		for (i = 0; attrs[i]; i++) {
			if (!g_strcasecmp (attrs[i], "indexid")){
				i++;
				break;
			}
		}
		g_return_if_fail(attrs[i]);
		spi->index_ids[SEEALSO] = g_strdup(attrs[i]); 
		spi->in_term++;
		g_string_assign (spi->sub_text, "");
		return;
	}


	if (!g_strcasecmp (name, "indexdoc")){
		spi->sub_type = NONE;
		return;
	}
					

	if (!spi->in_term || spi->sub_type != NONE)
		return;

	g_return_if_fail (spi->sub_type == NONE ||
			  !spi->stinfo[spi->sub_type]);
}

static void 
process_index_term(SAXParseInfo *spi, ItemType it)
{ 
	GSList *idx_refs_list = NULL;
	GTree *parent_tree = NULL;
	char *tmp_uri = NULL, *tmp_text = NULL; 
	IndexItem *ii = NULL, *parent_ii = NULL, *duplicate_ii = NULL;

	if (spi->sub_type == SECONDARY){
		parent_ii = g_tree_lookup(spi->index_items,
						spi->stinfo[PRIMARY]);
		if (!parent_ii->subitems)
			parent_ii->subitems = 
				g_tree_new((GCompareFunc)
						g_strcasecmp);
		spi->parent_tree = parent_ii->subitems;
	}
	ii =
		g_tree_lookup (spi->parent_tree,
			       		spi->stinfo[it]);

	if (!ii) {
		ii = g_new0 (IndexItem, 1);
		ii->type = it;
		ii->text = g_strdup(spi->stinfo[it]);
		if(spi->index_ids[it])
			ii->index_id = g_strdup(spi->index_ids[it]);
		g_tree_insert (spi->parent_tree,
			       		ii->text, ii);
	}

	if(spi->idx_refs[it]) {
		idx_refs_list = spi->idx_refs[it];
		while (idx_refs_list != NULL) {
			if(!ii->uri){	
				tmp_uri=g_strdup_printf("%s%s%s",
							spi->doc_uri,
							spi->separator,
							(char *)idx_refs_list->data);
				ii->uri=tmp_uri;
			} else {
				tmp_text=g_strdup_printf("%s#%s",
							 spi->stinfo[it],
						         (char *)idx_refs_list->data);
				if (!ii->duplicates)
                               		ii->duplicates =
                                       			g_tree_new ((GCompareFunc)
                                               		g_strcasecmp);

				duplicate_ii =
						g_tree_lookup (ii->duplicates,
			       					tmp_text);
				if(!duplicate_ii){
					duplicate_ii = g_new0 (IndexItem, 1);
                        		duplicate_ii->type = it;
                        		duplicate_ii->text =tmp_text;
                        		g_tree_insert (ii->duplicates,
                                    		        duplicate_ii->text, duplicate_ii);
					tmp_uri=g_strdup_printf("%s%s%s",
								spi->doc_uri,
								spi->separator,
								(char *)idx_refs_list->data);
					duplicate_ii->uri=tmp_uri;
				}
			}
			idx_refs_list=g_slist_next(idx_refs_list);
		}
	}
	if(spi->sub_type == SECONDARY){
		if (!ii->subitems)
			ii->subitems = 
				g_tree_new((GCompareFunc)
						g_strcasecmp);
		spi->parent_tree = ii->subitems;
	}

	if (spi->stinfo[SEE]) {
                char *buf;
		if (!ii->ref_subitems)
                          ii->ref_subitems =
                                        g_tree_new ((GCompareFunc)
                                                        g_strcasecmp);
		parent_tree = ii->ref_subitems;
                buf = g_strdup_printf ("%s%s%s",
                                        _("(see: "), spi->stinfo[SEE], ")");
                ii = g_tree_lookup (parent_tree, buf);
                if (!ii) {
                         ii = g_new0 (IndexItem, 1);
                         ii->text = g_strdup (buf);
                         g_tree_insert (parent_tree, ii->text, ii);
                         ii->type = SEE;
                         ii->xref=TRUE;
                         if(spi->index_ids[SEE])
                                ii->index_id=g_strdup(spi->index_ids[SEE]);
                }
	}
	if (spi->stinfo[SEEALSO]) {
                char *buf;
		if (!ii->ref_subitems)
                          ii->ref_subitems =
                                        g_tree_new ((GCompareFunc)
                                                        g_strcasecmp);
		parent_tree = ii->ref_subitems;
                buf = g_strdup_printf ("%s%s%s",
                                        _("(see also: "), spi->stinfo[SEEALSO], ")");
                ii = g_tree_lookup (parent_tree, buf);
                if (!ii) {
                         ii = g_new0 (IndexItem, 1);
                         ii->text = g_strdup (buf);
                         g_tree_insert (parent_tree, ii->text, ii);
                         ii->type = SEEALSO;
                         ii->xref=TRUE;
                         if(spi->index_ids[SEEALSO])
                                ii->index_id=g_strdup(spi->index_ids[SEEALSO]);
                }
	}
	if(it != PRIMARY){
		int i;
		for (i = it; i < NONE; i++) {
                       g_free (spi->stinfo[i]);
                       spi->stinfo[i] = NULL;
                	g_free (spi->index_ids[spi->sub_type]);
                	spi->index_ids[spi->sub_type] = NULL;
			g_slist_free(spi->idx_refs[i]);
			spi->idx_refs[i] = NULL;
                }
	}


}
static void
end_element (SAXParseInfo * spi, const gchar * name)
{
	int this_type;/*, i;*/
	ItemType it;

	if (!g_strcasecmp (name, "indexitem")) {

		spi->in_indexitem--;
		it = spi->sub_type;

		if (spi->in_indexitem > 0 && spi->stinfo[it]){
			process_index_term(spi, it);
		} else if(spi->in_indexitem == 0){
			spi->sub_type = NONE;
			g_free (spi->stinfo[PRIMARY]);
			spi->stinfo[PRIMARY] = NULL;
			g_free (spi->index_ids[PRIMARY]);
			spi->index_ids[PRIMARY] = NULL;
		}
				
			
		return;
	}
       

	if (!g_strcasecmp (name, "indexdoc")){
		spi->sub_type = NONE;
		return;
	}

	if (spi->sub_type == NONE)
		return;

	if (!g_strcasecmp(name, "link")){
			this_type = spi->sub_type;
			return;
	} else if (!g_strcasecmp (name, "title")) {
		this_type=spi->sub_type;
		spi->in_term--;
	} else if (!g_strcasecmp (name, "seealso")) {
		this_type = SEEALSO;
		spi->in_term--;
	} else if (!g_strcasecmp (name, "see")) {
		this_type = SEE;
		spi->in_term--;
	} else
		this_type = NONE;


	if (spi->sub_text->len) {
		despace (spi->sub_text);
		spi->stinfo[this_type] = g_strdup (spi->sub_text->str);
		g_assert (spi->stinfo[this_type]);
	}
	/*g_message ("Set \"%s\" for %s (%d)", spi->stinfo[this_type], name,
		   this_type);*/
	g_string_assign (spi->sub_text, "");
}
#endif

static void
handle_error (SAXParseInfo * spi, const char *msg, ...)
{
        va_list args;

        va_start (args, msg);
#ifdef ERROR_OUTPUT
        g_logv ("XML", G_LOG_LEVEL_CRITICAL, msg, args);
#endif
        va_end (args);
}

static void
handle_fatal_error (SAXParseInfo * spi, const char *msg, ...)
{
        va_list args;

        va_start (args, msg);
#ifdef ERROR_OUTPUT
        g_logv ("XML", G_LOG_LEVEL_ERROR, msg, args);
#endif
        va_end (args);
}

static xmlSAXHandler sax = {
        NULL,                   /* internalSubset */
        NULL,                   /* isStandalone */
        NULL,                   /* hasInternalSubset */
        NULL,                   /* hasExternalSubset */
        NULL,                   /* resolveEntity */
        (getEntitySAXFunc) get_entity,  /* getEntity */
        NULL,                   /* entityDecl */
        NULL,                   /* notationDecl */
        NULL,                   /* attributeDecl */
        NULL,                   /* elementDecl */
        NULL,                   /* unparsedEntityDecl */
        NULL,                   /* setDocumentLocator */
        (startDocumentSAXFunc) start_document,  /* startDocument */
        (endDocumentSAXFunc) end_document,      /* endDocument */
        (startElementSAXFunc) start_element,    /* startElement */
        (endElementSAXFunc) end_element,        /* endElement */
        NULL,                   /* reference */
        (charactersSAXFunc) characters, /* characters */
        NULL,                   /* ignorableWhitespace */
        NULL,                   /* processingInstruction */
        NULL,                   /* comment */
        NULL,                   /* warning */
        (errorSAXFunc) handle_error,    /* error */
        (fatalErrorSAXFunc) handle_fatal_error, /* fatalError */
        NULL,                   /* parameterEntity */
        NULL,                   /* cdata block */
};
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/* 
 * hyperbola_navigation_index_clist_select_row
 * @GtkWidget *clist : clist for displaying index terms
 * @gint row : number of row that was selected
 * @gint column : number of column (always 0)
 * @GdkEvent *event
 * @HyperbolaNavigationIndex *hni
 * If the user has selected a row that links to an index term in
 * a document then this will be displayed in the nautilus main window.
 * If the user has selected a see/seealso reference then the index display
 * will change to show the targeted see/seealso term.
 */

void
hyperbola_navigation_index_clist_select_row (GtkWidget * clist, gint row,
				       gint column, GdkEvent * event,
				       HyperbolaNavigationIndex * hni)
{
	IndexItem *ii;
	GTree *index_items;
	CListCreationInfo cci;
	MoreCListInfo mci;

	memset (&cci, 0, sizeof (cci));
	memset (&mci, 0, sizeof (mci));

	cci.hni = hni;
	mci.cci = &cci;
	mci.indent = 0;

	if (!event || event->type != GDK_2BUTTON_PRESS)	/* we only care if the user has double-clicked on an item...? */
		return;

	ii = gtk_clist_get_row_data (GTK_CLIST (clist), row);

	if(ii->xref){
		cci.current_rownum = row;
		switch (hni->index_type) {
                	case ALL_INDEX_FILES:
                        	index_items = hni->all_index_items;
                        	break;
                	case CONTENTS_SELECTED_INDEX_FILES:
                        	index_items = hni->contents_selected_index_items;
                        	break;
                	case INDEX_SELECTED_INDEX_FILES:
                        	index_items = hni->index_selected_index_items;
                        	break;
                	default:
                        	index_items = hni->all_index_items;
                        	break;
        	}
		cci.indexid = g_strdup(ii->index_id);

	        gtk_clist_freeze (GTK_CLIST (hni->clist));
        	g_tree_traverse ((GTree *)index_items,
                         (GTraverseFunc) hyperbola_navigation_index_show_reference_item,
                         G_IN_ORDER, &mci);
        	gtk_clist_thaw (GTK_CLIST (hni->clist));
		
	}else{
		if (!ii->uri)
			return;

		nautilus_view_open_location_in_this_window (hni->index_view_frame, ii->uri);
	}
}


#else

static void
hyperbola_navigation_index_select_row (GtkWidget * clist, gint row,
                                       gint column, GdkEvent * event,
                                       HyperbolaNavigationIndex * hni)
{
        IndexItem *ii;
        if (!event || event->type != GDK_2BUTTON_PRESS) /* we only care if the user has double-clicked on an item...? */
                return;

        ii = gtk_clist_get_row_data (GTK_CLIST (clist), row);

        if (!ii->uri)
                return;

        nautilus_view_open_location_in_this_window (hni->view_frame, ii->uri);
}


static void
hyperbola_navigation_index_read_app (HyperbolaNavigationIndex * idx,
                                     const char *topdir, const char *appname)
{
        SAXParseInfo spi;
        DIR *dirh;
        struct dirent *dent;

        memset (&spi, 0, sizeof (spi));
        spi.idx = idx;
        spi.appname = (char *) appname;
        spi.sub_text = g_string_new (NULL);

        dirh = opendir (topdir);
        if (!dirh)
                return;

        while ((dent = readdir (dirh))) {
                char *ctmp;
                char buf[PATH_MAX];
                ctmp = strrchr (dent->d_name, '.');
                if (!ctmp || strcmp (ctmp, ".sgml"))
                        continue;

                g_snprintf (buf, sizeof (buf), "%s/%s", topdir, dent->d_name);
                g_message ("Let's try %s", buf);
                *ctmp = '\0';
                spi.filename = dent->d_name;

                xmlSAXUserParseFile (&sax, &spi, buf);
        }
        closedir (dirh);

        g_string_free (spi.sub_text, TRUE);
}
static void
hyperbola_navigation_index_read (HyperbolaNavigationIndex * idx,
                                 const char *topdir)
{
        DIR *dirh;
        struct dirent *dent;
        GList *langlist;

        langlist = gnome_i18n_get_language_list (NULL);

        dirh = opendir (topdir);
        if (!dirh)
                return;

        while ((dent = readdir (dirh))) {
                char buf[PATH_MAX];
                GList *ltmp;

                if (dent->d_name[0] == '.')
                        continue;

                for (ltmp = langlist; ltmp; ltmp = ltmp->next) {
                        g_snprintf (buf, sizeof (buf), "%s/%s/%s", topdir,
                                    dent->d_name, (char *) ltmp->data);
                        if (g_file_test (buf, G_FILE_TEST_ISDIR))
                                break;
                }

                if (!ltmp)
                        continue;

                hyperbola_navigation_index_read_app (idx, buf, dent->d_name);
        }

        closedir (dirh);
}
BonoboObject *
hyperbola_navigation_index_new (void)
{
        HyperbolaNavigationIndex *hni;
        GtkWidget *wtmp, *vbox;
        char *dir;

        hni = g_new0 (HyperbolaNavigationIndex, 1);
        hni->all_items = g_tree_new ((GCompareFunc) g_strcasecmp);

        dir = gnome_datadir_file ("gnome/help");
        if (!dir)
                return NULL;
        hyperbola_navigation_index_read (hni, dir);
        g_free (dir);

        vbox = gtk_vbox_new (FALSE, GNOME_PAD);

        hni->ent = gtk_entry_new ();
        gtk_signal_connect (GTK_OBJECT (hni->ent), "changed",
                            hyperbola_navigation_index_ent_changed, hni);
        gtk_signal_connect (GTK_OBJECT (hni->ent), "activate",
                            hyperbola_navigation_index_ent_activate, hni);
        gtk_container_add (GTK_CONTAINER (vbox), hni->ent);

        hni->clist = gtk_clist_new (1);
        gtk_clist_freeze (GTK_CLIST (hni->clist));
        gtk_clist_set_selection_mode (GTK_CLIST (hni->clist),
                                      GTK_SELECTION_BROWSE);

        gtk_signal_connect (GTK_OBJECT (hni->clist), "select_row",
                            hyperbola_navigation_index_select_row, hni);

        wtmp =
                gtk_scrolled_window_new (gtk_clist_get_hadjustment
                                         (GTK_CLIST (hni->clist)),
                                         gtk_clist_get_vadjustment (GTK_CLIST
                                                                    (hni->
                                                                     clist)));
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);

        gtk_container_add (GTK_CONTAINER (wtmp), hni->clist);
        gtk_container_add (GTK_CONTAINER (vbox), wtmp);

        hyperbola_navigation_index_update_clist (hni);

        gtk_clist_columns_autosize (GTK_CLIST (hni->clist));
        gtk_clist_thaw (GTK_CLIST (hni->clist));
        gtk_widget_show_all (vbox);

hni->view_frame = nautilus_view_new (vbox);

        return BONOBO_OBJECT (hni->view_frame);
}
#endif


#ifdef ENABLE_SCROLLKEEPER_SUPPORT

static void
hash_table_get_keys_callback (gpointer key,
                              gpointer value,
                              gpointer user_data)
{
        GSList **keys;

        keys = (GSList **) user_data;
        if(key){
                *keys = g_slist_prepend (*keys, key);
        }
}

static GSList *
hash_table_get_keys (GHashTable *hash_table)
{
        GSList *keys;
        keys = NULL;
        g_hash_table_foreach (hash_table,
                              hash_table_get_keys_callback,
                              &keys);
        return keys;
}

/* 
 * hyperbola_navigation_index_parse_index_files
 * @HyperbolaNavigationIndex *hni
 * @HyperbolaIndexSelection index_type: whether all index files,
 * index files selected from contents page, or index files selected from
 * index_select_page.
 * This function takes the relevant hashtable (according to specified 
 * index_type) and finds the keys to the hashtable. It then uses the keys
 * to find each index file and passes this file to the SAXParser. The
 * SAXParser will then read the entries from each index file and add them
 * to the spi.index_items tree. This tree will then be assigned to either
 * hni->all_index_items, hni->contents_selected_index_items, or 
 * hni->index_selected_index_items (according to setting of index_type).
 */

static void
hyperbola_navigation_index_parse_index_files (HyperbolaNavigationIndex *hni, 
						HyperbolaIndexSelection index_type)
{
	SAXParseInfo spi;
	GSList *keys = NULL, *tmp = NULL;
	GHashTable *index_files = NULL;

	memset (&spi, 0, sizeof (spi));
	spi.idx = hni;
	spi.index_items = g_tree_new ((GCompareFunc) g_strcasecmp);

	switch(index_type){
		case ALL_INDEX_FILES:
			index_files = hni->index_contents->doc_tree->all_index_files;
			break;
		case CONTENTS_SELECTED_INDEX_FILES:
			index_files = hni->contents_selected_index_files;
			break;
		case INDEX_SELECTED_INDEX_FILES:
			index_files = hni->index_selected_index_files;
			break;
	}

	keys = hash_table_get_keys(index_files);
	tmp = keys;

	while (tmp != NULL){
		spi.doc_uri = g_strdup((char *)tmp->data);
		/* Set the correct separator token.  SGML requires ? but
		 * the other use # */
		if (!g_strncasecmp ("help", spi.doc_uri, 4)) {
			spi.separator = "?";
		} else {
			spi.separator = "#";
		}
		spi.indexfile = g_strdup(g_hash_table_lookup((GHashTable *)index_files,
						  (char *)tmp->data));
		spi.sub_text = g_string_new (NULL);
		xmlSAXUserParseFile (&sax, &spi, spi.indexfile);
		g_string_free (spi.sub_text, TRUE);
		tmp = g_slist_next(tmp);
		/* If CONTENTS_SELECTED_INDEX_FILES remove the entry
		 * from hni->contents_selected_index_files once it has
		 * been parsed.
		 */
		if(index_type == CONTENTS_SELECTED_INDEX_FILES){
				g_hash_table_remove((GHashTable *)index_files, spi.doc_uri);
		}
		g_free(spi.doc_uri);
		g_free(spi.indexfile);
	}

	if(index_type == ALL_INDEX_FILES){
		hni->all_index_items = spi.index_items;
	}else if(index_type == CONTENTS_SELECTED_INDEX_FILES){
		hni->contents_selected_index_items = spi.index_items;
	} else if(index_type == INDEX_SELECTED_INDEX_FILES)
		hni->index_selected_index_items = spi.index_items;
	
}

/* 
 * index_ctree_populate
 * @HyperbolaNavigationIndex *hni
 * Populates the tree for the index_select_page
 */
static void
index_ctree_populate (HyperbolaNavigationIndex * hni)
{
	IndexPopulateInfo subpi;

	subpi.hni = hni;
	subpi.sibling = NULL;
	subpi.parent = NULL;

	g_tree_traverse (hni->index_contents->doc_tree->children, 
			index_ctree_populate_subnode, G_IN_ORDER, &subpi);
}

static gboolean
index_ctree_populate_subnode (gpointer key, gpointer value, gpointer user_data)
{
	HyperbolaTreeNode *node = value;
	IndexPopulateInfo *pi = (IndexPopulateInfo *) user_data, subpi;
	gboolean term;
	char *title;
	GdkPixmap *pixmap_closed = NULL;
	GdkBitmap *mask_closed = NULL;
	GdkPixmap *pixmap_opened = NULL;
	GdkBitmap *mask_opened = NULL;

	title = node->title;

	get_node_icons (pi->hni->index_contents, node,
                        &pixmap_closed, &mask_closed,
                        &pixmap_opened, &mask_opened);
	term = (node->type == HYP_TREE_NODE_PAGE) || !node->children;
	pi->sibling =
	gtk_ctree_insert_node (GTK_CTREE (pi->hni->index_contents->ctree),
                                       pi->parent, NULL, &title, 5,
                                       pixmap_closed, mask_closed,
                                       pixmap_opened, mask_opened,
                                       term, FALSE);
	node->user_data = pi->sibling;


	gtk_ctree_node_set_row_data (GTK_CTREE (pi->hni->index_contents->ctree), 
						pi->sibling, node);

	if (node->children) {
		
		subpi.hni=pi->hni;
		subpi.sibling = NULL;
		subpi.parent = pi->sibling;
		g_tree_traverse (node->children, index_ctree_populate_subnode,
                                 G_IN_ORDER, &subpi);
	}

	return FALSE;
}

/*
 * make_index_select_page
 * @GtkWidget *button: pointer to "..." button on index_display_page
 * @HyperbolaNavigationIndex *hni
 * This is called when the "..." button is clicked.
 * If the index_select_page has not yet been created, then 
 * a call is made to create_index_select_page().
 * If hni->index_select_page already exists, then it is shown.
 * The value of hni->page_type is set to INDEX_SELECT_PAGE
 */

static void
make_index_select_page (GtkWidget *button,
			HyperbolaNavigationIndex *hni)
{
	GtkWidget *page_label;

	if (hni->index_select_page == NULL){
		page_label = gtk_label_new((""));
		gtk_label_parse_uline(GTK_LABEL(page_label), _("_Index:"));
		create_index_select_page(hni);	
		gtk_widget_hide(hni->index_display_page);
		gtk_notebook_append_page(GTK_NOTEBOOK(hni->notebook),
				hni->index_select_page, page_label);
		gtk_notebook_set_page(GTK_NOTEBOOK(hni->notebook), 2);	
	}else{
		gtk_widget_hide(hni->index_display_page);
		gtk_widget_show(hni->index_select_page);
		gtk_notebook_set_page(GTK_NOTEBOOK(hni->notebook), 2);	
	}
	hni->page_type = INDEX_SELECT_PAGE;
}

/*
 * create_index_select_page
 * @HyperbolaNavigationIndex *hni
 * This function is called only once to create the index_select_page.
 * This widget is assigned to hni->index_select_page.
 */
static void
create_index_select_page(HyperbolaNavigationIndex *hni)
{
	GtkWidget *label;
	GtkWidget *ok_button, *cancel_button;
	GtkWidget *vbox, *hbox1, *hbox2;
	GtkWidget *hseparator;
		
	vbox = gtk_vbox_new(FALSE, 5);
	label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(label),_("Select _documents to show \nin index tab"));
	
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD_SMALL, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);	
	gtk_box_pack_start ( GTK_BOX(vbox), label, FALSE, FALSE, 5);

	hni->index_select_win = gtk_scrolled_window_new (
			gtk_clist_get_hadjustment(GTK_CLIST (
			hni->index_contents->ctree)),
			gtk_clist_get_vadjustment (GTK_CLIST 
			(hni->index_contents->ctree)));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (hni->index_select_win),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start ( GTK_BOX(vbox), hni->index_select_win, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (hni->index_select_win), hni->index_contents->ctree);
	gtk_clist_set_column_auto_resize (GTK_CLIST (hni->index_contents->ctree),
					  0 /* column */,
					  TRUE /* auto_resize */);

	hbox1 = gtk_hbox_new(TRUE, 4);
	hbox2 = gtk_hbox_new(FALSE, 4);
	
	label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(label),_("Select Al_l") );
	hni->all_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(hni->all_button), label);
	
	label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(label),_("Select _None"));
	hni->none_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(hni->none_button), label);

	ok_button = gtk_button_new_with_label(_("OK"));
	cancel_button = gtk_button_new_with_label(_("Cancel"));
	
	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			       hyperbola_navigation_index_OK_button, hni);
	gtk_signal_connect(GTK_OBJECT(hni->all_button), "clicked",
			       hyperbola_navigation_index_select_all_button, hni);
	gtk_signal_connect(GTK_OBJECT(hni->none_button), "clicked",
			       hyperbola_navigation_index_select_none_button, hni);
	gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
			       hyperbola_navigation_index_cancel_button, hni);
	hseparator = gtk_hseparator_new();

	gtk_box_pack_start( GTK_BOX(hbox1), hni->all_button, TRUE, TRUE, 0);
	gtk_box_pack_start( GTK_BOX(hbox1), hni->none_button, TRUE, TRUE, 0);
	gtk_box_pack_start( GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	gtk_box_pack_start( GTK_BOX(vbox), hseparator, FALSE, FALSE, 0);
	gtk_box_pack_end( GTK_BOX(hbox2), cancel_button, FALSE, FALSE, 0);
	gtk_box_pack_end( GTK_BOX(hbox2), ok_button, FALSE, FALSE, 0);
	gtk_box_pack_start( GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

	hni->index_select_page = vbox;
	gtk_widget_show_all(hni->index_select_page);
}
/*
 * show_indexes_for_contents_selection_button
 * @GtkToggleButton *contents_rbutton
 * @HyperbolaNavigationIndex *hni
 * Callback for "Selection on Contents tab" radio button.
 * Parses the index files selected on contents page, and displays
 * these terms in the hni->clist
 */
static void
show_indexes_for_contents_selection_button(GtkToggleButton *contents_rbutton, 
					HyperbolaNavigationIndex *hni)
{
	if (hni->index_type != CONTENTS_SELECTED_INDEX_FILES){
		hni->index_type = CONTENTS_SELECTED_INDEX_FILES;
		hyperbola_navigation_index_find_contents_selected_index_files(hni);
		hyperbola_navigation_index_parse_index_files(hni, 
					CONTENTS_SELECTED_INDEX_FILES);
		hyperbola_navigation_index_create_clist(hni); 
	}
}
/*
 * show_indexes_for_all_docs_button
 * @GtkToggleButton *all_rbutton
 * @HyperbolaNavigationIndex *hni
 * Callback for "All documents" radio button.
 * Parses all the index files if not already done, and displays
 * these terms in the hni->clist
 */
static void
show_indexes_for_all_docs_button(GtkToggleButton *all_rbutton, 
					HyperbolaNavigationIndex *hni)
{
	if (hni->index_type != ALL_INDEX_FILES){
		hni->index_type = ALL_INDEX_FILES;
		/* hni->all_index_items only needs to be set up once as it
		 * does not change
		 */
		if(!hni->all_index_items){
			hyperbola_navigation_index_parse_index_files(hni, 
					ALL_INDEX_FILES);
		}
		hyperbola_navigation_index_create_clist(hni);
	}
}
/*
 * show_indexes_for_specific_docs_button
 * @GtkToggleButton *specific_rbutton
 * @HyperbolaNavigationIndex *hni
 * Callback for "Specific documents" radio button.
 * Parses those index files selected from index_select_page, and displays
 * these terms in the hni->clist
 */
static void
show_indexes_for_specific_docs_button(GtkToggleButton *specific_rbutton, 
					HyperbolaNavigationIndex *hni)
{
	if (hni->index_type != INDEX_SELECTED_INDEX_FILES){
		hni->index_type = INDEX_SELECTED_INDEX_FILES;
		hyperbola_navigation_index_parse_index_files(hni, 
					INDEX_SELECTED_INDEX_FILES);
		hyperbola_navigation_index_create_clist(hni); 
	}
}
/*
 * show_index_for_all_terms
 * @GtkToggleButton *all_terms_rbutton: pointer to "Show all index terms"
 *                                     toggle button on index_display_page
 * @HyperbolaNaviagtionIndex *hni
 * When this button is toggled then must create index display for selected
 * documents, showing all terms.
 * The hni->show_all_terms is set to TRUE
 */
static void
show_index_for_all_terms(GtkToggleButton *all_terms_rbutton, 
					HyperbolaNavigationIndex *hni)
{
	hni->show_all_terms = TRUE;
	switch(hni->index_type){
		case CONTENTS_SELECTED_INDEX_FILES:
			hyperbola_navigation_index_create_clist(hni);
			break;
		case INDEX_SELECTED_INDEX_FILES:
			hyperbola_navigation_index_create_clist(hni);
			break;
		case ALL_INDEX_FILES:
			hyperbola_navigation_index_create_clist(hni);
			break;
	};
			
}
	
/*
 * show_index_for_selected_terms
 * @GtkToggleButton *specific_terms_rbutton:
 				pointer to "Show only terms containing"
 *                              toggle button on index_display_page.
 * @HyperbolaNaviagtionIndex *hni
 * When this button is toggled then must create index display for selected
 * documents, showing only terms specified in the GtkEntry box.
 * The hni->show_all_terms is set to FALSE
 */
static void
show_index_for_selected_terms(GtkToggleButton *all_rbutton, 
					HyperbolaNavigationIndex *hni)
{
	char *tmp;
	hni->show_all_terms = FALSE;
	/* check to see if there is text entered in the GtkEntry box */
	tmp = gtk_entry_get_text (GTK_ENTRY (hni->ent));
	/* only update if the gtkentry box is not empty */
	if (strcmp(tmp, "")){
		hyperbola_navigation_index_show_selected_terms(hni);
	}

}

/*
 * make_index_display_page
 * @HyperbolaNavigationIndex *hni
 * Creates the widget for the index_display_page, and assigns
 * it to hni->index_display_page.
 * The index_select_page is created when the "..." button is
 * clicked for the first time.
 * Returns a pointer to this widget.
 */
static GtkWidget *
make_index_display_page(HyperbolaNavigationIndex *hni)
{
	GtkWidget *vbox, *top_vbox, *top_hbox, *mid_vbox, *mid_hbox, *end_vbox;
	GtkWidget *hseparator;
	GSList *radio_group;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *underlined_label;
	
	vbox = gtk_vbox_new (FALSE, 0);
	top_vbox = gtk_vbox_new (FALSE, 0);
	top_hbox = gtk_hbox_new (FALSE, 0);
	mid_vbox = gtk_vbox_new (FALSE, 0);
	mid_hbox = gtk_hbox_new (FALSE, 0);
	end_vbox = gtk_vbox_new (FALSE, 0);

	label = gtk_label_new(_("Show indexes for:"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD_SMALL, 0);
	gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);

	underlined_label = gtk_label_new("");
	
	gtk_label_parse_uline(GTK_LABEL(underlined_label),
				   	_("Selection on Contents _tab"));
	gtk_misc_set_alignment(GTK_MISC(underlined_label), 0.0, 0.5);
	hni->contents_rbutton = gtk_radio_button_new(NULL);
	gtk_container_add(GTK_CONTAINER(hni->contents_rbutton),
						GTK_WIDGET(underlined_label));
	
	radio_group = gtk_radio_button_group(GTK_RADIO_BUTTON(hni->contents_rbutton));
	
	underlined_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(underlined_label), _("_All documents"));
	gtk_misc_set_alignment(GTK_MISC(underlined_label), 0.0, 0.5);
	hni->all_rbutton = gtk_radio_button_new(radio_group);
	gtk_container_add(GTK_CONTAINER(hni->all_rbutton),
					GTK_WIDGET(underlined_label));
	radio_group = gtk_radio_button_group(GTK_RADIO_BUTTON(hni->all_rbutton));
	underlined_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(underlined_label),
				   	_("_Specific documents"));
	gtk_misc_set_alignment(GTK_MISC(underlined_label), 0.0, 0.5);

	hni->specific_rbutton = gtk_radio_button_new(radio_group);
	gtk_container_add(GTK_CONTAINER(hni->specific_rbutton),GTK_WIDGET(underlined_label));

	gtk_signal_connect (GTK_OBJECT (hni->contents_rbutton), "toggled",
				GTK_SIGNAL_FUNC(show_indexes_for_contents_selection_button),
					hni);
	gtk_signal_connect (GTK_OBJECT (hni->all_rbutton), "toggled",
				GTK_SIGNAL_FUNC(show_indexes_for_all_docs_button), hni);
	gtk_signal_connect (GTK_OBJECT (hni->specific_rbutton), "toggled",					GTK_SIGNAL_FUNC(show_indexes_for_specific_docs_button), hni);
	gtk_box_pack_start(GTK_BOX(top_vbox), hni->contents_rbutton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(top_vbox), hni->all_rbutton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(top_hbox),hni->specific_rbutton,TRUE, TRUE, 0);

	
	button = gtk_button_new_with_label("...");
	gtk_signal_connect(GTK_OBJECT(button),"clicked",make_index_select_page,hni);

	gtk_box_pack_end(GTK_BOX(top_hbox), button, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(top_vbox), top_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), top_vbox, FALSE, FALSE, 0);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, FALSE, 5);

	underlined_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(underlined_label),
					_("S_how all index terms"));
 
	gtk_misc_set_alignment(GTK_MISC(underlined_label), 0.0, 0.5);
	hni->all_terms_rbutton = gtk_radio_button_new(NULL);
	gtk_container_add(GTK_CONTAINER(hni->all_terms_rbutton),GTK_WIDGET(underlined_label));
	gtk_signal_connect (GTK_OBJECT (hni->all_terms_rbutton), "toggled",
					GTK_SIGNAL_FUNC(show_index_for_all_terms), hni);
	gtk_box_pack_start(GTK_BOX(mid_vbox), hni->all_terms_rbutton,FALSE,FALSE,0);
	radio_group = gtk_radio_button_group(GTK_RADIO_BUTTON(hni->all_terms_rbutton));
	underlined_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(underlined_label),
					_("Show _only terms containing"));
	gtk_misc_set_alignment(GTK_MISC(underlined_label), 0.0, 0.5);
	hni->specific_terms_rbutton = gtk_radio_button_new( radio_group);
	gtk_container_add(GTK_CONTAINER(hni->specific_terms_rbutton),GTK_WIDGET(underlined_label));
	gtk_signal_connect (GTK_OBJECT (hni->specific_terms_rbutton), "toggled",
                        GTK_SIGNAL_FUNC(show_index_for_selected_terms), hni);
	gtk_box_pack_start(GTK_BOX(mid_vbox), hni->specific_terms_rbutton, FALSE, FALSE, 0);

	hni->ent = gtk_entry_new();
	gtk_signal_connect (GTK_OBJECT (hni->ent), "changed",
				hyperbola_navigation_index_ent_changed, hni);
	gtk_signal_connect (GTK_OBJECT (hni->ent), "activate",
				hyperbola_navigation_index_ent_activate, hni);
	gtk_container_add (GTK_CONTAINER (mid_hbox), hni->ent);

	underlined_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(underlined_label), _("Sho_w"));
	hni->show_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(hni->show_button),GTK_WIDGET(underlined_label));
	gtk_signal_connect(GTK_OBJECT(hni->show_button), "clicked",
				hyperbola_navigation_index_show_button, hni);
	gtk_box_pack_start(GTK_BOX(mid_hbox), hni->show_button, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(mid_vbox), mid_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), mid_vbox, FALSE, FALSE, 0);
	
	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, FALSE, 5);

	underlined_label = gtk_label_new("");
	gtk_label_parse_uline( GTK_LABEL(underlined_label), _("Inde_x:"));
	gtk_misc_set_alignment (GTK_MISC (underlined_label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (underlined_label), GNOME_PAD_SMALL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), underlined_label, FALSE, FALSE, 1);

	hni->clist = gtk_clist_new (1);
	gtk_clist_freeze (GTK_CLIST (hni->clist));
	gtk_clist_set_selection_mode (GTK_CLIST (hni->clist), GTK_SELECTION_BROWSE);
	/*
	 * The default is to display an index for the docs selected on
	 * the contents page
	 */
	hyperbola_navigation_index_create_clist (hni);

	hni->wtmp = gtk_scrolled_window_new (gtk_clist_get_hadjustment
					(GTK_CLIST(hni->clist)),
					 gtk_clist_get_vadjustment (GTK_CLIST (hni-> clist)));
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (hni->wtmp),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start (GTK_BOX(end_vbox), hni->wtmp, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (hni->wtmp), hni->clist);

	gtk_clist_columns_autosize (GTK_CLIST (hni->clist));
	gtk_clist_thaw (GTK_CLIST (hni->clist));

	gtk_box_pack_start(GTK_BOX(vbox), end_vbox, TRUE, TRUE, 0);
	hni->index_display_page = vbox;
	gtk_widget_show_all(hni->index_display_page);
	
	return (vbox);
}
/*
 * hyperbola_navigation_notebook_page_changed
 * @GtkNotebook *notebook
 * @GtkNotebookPage *page
 * @gint page_num : number of page user is switching to (0 is contents
 * page, 1 is index_display_page, 2 is index_select_page)
 * @HyperbolaNavigationIndex *hni
 * Controls the pages that will be displayed to user on switching from
 * Contents page to Index page.
 * Also sets hni->page_type to indicate what page is now current.
 * If contents_rbutton is currently on, then this function
 * also ensures that contents_selected_index_files is updated, and that
 * the displayed index terms are updated accordingly.
 */
void hyperbola_navigation_notebook_page_changed (GtkNotebook *notebook,
                                   GtkNotebookPage *page,
                                   gint page_num,
                                   HyperbolaNavigationIndex *hni)
{      
  	/* 
	 * When user is switching from contents page to the index
	 * page, the index_display_page should be displayed - not
	 * the index_select_page.
	 * If the index_select_page is current (ie page_num now == 2)
	 * then redraw the index_display_page.
	 */

	if(page_num > 0 && hni->index_type == CONTENTS_SELECTED_INDEX_FILES 
					&& hni->page_type == CONTENTS_PAGE){ 
		/* User is switching to index page and "Selection on Contents Tab"
		 * radio button is selected, then create the index for the selected
		 * files in the content's page tree
		 */ 
		hyperbola_navigation_index_find_contents_selected_index_files(hni);
		hyperbola_navigation_index_parse_index_files(hni, 
							    CONTENTS_SELECTED_INDEX_FILES);
		hyperbola_navigation_index_create_clist(hni); 
		
		if(page_num == 2 && hni->page_type == CONTENTS_PAGE){
			/* If switching from Contents page and index_select_page is 
			 * current, switch to index_display_page - makes more sense for 
			 * user (I think)
			 */
			redraw_index_display_page(hni);
		} else if(page_num == 1){
			/* Current page is now index_display_page */
			hni->page_type = INDEX_DISPLAY_PAGE;
		}

	} else if (page_num == 2){
		/* Switching to index_select_page */
		hni->page_type = INDEX_SELECT_PAGE;
	} else if (page_num == 1){
		/* Switching to index_display_page */
		hni->page_type = INDEX_DISPLAY_PAGE;
	} else if (page_num == 0){
		/* Switching to Contents Page so set hni->page_type */ 
		hni->page_type = CONTENTS_PAGE;
	}
		
}
/*
 * redraw_index_display_page
 * @HyperbolaNavigationIndex *hni
 * Hides the index_select_page and shows the index_display_page.
 * Sets the current page (hni->page_type) to INDEX_DISPLAY_PAGE.
 */

static void
redraw_index_display_page (HyperbolaNavigationIndex *hni)
{
	gtk_widget_hide(hni->index_select_page);
	gtk_widget_show(hni->index_display_page);
	gtk_notebook_set_page(GTK_NOTEBOOK(hni->notebook), 1);	
	hni->page_type = INDEX_DISPLAY_PAGE;
}

/*
 * hyperbola_navigation_index_unselect_children
 * @const char *key : the current key value
 * @HyperbolaTreeNode *tnode : the current node
 * MoreIndexFiles *midx_files : data necessary for processing
 * Traversal function used when a row of the contents tree on
 * the index_select_page is unselected - if a row is unselected
 * then its children must be unselected also.
 * Removes the corresponding index files from the 
 * hni->index_selected_index_files hashtable.
 * Returns 0 to indicate traversal should be continued.
 */

static int
hyperbola_navigation_index_unselect_children(const char *key, 
											HyperbolaTreeNode *tnode,
											MoreIndexFiles *midx_files)
{
	IndexFiles *idx_files = midx_files->idx_files;
	MoreIndexFiles sub_midx_files = *midx_files;
	HyperbolaNavigationIndex *hni = idx_files->hni;
	char *doc_uri = NULL;
	gpointer orig_key, orig_value;

	if(tnode->uri){
		doc_uri = g_strdup(tnode->uri);
		if(g_hash_table_lookup_extended(hni->index_selected_index_files, 
							doc_uri, &orig_key, &orig_value)){
			g_hash_table_remove(hni->index_selected_index_files, doc_uri);
			g_free(orig_key);
		}
		g_free(doc_uri);
	}
	if(tnode->children)
		g_tree_traverse (tnode->children, (GTraverseFunc)
						hyperbola_navigation_index_unselect_children,
						G_IN_ORDER, &sub_midx_files);

	return 0;

}

/*
 * hyperbola_navigation_index_tree_unselect_row
 * @GtkCTree * ctree: the index tree
 * @GtkCTreeNode *node : the current node
 * @gint column: column number (always 0)
 * @HyperbolaNavigationIndex *hni
 * Callback for when a row is unselected in the contents tree
 * on the index_select_page.
 */
static void
hyperbola_navigation_index_tree_unselect_row (GtkCTree * ctree, 
										GtkCTreeNode * node,
										gint column,
										HyperbolaNavigationIndex * hni)
{
	HyperbolaTreeNode *tnode;
	char *key;
	gpointer orig_key, orig_value;
	IndexFiles idx_files;
	MoreIndexFiles midx_files;

	memset (&idx_files, 0, sizeof (idx_files));
	memset (&midx_files, 0, sizeof (midx_files));
	idx_files.hni = hni;
	midx_files.idx_files = &idx_files;

	tnode = gtk_ctree_node_get_row_data (ctree, node);

	if (!tnode)
		return;
	
	if(tnode->uri){
		key = g_strdup(tnode->uri);
		if(g_hash_table_lookup_extended(hni->index_selected_index_files, key,
						&orig_key, &orig_value)){
			g_hash_table_remove(hni->index_selected_index_files, key);
			g_free(orig_key);
		}
		g_free(key);
	}
	if(tnode->children)
		g_tree_traverse (tnode->children, (GTraverseFunc)
						hyperbola_navigation_index_unselect_children,
						G_IN_ORDER, &midx_files);


}

/* hyperbola_navigation_index_find_selected_index_files
 * @const char *key : the current key value
 * @HyperbolaTreeNode *tnode : the current node
 * MoreIndexFiles *midx_files : data necessary for processing
 * A tree traversal function for traversing a specified tree and
 * adding the index files found to the relevant hashtable (as determined
 * by the value of hni->index_type
 * Returns 0 to indicate continued traversal of tree.
 */

static int
hyperbola_navigation_index_find_selected_index_files(const char *key, 
													HyperbolaTreeNode *tnode,
													MoreIndexFiles *midx_files)
{
	int row_num;
	IndexFiles *idx_files = midx_files->idx_files;
	MoreIndexFiles sub_midx_files = *midx_files;
	char *doc_uri = NULL, *index = NULL;
	HyperbolaNavigationIndex *hni = idx_files->hni;
	GtkWidget *ctree, *index_files;

	if (idx_files->index_type == CONTENTS_SELECTED_INDEX_FILES){
		ctree = (GtkWidget *)hni->contents_ctree;
		index_files = (GtkWidget *)hni->contents_selected_index_files;
	} else{
		ctree = (GtkWidget *)hni->index_contents->ctree;
	        index_files = (GtkWidget *)hni->index_selected_index_files;
	}

	row_num = gtk_clist_find_row_from_data(GTK_CLIST(ctree), tnode);
	
	if(tnode->uri){
		doc_uri = g_strdup(tnode->uri);
		index = g_strdup(g_hash_table_lookup(
		hni->index_contents->doc_tree->all_index_files, doc_uri));
		if (index){
			g_hash_table_insert((GHashTable *)index_files, doc_uri, index);
		}
	}
	if(tnode->children)
		g_tree_traverse (tnode->children, (GTraverseFunc)
					hyperbola_navigation_index_find_selected_index_files,
					G_IN_ORDER, &sub_midx_files);
	return 0;

}
 
/*
 * hyperbola_navigation_index_find_contents_selected_index_files
 * @HyperbolaNavigationIndex *hni
 * Examines the contents trees and finds which documents are selected,
 * and inserts these into hni->contents_selected_index_files
 */
static void
hyperbola_navigation_index_find_contents_selected_index_files(HyperbolaNavigationIndex *hni)
{
	int top_row_num, row_num;
	char *doc_uri, *index;
	HyperbolaTreeNode *top_tnode, *tnode;
	IndexFiles idx_files;
   	MoreIndexFiles midx_files;

	memset (&idx_files, 0, sizeof (idx_files));
	memset (&midx_files, 0, sizeof (midx_files));

	if(hni->contents_top_ctree){
		/* If the top_ctree exists it will contain the Introductory
		 * Documents only (ie individual documents or contents sections)
		 * so there will be no children of a selected node to be included
	 	 * (ie user will select a document or a TOC section)
		 */
		top_row_num = GTK_CLIST(hni->contents_top_ctree)->focus_row;
		/* if a node is selected */
	        if (top_row_num >= 0){
        	   	top_tnode = (HyperbolaTreeNode *)gtk_clist_get_row_data (GTK_CLIST
						(hni->contents_top_ctree), top_row_num);
			if(top_tnode->uri){
           			doc_uri = g_strdup(top_tnode->uri);
           			index = g_strdup(g_hash_table_lookup
		        	(hni->index_contents->doc_tree->all_index_files, doc_uri));
            			if (index){
               				g_hash_table_insert(hni->contents_selected_index_files,
                                                        doc_uri, index);
               			}
        		}
		}
	}
	
	row_num = GTK_CLIST(hni->contents_ctree)->focus_row;

	if (row_num >= 0){
		idx_files.hni = hni;
		idx_files.index_type = CONTENTS_SELECTED_INDEX_FILES;
		midx_files.idx_files = &idx_files;
		tnode = (HyperbolaTreeNode *)gtk_clist_get_row_data (GTK_CLIST
				(hni->contents_ctree), row_num);
		if(tnode->uri){
			doc_uri = g_strdup(tnode->uri);
			index = g_strdup(g_hash_table_lookup	
			(hni->index_contents->doc_tree->all_index_files, doc_uri));
			if (index){
				g_hash_table_insert(hni->contents_selected_index_files,
                                                       doc_uri, index);
			}
		}
		if(tnode->children)
			g_tree_traverse (tnode->children, (GTraverseFunc)
                       hyperbola_navigation_index_find_selected_index_files,
                       G_IN_ORDER, &midx_files);
    }


}

/* 
 * This function is needed because I do not want to emit "tree_select_row" 
 * signal when selecting the child nodes. I tried blocking the signal and 
 * then calling gtk_ctree_select_recursive but that did not work, the signal
 *  wasnt blocked for each selected child. 
 */ 
static void 
block_select_unblock(GtkCTree *ctree, GtkCTreeNode *node, gpointer data)
{
		gtk_signal_handler_block(GTK_OBJECT(ctree), select_handler_id); 
		gtk_ctree_select(ctree, node);
		gtk_signal_handler_unblock(GTK_OBJECT(ctree), select_handler_id); 
}
/*
 * hyperbola_navigation_index_tree_select_row
 * @GtkCTree * ctree : the index_select_page contents tree
 * @GtkCTree * node : the node selected
 * gint column : always 0 for this tree
 * @HyperbolaNavigationIdex *hni
 * The callback for selection of a row in the index_select_page
 * contents tree. Finds the document (or group of documents) for
 * this selection and adds them to hni->index_selected_index_files
 */

static void
hyperbola_navigation_index_tree_select_row (GtkCTree * ctree,
 					GtkCTreeNode * node,
					gint column,
					HyperbolaNavigationIndex * hni)
{
	char *doc_uri, *index;
	HyperbolaTreeNode *tnode;
	IndexFiles idx_files;
	MoreIndexFiles midx_files;

	memset (&idx_files, 0, sizeof (idx_files));
	memset (&midx_files, 0, sizeof (midx_files));

	idx_files.hni = hni;
	idx_files.index_type = INDEX_SELECTED_INDEX_FILES;
	midx_files.idx_files = &idx_files;
	
	tnode = gtk_ctree_node_get_row_data (ctree, node);
	
	gtk_ctree_pre_recursive(ctree, node, block_select_unblock, NULL);

	if(tnode->uri){
		doc_uri = g_strdup(tnode->uri);
		index = g_strdup(g_hash_table_lookup
				(hni->index_contents->doc_tree->all_index_files, doc_uri));
		if (index){
			g_hash_table_insert(hni->index_selected_index_files, doc_uri,index);
		}
	}
	if(tnode->children)
		g_tree_traverse (tnode->children, (GTraverseFunc)
                          hyperbola_navigation_index_find_selected_index_files,
                          G_IN_ORDER, &midx_files);


}


/*
 * make_index_page
 * @HyperbolaNavigationIndex *hni
 * Creates the widget for the GtkNotebook index page.
 * The index page will be either:
 * the index_display_page - the main page displaying an index
 * the index_select_page - this displays a contents tree to allow
 * the user to select documents whose indexes will then be displayed 
 * on the index_display_page.
 * This function creates the index_display_page widget with a call to
 * make_index_display_page(). 
 * Returns a pointer to this widget.
 */

GtkWidget *
make_index_page (HyperbolaNavigationIndex *hni)
{
	GtkWidget *vbox;
	hni->all_index_items = g_tree_new ((GCompareFunc) g_strcasecmp);
	hni->index_selected_index_items = g_tree_new ((GCompareFunc) g_strcasecmp);
	hni->contents_selected_index_items = g_tree_new ((GCompareFunc) g_strcasecmp);

	hni->index_selected_index_files = g_hash_table_new(g_str_hash, g_str_equal);
	hni->contents_selected_index_files = g_hash_table_new(g_str_hash, g_str_equal);
	hni->index_contents->doc_tree = hyperbola_doc_tree_new();
	hni->index_contents->doc_tree->contents_tree_type = FALSE;
	hyperbola_doc_tree_populate(hni->index_contents->doc_tree);

	hni->index_contents->ctree = gtk_ctree_new(1, 0);
	gtk_ctree_set_line_style (GTK_CTREE (hni->index_contents->ctree),
                                  GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style (GTK_CTREE (hni->index_contents->ctree),
                                      GTK_CTREE_EXPANDER_TRIANGLE);

	gtk_clist_freeze (GTK_CLIST (hni->index_contents->ctree));
	gtk_clist_set_selection_mode (GTK_CLIST (hni->index_contents->ctree),
                                      GTK_SELECTION_EXTENDED);
	gtk_clist_thaw(GTK_CLIST (hni->index_contents->ctree));
	select_handler_id = gtk_signal_connect (GTK_OBJECT (
							hni->index_contents->ctree), "tree_select_row",
                            hyperbola_navigation_index_tree_select_row, hni);
	gtk_signal_connect (GTK_OBJECT (
							hni->index_contents->ctree), "tree_unselect_row",
                            hyperbola_navigation_index_tree_unselect_row, hni);
	gtk_signal_connect (GTK_OBJECT (hni->index_contents->ctree), "destroy",
                            hyperbola_navigation_tree_destroy, hni->index_contents);
	
	/* The default is for Selection on Contents tab to be selected */
	hyperbola_navigation_index_find_contents_selected_index_files(hni);
	hni->index_type = CONTENTS_SELECTED_INDEX_FILES;
	hyperbola_navigation_index_parse_index_files(hni,
		 		CONTENTS_SELECTED_INDEX_FILES);

	/*
	 * Do the parse of all the index files here on startup so not necessary later.
	 * (ie populates all_index_items)
	 */
	hyperbola_navigation_index_parse_index_files(hni,
					  ALL_INDEX_FILES);

	index_ctree_populate(hni);

	vbox = make_index_display_page(hni);
	
	/* on creation of the notebook the contents_page is current */
	hni->page_type = CONTENTS_PAGE;

        return (vbox);
}
#endif
