/* simple info file parser. */
/* currently only finds nodes and contructs a tree */
/* partially motivated by source code of the 'info' program */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include "parse.h"
#include "data.h"
#include "utils.h"

int num_files_left;

/* main routine to read in a node once we found its start in file f */
/* assumes NODE has already been allocated */
int read_node (gzFile f, char *line, NODE *node)
{
  /* we found a node line */
  if (!parse_node_line( node, line ))
    return (READ_ERR);

  /* read in the contents, they go until a '\014', or a EOF */
  return (read_node_contents( f, node ));
}

/* take a node definition and parse components */
/* assumes that NODE has already been allocated */
/* returns pointer to node if ok, or NULL on failure */
NODE *parse_node_line( NODE *node, char * line )
{
  char *result;
  char *temp;

  /* fill in rest soon */
  node->filename  = NULL;
  node->nodename  = NULL;
  node->contents  = NULL;
  node->next      = NULL;
  node->prev      = NULL;
  node->up        = NULL;

  temp = line;

  /* have trouble on (dir) file which has a slightly diferrent 'File:' line */
  /* so currently we have a hack here                                       */
  if (!(result=parse_node_label( &temp, "File:", 0)))
    return NULL;
  node->filename = result;

  /* don't allow_eof if we are looking at the 'dir' file, its a special case */
  if (!(result=parse_node_label(&temp,"Node:",strcmp(node->filename, "dir"))))
    return NULL;
  node->nodename = result;

  /* not clear any of the rest are actually necessary */
  /* keep eye out for hitting end of line             */
  if ((result=parse_node_label( &temp, "Next:", 1)))
    node->next = result;

  if ((result=parse_node_label( &temp, "Prev:", 1)))
    node->prev = result;

  if ((result=parse_node_label( &temp, "Up:", 1)))
    node->up = result;

  /* cleanup node filename */
  fixup_info_filename( node->filename );

  return node;
}

/* grab node name from the label 'label' */
/* NULL means it doesn't exist */
/* strdup's a string and returns it, which can be freed */
char *parse_node_label( char **line, char *label, int allow_eof )
{
  char *start, *end;
  char *temp;

  temp = strdup( *line );

  start = strstr( temp, label );
  if (start == NULL)
    return NULL;

  start += strlen(label);
  if (allow_eof)
    {
      end = strstr( start, "," );
      if (end == NULL)
	end = strstr( start, "\n" );
    }
  else {
    end = strstr( start, "," );
    if (!end)
	    end = strstr( start, "\t" ); /* might help (dir) files */
  }

  if (end == NULL)
    return NULL;

  *end = '\000';
  strip_spaces( start );
  return start;
}
  

/* from current position in file f, read till we hit EOF or */
/* a end of node marker. Allocate space and set ptr to point at it */
/* contents of node->contents is a mirror image of what was in  */
/* the node (menus and all */
/* assumes that NODE is already allocated */
#define SEARCH_BUF_SIZE 1024
#define CONTENTS_BUF_INCR 1024

int read_node_contents( gzFile f, NODE *node )
{
  int nread;
  int found;
  char *searchbuf;
  char *ptr;

  char *tmpcontents;
  int  tmpcontlen;
  int  linelen;

  char *status;

  searchbuf = (char *) malloc( SEARCH_BUF_SIZE );
  tmpcontents = (char *) malloc( CONTENTS_BUF_INCR );
  tmpcontlen = CONTENTS_BUF_INCR;
  nread = 0;

  /* we read until we hit a '\014' or the end of file */
  /* since its coming form a pipe we read up to EOL   */
  /* and save contents as we go along                 */
  for ( found=0 ; !found ; )
    {
      status=gzgets(f,  searchbuf, SEARCH_BUF_SIZE);
      linelen = strlen( searchbuf );
      for (found=0, ptr = searchbuf; *ptr && !found; ptr++)
	if (*ptr == INFO_FF || *ptr == INFO_COOKIE)
	    found=1;

      /* if we didn't find the magic character, but hit eof, same deal */
      if (!found && gzeof(f))
	{
	  found = 1;
	  continue;
	}

      if ((nread+linelen+2) > tmpcontlen)
	{
	  tmpcontlen += CONTENTS_BUF_INCR;
	  tmpcontents = realloc( tmpcontents, tmpcontlen );
	}

      memcpy(tmpcontents+nread, searchbuf, linelen);
      nread += linelen;
      if (!gzeof(f) || num_files_left)
	*(tmpcontents+nread) = '\14';
    }

/*tmpcontents=realloc(tmpcontents, nread); */
  node->contlen = nread;
  node->contents = tmpcontents;
  if (searchbuf)
    free(searchbuf);
  else
    fprintf(stderr, "For some reason searchbuf is NULL\n");

  return READ_OK;
}

/* given pointer to a string, tells us if its the line following */
/* a info header line        */
/* ex.                       */
/*  This is a header         */
/*  ================         */
/* r points at the row of '='*/
int is_a_hdr_line ( char *r )
{
 return (!strncmp(r, "***", 3) || 
	 !strncmp(r, "===", 3) || 
	 !strncmp(r, "---", 3) ||
	 !strncmp(r, "...", 3));
}


/* returns 0 if good line found, non-0 otherwise */
int parse_menu_line( char *line, char **refname, char **reffile,
                     char **refnode, char **end_of_link,
		     int span_lines)
{
  char *match;
  char *match2;
  char *start;
  char *end_of_line;
  char *end_refnode;
  

  start = line;
  end_of_line = strchr( line, '\n' );
  *end_of_link = NULL;

  /* ok, we found a menu line, have to convert to a reference */
  /* four types we worry about */
  /* only care about stuff up to '.' OR second ':'            */
  /* 1) 'Link::                   This is a link'             */
  /* 2) 'Link: (file).            This is another link'       */
  /* 3) 'Link: (file)node.        This is yet another'        */
  /* 4) 'Link:                    node.'                      */
      
  /* found a variation on #4 in amdref.info-2 !               */
  /* 5) 'Link:                    node:                       */

  /* find the refname */
  if (!(match=copy_to_anychar( start, ":", refname )))
    return -1;
  strip_spaces( *refname );
  strip_dupspaces( *refname );
  match++;

  /* simple case is case #1 above */
  if (*match == ':')
    {
      *reffile = NULL;
      *refnode = NULL;
      *end_of_link = match;
      return 0;
    }
  else
    {
      /* look for parentheses */
      match2 = strchr( match, '(' );
      
      /* this means it must be form 4 */
      /* but dont look too far away */
      /* this is a real hack - should do something more intelligent that 10 chars */
      if (!match2 || (match2 - match) > 10)
	{
	  /* look for a ':' or '.' ending node */
#if 0
	  if (!(end_refnode=copy_to_anychar( match, ".", refnode )))
	    if (!(end_refnode=copy_to_anychar( match, ":", refnode )))
	      return -1;
	  /* but it cant be past end of the menu line */
	  if (end_refnode > end_of_line && !span_lines)
	    return -1;
#endif
	  /* span_lines is ignored now we have parse_note_ref() */
	  if (!(end_refnode=copy_to_anychar( match, "\n,.", refnode )))
	    return -1;
	  *end_of_link = end_refnode;
	  strip_spaces( *refnode );
	  strip_dupspaces( *refnode );
	  if ( *refnode == '\000')
	    {
	      free(refnode);
	      refnode = NULL;
	    }
	  *reffile = NULL;
	  return 0;
	}
      else
	{
	  match2++;
	  if (!(match=copy_to_anychar (match2, ")", reffile)))
	    return -1;
	  strip_spaces( *reffile );
	  fixup_info_filename( *reffile );
	  match++;
	  /* unsure about having '\n' here */
	  if (!(match=copy_to_anychar (match, "\n.,", refnode)))
	    return -1;
	  *end_of_link = match;
	  strip_spaces( *refnode );
	  strip_dupspaces( *refnode );
	  strip_dupspaces( *refname );
	  if (!(**refnode))
	    *refnode = NULL;

	  return 0;
	}
    }
}

/* used for *note and *Note refs */
/* returns 0 if good line found, non-0 otherwise */
int parse_note_ref( char *line, char **refname, char **reffile,
                     char **refnode, char **end_of_link,
		     int span_lines)
{
  char *match;
  char *match2;
  char *start;
  char *end_of_line;
  char *end_refnode;
  

  start = line;
  end_of_line = strchr( line, '\n' );
  *end_of_link = NULL;

  /* ok, we found a menu line, have to convert to a reference */
  /* four types we worry about */
  /* only care about stuff up to '.' OR second ':'            */
  /* 1) 'Link::                   This is a link'             */
  /* 2) 'Link: (file).            This is another link'       */
  /* 3) 'Link: (file)node.        This is yet another'        */
  /* 4) 'Link:                    node.'                      */
      
  /* found a variation on #4 in amdref.info-2 !               */
  /* 5) 'Link:                    node:                       */

  /* find the refname */
  if (!(match=copy_to_anychar( start, ":", refname )))
    return -1;
  strip_spaces( *refname );
  strip_dupspaces( *refname );
  match++;

  /* simple case is case #1 above */
  if (*match == ':')
    {
      *reffile = NULL;
      *refnode = NULL;
      *end_of_link = match;
      return 0;
    }
  else
    {
      /* look for parentheses */
      match2 = strchr( match, '(' );
      
      /* this means it must be form 4 */
      /* but dont look too far away */
      /* this is a real hack - should do something more intelligent that 10 chars */
      if (!match2 || (match2 - match) > 10)
	{
	  /* look for a ',', ':' or '.' ending node */
#if 0
	  if (!(end_refnode=copy_to_anychar( match, ",", refnode )))
	    if (!(end_refnode=copy_to_anychar( match, ".", refnode )))
	      if (!(end_refnode=copy_to_anychar( match, ":", refnode )))
#endif
	  if (!(end_refnode=copy_to_anychar( match, ",.:", refnode )))
		return -1;

	  /* but it cant be past end of the menu line */
	  if (end_refnode > end_of_line && !span_lines)
	    return -1;

	  *end_of_link = end_refnode;
	  strip_spaces( *refnode );
	  strip_dupspaces( *refnode );
	  *reffile = NULL;
	  return 0;
	}
      else
	{
	  match2++;
	  if (!(match=copy_to_anychar (match2, ")", reffile)))
	    return -1;
	  strip_spaces( *reffile );
	  fixup_info_filename( *reffile );
	  match++;
	  if (!(match=copy_to_anychar (match, ".,", refnode)))
	    return -1;
	  *end_of_link = match;
	  strip_spaces( *refnode );
	  strip_dupspaces( *refnode );
	  strip_dupspaces( *refname );
	  if (!(**refnode))
	    *refnode = NULL;

	  return 0;
	}
    }
}


/* old version 1.0 stuff */
#if 0



void scan_for_menu( NODE *node )
{
  char *ptr;
  char *match;
  char *line;
  char *buf;
  char *refname, *reffile, *refnode;
  char *junk;

  MENU_ENTRY *head;
  
  int  size;
  int found;

  NODE *newnode;
  REFERENCE *newref;
  MENU_ENTRY *newentry;
  char menu_hdr[8192];
  char *menu_hdr_ptr;

  /* search for start of a menu */
  size = node->contlen;
  found = 0;
  for (ptr = node->contents ; ptr < (node->contents+node->contlen) && !found;) 
    {
      line = get_line_from_contents( ptr, size );
      match = strstr(line, MENU_START);
      if (match)
	{
	  node->menu_start=ptr;
	  found = 1;
	}
      size = size - strlen(line) - 1;
      ptr += strlen(line) + 1;
      free(line);
    }
  
  if (!found)
    return;

  /* we found a menu start, lets read menu in now */
  /* keep looking for entries till we hit a end-of-node condition */
  head = NULL;
  menu_hdr_ptr = menu_hdr;
  for ( ; ptr < (node->contents+node->contlen); )
    {
      buf = get_line_from_contents( ptr, size );
      line = buf;
      size = size - strlen(line) - 1;
      ptr += strlen(line) + 1;
      
      if (*line == '\000')
	{
	  free(buf);
	  continue;
	}

      if (*line == INFO_FF || *line == INFO_COOKIE)
	{
	  free(buf);
	  break;
	}

      /* see if its a new menu entry or not */
      if (*line != '*')
	{
#if 0
	  free(buf);
	  break;
#endif
	  if ( (*line != '=') && (*line != '#') )
	    {
	      memcpy(menu_hdr_ptr, line, strlen(line));
	      menu_hdr_ptr += strlen(line);
	      *menu_hdr_ptr = '\n';
	      menu_hdr_ptr++;
	      *menu_hdr_ptr = '\0';
	    }
	  free(buf);
	  continue;
	}
      else
	{
	  line += 2;

	  if (parse_menu_line( line, &refname, &reffile, &refnode, &junk, 0 ))
	    {
	      free(buf);
	      continue;
	    }

	  /* found the end of nodename, so make a new reference */
	  newref = (REFERENCE *) malloc( sizeof(REFERENCE) );
	  newref->refname = refname;

	  newentry = (MENU_ENTRY *) malloc( sizeof(MENU_ENTRY) );
	  newentry->ref = newref;
	  if (menu_hdr_ptr != menu_hdr)
	    {
	      newentry->header = strdup(menu_hdr);
	      menu_hdr_ptr = menu_hdr;
	    }
	  else
	    newentry->header = NULL;
	  newentry->next = NULL;

	  newnode = (NODE *) malloc( sizeof(NODE) );
	  newref->node = newnode;
	  newnode->next = newnode->prev = newnode->up = NULL;
	  if (refnode)
	    newnode->nodename = refnode;
	  else
	    newnode->nodename = strdup(refname);

	  if (reffile)
	    newnode->filename = reffile;
	  else
	    newnode->filename = strdup(node->filename);

	      
	  if (head == NULL)
	    node->menu = newentry;
	  else
	    head->next = newentry; 
	  head = newentry;
	}
      free(buf);
    }
}



#endif
