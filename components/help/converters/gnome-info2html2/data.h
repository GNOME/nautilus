#ifndef DATA_H
#define DATA_H

/* data.h - first cut at data structures for info2html filter */
/* many of these are motivated by the source code to the 'info' program */

/* file we're working on */
char work_filename[1024];

/* node we're working on */
char work_node[1024];

/* some type's we'll want to use below */
typedef struct info_menu_entry MENU_ENTRY;

/* the basic component of an info file is a Node */
/* a node is described by (FILENAME)NODENAME */
/* .next and .prev are normally along the same branch as current node */
/* .up is normally 'one branch' up the tree above current branch. */
/* All can be arbitrary links however                             */
/* menu entry is just a linked list of references */

typedef struct {
  char *filename;                /* file in which this node exists */
  char *nodename;                /* name of this node */
  char *contents;                /* text within this node */
  int  contlen;                  /* length of contents */
  char *next;                    /* node which follows this one */
  char *prev;                    /* node previous to this one */
  char *up;                      /* node above this one */
  MENU_ENTRY *menu;              /* linked list of refs from this node */
  char *menu_start;              /* ptr to start of menu text in contents */
} NODE;

/* a reference is a link to a node */
typedef struct {
  char *refname;                  /* menu name for reference */
  NODE *node;                     /* descriptor of node we point at */
} REFERENCE;


struct info_menu_entry{
  char          *header;          /* header to go before menu */
  REFERENCE     *ref;
  struct info_menu_entry    *next;
};

#define INFO_FF '\014'
#define INFO_COOKIE '\037'


#define MENU_START "* Menu:"
#define MENU_ENTRY "* "


#endif /* DATA_H */
