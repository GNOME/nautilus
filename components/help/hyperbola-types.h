#ifndef HYPERBOLA_TYPES_H
#define HYPERBOLA_TYPES_H 1

#include <gtk/gtkwidget.h>
#include <stdio.h>

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
#include <gtk/gtk.h>
#include <gtk/gtkctree.h>
#include <gtk/gtknotebook.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus/libnautilus.h>
#include <ctype.h>
#endif

/* Be a URL, for now */
typedef const char *HyperbolaLocationReference;

typedef struct _HyperbolaTreeNode HyperbolaTreeNode;
 
typedef enum {
	HYP_TREE_NODE_FOLDER,
	HYP_TREE_NODE_SECTION,
	HYP_TREE_NODE_BOOK,
	HYP_TREE_NODE_PAGE
} HyperbolaTreeNodeType;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/*
 * HyperbolaIndexSelection is used to flag the type of index to be built, ie from:
 * all files; index files selected from the index page's contents tree; or
 * index files selected from the contents page's contents tree.
 */
typedef enum { 	
	ALL_INDEX_FILES, 
	INDEX_SELECTED_INDEX_FILES, 	
	CONTENTS_SELECTED_INDEX_FILES	
} HyperbolaIndexSelection;

typedef enum {
	CONTENTS_PAGE,
	INDEX_DISPLAY_PAGE,
	INDEX_SELECT_PAGE
}HyperbolaPageType;

#endif

struct _HyperbolaTreeNode {
	HyperbolaTreeNode *up;

	HyperbolaTreeNodeType type;

	char *title;
	char *uri;

	GTree *children;	/* By title (relative to parent) */

	gpointer user_data;
};

typedef struct {
	GTree *children;	/* By title */

	GHashTable *global_by_uri;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	GHashTable *all_index_files; /* Stores all index files, key is doc uri */

	gboolean contents_tree_type; /* If true, then it is a contents tree 
                                      * for the contents page, if false, then
                                      * it is a contents tree for the index page 
				      */
#endif
	gpointer user_data;
} HyperbolaDocTree;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/*
 * if scrollkeeper support not enabled then the equivalents of 
 * these structs (plus/minus some members) are defined in 
 * hyperbola-nav-tree.c and hyperbola-nav-index.c
 */
typedef struct {
        NautilusView *view_frame;
	GtkWidget *contents_widget;
        GtkWidget *ctree;
        HyperbolaDocTree *doc_tree;
        GtkWidget *top_ctree;
        HyperbolaDocTree *top_doc_tree;
        GtkWidget *selected_ctree;
        GtkCTreeNode *selected_node;
        int select;
        char *pending_location;

        gint notify_count;

        GdkPixmap *folder_open_pixmap;
        GdkBitmap *folder_open_mask;
        GdkPixmap *folder_closed_pixmap;
        GdkBitmap *folder_closed_mask;

        GdkPixmap *book_open_pixmap;
        GdkBitmap *book_open_mask;
        GdkPixmap *book_closed_pixmap;
        GdkBitmap *book_closed_mask;

        GdkPixmap *section_open_pixmap;
        GdkBitmap *section_open_mask;
        GdkPixmap *section_closed_pixmap;
        GdkBitmap *section_closed_mask;
} HyperbolaNavigationTree;

typedef struct {
        NautilusView *index_view_frame;

	HyperbolaNavigationTree *index_contents;

	GtkWidget *notebook;
	
	GtkWidget *index_display_page;

	GtkWidget *contents_rbutton;
	GtkWidget *specific_rbutton;
	GtkWidget *all_rbutton;
	GtkWidget *all_terms_rbutton;
	GtkWidget *specific_terms_rbutton;
	GtkWidget *show_button;
	GtkWidget *wtmp;

	GtkWidget *index_select_page;
        /* the index page will need pointers to the contents page's
	 * top_ctree and ctree
	 */
	GtkWidget *all_button; 
	GtkWidget *none_button; 
	GtkWidget *index_select_win;

	GtkWidget *contents_ctree;

	GtkWidget *contents_top_ctree;

        GtkWidget *clist, *all_index_terms_clist, *ent;

	GTree *current_index_items;

        GTree *all_index_items;

        GTree *index_selected_index_items;

        GTree *contents_selected_index_items;

	GHashTable *index_selected_index_files;

	GHashTable *contents_selected_index_files;

	HyperbolaIndexSelection index_type;

	gboolean index_terms_found; /* if true, index entry found for clist */

	gboolean show_all_terms;  /* If true then show all index terms, if false then 
				   * show only selected index terms.
				   */
	HyperbolaPageType page_type;

        gint8 notify_count;
	
} HyperbolaNavigationIndex;

typedef struct {
        HyperbolaNavigationTree *view;
        GtkWidget *ctree;
        GtkCTreeNode *sibling, *parent;
} PopulateInfo;
typedef struct {
        HyperbolaNavigationIndex *hni;
        GtkCTreeNode *sibling, *parent;
} IndexPopulateInfo;

#endif

#endif
