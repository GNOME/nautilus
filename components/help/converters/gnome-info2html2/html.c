/* handles all html operations */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <glib.h>

#include "data.h"
#include "html.h"
#include "parse.h"
#include "utils.h"

#define USE_FILE_URLS

int inTable=0;

char *BaseFilename=NULL;
char *OverrideBaseFilename=NULL;

int galeon_mode=0;

/* prototypes */
char *form_info_tag_href( char *nodefile, char *nodename );
int make_Top_link( char *destdir, char *destfile );
int make_info_dir( char *destdir );
void write_node_link_html( FILE *f, char *nodefile, char *refname, char *ref );
void start_html_content( FILE *f );
void make_nav_links( FILE *f, NODE *node );
void html_error( char *s, char *p, char *q );


/* print out the url for a info file */
char *form_info_tag_href( char *nodefile, char *nodename )
{
  char tmp[1024];
  char *escaped_nodename;
  char *filename;

  escaped_nodename = escape_html_chars( nodename );
  if (!strcmp(BaseFilename, nodefile))
    {
	  if (OverrideBaseFilename)
		  filename = OverrideBaseFilename;
	  else 
		  filename = BaseFilename;
    }
  else
	  filename = nodefile;

  if (galeon_mode)
    g_snprintf (tmp, sizeof (tmp), "HREF=\"info:%s?%s\"", filename,  escaped_nodename);
  else
    g_snprintf (tmp, sizeof (tmp), "HREF=\"#%s\"", escaped_nodename);

  if (escaped_nodename)
    g_free(escaped_nodename);
  return g_strdup(tmp);
}


/* returns zero if success making link from destfile -> index.html in */
/* specified directory. If it already exists, just returns with success */
int make_Top_link( char *destdir, char *destfile )
{
  struct stat filestat;
  char *indexlink;
  
  indexlink = (char *) g_malloc( strlen(destdir) + 20);
  strcpy(indexlink, destdir);
  strcat(indexlink, "/index.html");
  
  if (lstat(indexlink, &filestat))
    {
      if (errno == ENOENT)
	{
	  if (symlink(destfile, indexlink))
	    {
	      fprintf(stderr,"Error creating link to %s\n",indexlink);
	      perror("Error was");
	      exit(1);
	    }
	}
      else
	{
	  fprintf(stderr,"Error stat'ing file %s\n",indexlink);
	  perror("Error was");
	  exit(1);
	}
    }
  else if (!S_ISLNK(filestat.st_mode))
    {
      fprintf(stderr, "file %s exists and isnt a link\n",indexlink);
      fprintf(stderr, "FIX ME!!!\n");
      g_free(indexlink);
      return -1;
    }
  return 0;
}

/* return non-zero if error with making directory */
int make_info_dir( char *destdir )
{
  struct stat filestat;

  if (stat(destdir, &filestat))
    {
      if (errno == ENOENT)
	{
	  if (mkdir(destdir, 01777))
	    {
	      fprintf(stderr,"Error creating directory %s\n",destdir);
	      perror("Error was");
	      exit(1);
	    }
	}
      else
	{
	    fprintf(stderr,"Error stat'ing directory %s\n",destdir);
	    perror("Error was");
	    exit(1);
	}
    }
  else if (!S_ISDIR(filestat.st_mode))
    {
      fprintf(stderr, "Info dir %s exists and isnt a directory!\n",destdir);
      fprintf(stderr, "FIX ME!!!\n");
      return -1;
    }
  return 0;
}

/* write a link to another document */
void write_node_link_html( FILE *f, char *nodefile, char *refname, char *ref )
{
  char *converted_nodename;
  char *href;

  if (ref) {
      if (g_strcasecmp(ref, "(dir)")) {
	  converted_nodename = g_strdup( ref );
	  map_spaces_to_underscores( converted_nodename );
	  href = form_info_tag_href(nodefile, converted_nodename);
	  fprintf(f,"<A %s>%s%s</A>\n", href, refname, ref);
	  g_free(href);
	  g_free(converted_nodename);
	} else {
	  href = form_info_tag_href("dir", "Top");
	  fprintf(f,"<A %s>%s(dir)</A>\n",href, refname);
	  g_free(href);

	}
    }
}

/* write out top of a new html file */
#if 0
void write_html_header( FILE *f, char *filename, char *nodename)
{
  fprintf(f,"<!DOCTYPE HTML PUBLIC \"-//W3C/DTD HTML 3.2//EN\">\n");
  fprintf(f,"<HTML>\n");
  fprintf(f,"<HEAD>\n");
  fprintf(f,"<TITLE>Info Node: (%s)%s</TITLE>\n",filename,nodename);
  fprintf(f,"<META NAME=\"GENERATOR\" CONTENT=\"info2html\">\n");
  fprintf(f,"</HEAD>\n");
  fprintf(f,"<!-- conversion of file \"%s\", node \"%s\" -->\n",work_filename, work_node);
}
#endif

/* start of everything after html header */
void start_html_content( FILE *f )
{
  fprintf(f,"<BODY>\n");
}

/* we want to put links to next, prev, and up nodes */
void make_nav_links( FILE *f, NODE *node )
{
#if 0
  fprintf(f,"<PRE>\n");
  write_node_link_html( f, node->filename, "Next:", node->next );
  write_node_link_html( f, node->filename, "Prev:", node->prev );
  write_node_link_html( f, node->filename, "Up:", node->up );
  fprintf(f,"</PRE>\n");
#else
  fprintf(f,"<TABLE border=2 cellspacing=1 cellpadding=4 width=100%%>\n");
  fprintf(f,"<TR bgcolor=\"#eeeee0\">\n");
  fprintf(f,"\t<TH align=center width=33%%>\n\t");
  write_node_link_html( f, node->filename, "Next:", node->next );
  fprintf(f,"\t</TH>\n");
  fprintf(f,"\t<TH align=center width=33%%>\n\t");
  write_node_link_html( f, node->filename, "Prev:", node->prev );
  fprintf(f,"\t</TH>\n");
  fprintf(f,"\t<TH align=center width=34%%>\n\t");
  write_node_link_html( f, node->filename, "Up:", node->up );
  fprintf(f,"\t</TH>\n");
  fprintf(f,"</TR>\n</TABLE>\n");
#endif

}

/* s is error message */
/* p is start of offending line */
/* q is end of offending line */
void html_error( char *s, char *p, char *q )
{
  fprintf(stderr, "%s:%s\n",work_filename, work_node);
  fprintf(stderr, "\t%s\n",s);
  fprintf(stderr, "\tOffending line is:\n\t|");
  fwrite(p, 1, q-p, stderr);
  fprintf(stderr, "|\n");
}

/********************************************************************
 * here is what we expect in contents of a node: 
 * 
 *  headers:   These are identified as a line of text
 *             followed by a row of '---' or '###' normally.
 *             These get mapped to <H2> </H2> for now.
 *
 *  body text: Format this between <PRE> </PRE> statements.
 *             Catch any *Note and *note and make into
 *             links to other documents. Also try to catch
 *             URLs as well.
 *
 *  menus:     Starts with a '* Menu' line. Goes until the
 *             end of the node, or until the next line which
 *             starts with something other than a '* ' or '\n'.
 *
 *  end of node: The INFO_FF and INFO_COOKIE mark the end of a node.
 *               Hitting EOF also marks the end of a node.
 ********************************************************************/

void dump_html_for_node( NODE *node )
{
/*  char *destdir; */
/*  char *destfile; */
  char *escaped_nodename;
/*  char *converted_nodename; */
  char *contents_start, *contents_end;
  char *header_name;
  char *p, *q, *r, *skippnt;
  char *end_menu_entry;


  int menu_open, body_open;

  int seen_menu;
    
  int prev_was_blank, next_is_blank, current_is_blank;

  int seen_first_header;

  int last_output_was_header;

  int nskip;

  int we_are_in_dir_node;

  int i;

  FILE *f;

/* msf - used to write each node to a separate file - now we're going */
/*       to just output HTML to stdout.                               */
/*       Each node will just be concantentated to previous            */
#if 0
  destdir = (char *) g_malloc ( strlen(node->filename) + 
			      strlen(HTML_ROOT) +
			      strlen(node->filename) + 2);
  strcpy(destdir, HTML_ROOT);
  strcat(destdir, "/");
  strcat(destdir, node->filename);
  strcat(destdir, "/");

  /* check that the dir for info file exists */
  make_info_dir( destdir );

  /* ok, we made the dir, lets go */
  destfile = (char *) g_malloc( strlen(destdir) + strlen(node->nodename) + 10);
  strcpy(destfile, destdir);
  converted_nodename = g_strdup( node->nodename );
  map_spaces_to_underscores( converted_nodename );
  strcat(destfile, converted_nodename);
  strcat(destfile, ".html");
  g_free(converted_nodename);

  if (!(f=fopen(destfile, "w")))
    {
      fprintf(f,"Couldnt create node html file %s\n",destfile);
      perror("Error was");
      exit(1);
    }
#endif

  /* hack - just dump to stdout for now */
  f = stdout;

  /* see if this is THE dir node */
  we_are_in_dir_node = !strcmp("Top", node->nodename) && !strcmp("dir", node->filename);

#if 0
  /* try and make a link between 'index.html' and 'Top.html' */
  if (!strcmp("Top", node->nodename))
      make_Top_link( destdir, destfile );
#endif

#if 0
  /* do the html header first */
  write_html_header( f, node->filename, node->nodename );
#endif

#if 0
  /* now for the body */
  start_html_content( f );
#endif

  /* make an anchor */
  escaped_nodename = escape_html_chars( node->nodename );
  map_spaces_to_underscores( escaped_nodename );
  fprintf(f, "<A name=\"%s\"></A>\n",escaped_nodename);
  g_free(escaped_nodename);

  /* links to other immediate nodes */
  make_nav_links( f, node );

  /* setup pointers to textual content of current node */
  contents_start = node->contents;
  contents_end   = node->contents+node->contlen;

  /* scan through all of contents and generate html on the fly */
  /* p points at start of current line */
  /* q points at the end of current line (at '\n' actually) */
  /* r points at the start of next line */
  /* we do this to catch headers */
  /* scan for a header at the top of the contents */
  /* if we see a '\n***'3 '*' in a row i */
  /* then take previous line as a header */
  header_name = NULL;
  p = contents_start = node->contents;
  q = memchr(p, '\n', contents_end - p);
  r=q+1;

  /* we have several states we could be in */
  next_is_blank = 0;
  prev_was_blank = 0;
  current_is_blank = 0;
  seen_first_header = 0;

  seen_menu = 0;
  menu_open = 0;
  body_open = 0;

  last_output_was_header = 0;
  for (; q && r <= contents_end; )
    {
      nskip = 1;
      skippnt = NULL;
      next_is_blank = (*r == '\n');
      current_is_blank = (*p == '\n');

      /* test some easy things first */
      if (!strncmp(p, MENU_START, strlen(MENU_START)))
	{
	  if (we_are_in_dir_node && !seen_menu)
	    {
	      if (body_open)
		{
		  close_body_text_html(f);
		  body_open = 0;
		}

	      fprintf(f,"<H1> Main Info File Directory </H1>\n");

	      open_body_text_html(f);
	      body_open = 1;

	      fprintf(f,"This is the main directory of available info files.\n");
	    }

	  if (body_open)
	    {
	      close_body_text_html(f);
	      body_open = 0;
	    }
	  else if (seen_menu)
	    html_error("Warning:saw new menu start and already in menu!", p, q);

	  if (menu_open)
	    close_menu_html( f );

	  if (last_output_was_header)
	    open_menu_html( f, "" );
	  else
	    open_menu_html( f, "Contents" );

	  seen_menu = 1;
	  menu_open = 1;
	  last_output_was_header = 0;
	}
      else if (we_are_in_dir_node && !seen_menu)
	{
	  /* do nothing */
	}
      else if (seen_menu)
	{
	  /* if current line is blank ignore it */
	  if (current_is_blank)
	    {
	      /* do nothing */
	    }
	  /* first see if its a menu line */
	  else if (!strncmp(p, MENU_ENTRY, strlen(MENU_ENTRY)))
	    {
	      if (!seen_menu)
		html_error("Have seen menu start and hit a menu line!", p, q);
	      else
		{
		  if (body_open)
		    {
		      if (menu_open)
			html_error("Hit a menu line, and body and menu are opened!", p, q);
		      close_body_text_html( f );
		      body_open = 0;
		      open_menu_html( f, "" );
		      menu_open = 1;
		    }
		  if (!menu_open)
		    {
		      open_menu_html( f, "" );
		      menu_open = 1;
		    }
		  write_menu_entry_html( f, p, node->filename, &end_menu_entry );
		  if (end_menu_entry != NULL)
		    skippnt = end_menu_entry;
		  last_output_was_header = 0;
		}
	    }
	  /* maybe its a header line */
	  /* man this is getting ridiculous, its like teaching a child */
	  /* to read! */
	  else if (is_a_hdr_line(r) || 
		   (*p != '*' && *r == '*' && *(r+1) == ' ') ||
		   (*p != '*' && seen_menu && (*p != ' ' && *(p+1) != ' ') &&
		    !current_is_blank && prev_was_blank && next_is_blank))
	    {
	      header_name = (char *) g_malloc( q-p+2 );
	      memcpy(header_name, p, q-p);
	      *(header_name + (q - p) ) = '\000';

	      /* if we were writing a particular component, close it */
	      if (menu_open)
		{
		  close_menu_html( f );
		  menu_open = 0;
		}

	      if (body_open)
		{
		  close_body_text_html( f );
		  body_open = 0;
		}

	      if (seen_first_header)
		write_header_html( f, header_name, HEADER_SIZE_2 );
	      else
		{
		  seen_first_header = 1;
		  write_header_html( f, header_name, HEADER_SIZE_1 );
		}

	      g_free(header_name);

	      /* jump ahead another line */
	      if (!(*r == '*' && *(r+1) == ' ') && !next_is_blank)
		nskip++;

	      last_output_was_header = 1;
	    }
	  /* well, has to be body text then */
	  else
	    {
	      if (menu_open)
		{
		  close_menu_html( f );
		  menu_open = 0;

		  write_html_horiz_rule ( f );
		}
	      
	      if (!body_open)
		{
		  open_body_text_html( f );
		  body_open = 1;
		}

	      if (*p != '\n' && !last_output_was_header)
		{
		  skippnt=write_body_text_html( f, p, q, node->filename );
		  last_output_was_header = 0;
		}
	    }
	}
      /* otherwise, no menu seen so things are easier */
      else
	{
	  if (is_a_hdr_line(r))
	    {
	      header_name = (char *) g_malloc( q-p+2 );
	      memcpy(header_name, p, q-p);
	      *(header_name + (q - p) ) = '\000';
	      
	      /* if we were writing a particular component, close it */
	      if (body_open)
		{
		  close_body_text_html( f );
		  body_open = 0;
		}

	      if (seen_first_header)
		write_header_html( f, header_name, HEADER_SIZE_2 );
	      else
		{
		  seen_first_header = 1;
		  write_header_html( f, header_name, HEADER_SIZE_1 );
		}

	      g_free(header_name);

	      /* jump ahead another line */
	      if (!(*r == '*' && *(r+1) == ' ') && !next_is_blank)
		nskip++;

	      last_output_was_header = 1;
	    }
	  /* well, has to be body text then */
	  else
	    {
	      if (!body_open)
		{
		  open_body_text_html( f );
		  body_open = 1;
		}

	      if (!(*p == '\n' && last_output_was_header))
		{
		  skippnt=write_body_text_html( f, p, q, node->filename );
		  last_output_was_header = 0;
		}
	    }
	}

      /* end of cases, move to next line in contents */
      prev_was_blank = (*p == '\n');
      if (skippnt)
	{
	  p = skippnt;
	  q = memchr(p, '\n', contents_end - p);
	  r = q+1;
	  skippnt = NULL;
	}
      else
	for (i=0; i< nskip; i++)
	  {
	    p = r;
	    q = memchr(p, '\n', contents_end - p);
	    r = q+1;
	  }
    }

  /* thats all folks */
  if (menu_open)
    close_menu_html( f );
  else if (body_open)
    close_body_text_html( f );

  /* put nav links at the bottom */
  make_nav_links(f, node);
#if 0
  fprintf(f,"</BODY>\n</HTML>\n");
#endif

  /* clean up */
#if 0
  g_free(destdir);
  g_free(destfile);
#endif
}


void write_header_html( FILE *f, char *p, char *hdr )
{
  fprintf(f,"<%s> %s </%s>\n",hdr,p,hdr);
}


void open_body_text_html( FILE *f )
{
  fprintf(f, "<PRE>\n");
}

void close_body_text_html( FILE *f )
{
  fprintf(f, "</PRE>\n");
}

/* we have to handle '*note' and '*Note' links in body text */
/*   p is ptr to start of current line */
/*   q is ptr to '\n' at end of current line */
char *write_body_text_html( FILE *f, char *p, char *q, char *nodefile )
{
  int curlen;
  int ref_exists;
  char *tmp;
  char *ptr;
  char *match1;
  char *note_ptr;
  char *converted_nodename;
  char *escaped_refname;
  char *escaped_refnode;
  char *escaped_seg;
  char *refname, *reffile, *refnode, *end;
  char *href;

  curlen = q - p;
  tmp = (char *) g_malloc( curlen + 1 );
  memcpy( tmp, p, curlen );
  *(tmp+curlen) = '\000';

  /* see if there is a reference in current line */
  /* and make sure this isnt a '*Note*' instead ! */
  ref_exists = 0;
  if ((note_ptr=strstr(tmp, "*Note")) || (note_ptr=strstr(tmp, "*note")))
    if (*(note_ptr+6) != '*')
      ref_exists = 1;

  if (ref_exists)
    {
      /* find the start of the link */
      note_ptr = (note_ptr - tmp) + p;
      match1 = note_ptr + 4;

      /* not needed any more */
      g_free(tmp);

      for (; 1; )
	if (*(match1+1) == ' ' || *(match1+1) == '\n')
	  match1++;
	else
	  break;
      
      /* find end of the link */
      if (parse_note_ref( match1, &refname, &reffile, &refnode, &end, 1))
	{
	  html_error( "Corrupt *Note link found!", p, q );
	  return NULL;
	}

      /* now we assume that parse_note_ref left control chars in ref* */
      /* if both null, we had a '::' and have to set both */
      if (reffile == NULL && refnode == NULL)
	{
	  reffile = g_strdup(nodefile);
	  refnode = g_strdup(refname);
	}
      /* otherwise we had (file)., and we set node to 'Top' */
      else if (refnode == NULL)
	refnode = g_strdup("Top");
      /* otherwise we had :nodename., and we set node to 'Top' */
      else if (reffile == NULL)
	reffile = g_strdup(nodefile);
      
/* Here we need to escape everything up to Note.
 * One caveat: The "*Note*" itself isn't escaped.  Currently we know this is
 * okay ("*Note" has no characters needing escapes.) but....
 */
      curlen = note_ptr - p;
      tmp = (char *) g_malloc (curlen + 1);
      memcpy (tmp, p, curlen);
      *(tmp + curlen) = '\000';
      escaped_seg = escape_html_chars (tmp);
      g_free (tmp);
      
      /* write out stuff up to Note */
      fprintf(f, "%s", escaped_seg);
      fprintf(f, "<STRONG>");
      fwrite(note_ptr, 1, match1 - note_ptr, f);
      fprintf(f, " </STRONG>");

      /* we need a nice nodename -> filename translation */
      /* so we convert newlines to spaces */
      converted_nodename = g_strdup( refnode );
      convert_newlines_to_spaces( converted_nodename );

      /* we don't want two spaces in a row */
      strip_dupspaces( converted_nodename );
      map_spaces_to_underscores( converted_nodename );

      /* escape HTML chars */
      escaped_refname = escape_html_chars( refname );
      escaped_refnode = escape_html_chars( refnode );

      /* now output the link to html doc */
#if 0
      fprintf(f,"<A HREF=\"../%s/%s.html\">", reffile, converted_nodename);
#endif
      href = form_info_tag_href(reffile, converted_nodename);
      fprintf(f,"<A %s>", href);
      for (ptr=escaped_refname; *ptr; ptr++)
	if (*ptr == '\n')
	  {
	    fprintf(f,"</A>\n");
	    fprintf(f,"<A %s>", href);
	  }
	else
	  fprintf(f,"%c", *ptr);
	
      if (strcmp(refname, refnode))
	{
	  fprintf(f,": ");
	  for (ptr=escaped_refnode; *ptr; ptr++)
	    if (*ptr == '\n')
	      {
		fprintf(f,"</A>\n");
		fprintf(f,"<A %s>", href);
	      }
	    else
	      fprintf(f,"%c", *ptr);

	  fprintf(f,"</A>");
	  if (end > q && !(strchr(refnode, '\n')))
	    fprintf(f,"\n");
	}
      else
	fprintf(f,"</A>");

      if (href)
	g_free(href);
      if (escaped_refnode)
	g_free(escaped_refnode);
      if (escaped_refname)
	g_free(escaped_refname);
      if (converted_nodename)
	g_free(converted_nodename);

      g_free(escaped_seg);
      g_free(refname);
      g_free(reffile);
      g_free(refnode);

      /* write out stuff at end */
      if (end < q)
	{

/* Escape up to the end of line. */
      curlen = q - (end+1);
      tmp = (char *) g_malloc (curlen + 1);
      memcpy (tmp, end+1, curlen);
      *(tmp+curlen) = '\000';
      escaped_seg = escape_html_chars (tmp);
      g_free (tmp);
      
      fprintf (f, "%s", escaped_seg);
      fprintf (f, "\n");
      g_free (escaped_seg);
	  return NULL;
	}
      else
	  return end+1;
    }
  else
    {

/* Escape the whole thing. */
      escaped_seg = escape_html_chars (tmp);
      fprintf (f, "%s", escaped_seg);
      fprintf (f, "\n");
      /* not needed any more */
      g_free(tmp);
      g_free (escaped_seg);
      return NULL;
    }
}

void open_menu_html( FILE *f, char *p )
{
  if (*p != '\000')
    fprintf(f, "<H2>%s</H2>\n",p);
  /*  fprintf(f, "<UL>\n"); */
#if 0
    fprintf(f, "<dl>\n"); 
#else
    if (inTable)
	    fprintf(stderr, "In a table and starting new one!\n");
    inTable = 1;
    fprintf(f, "<table width=100%%><tr><td>&nbsp;</td></tr>\n");
#endif
}

void close_menu_html( FILE *f )
{
  /* fprintf(f, "</UL>\n"); */
#if 0
    fprintf(f, "</dl>\n");
#else
    if (!inTable)
	    fprintf(stderr, "Not in a table and closing one!\n");
    inTable = 0;
    fprintf(f, "</table>\n");
#endif
}

/* writes menu entry contained in string p */
/* nodename and nodefile apply to the node which menu entry is in */
void write_menu_entry_html( FILE *f, char *p, char *nodefile, char **menu_end )
{
  char *refname;
  char *reffile;
  char *refnode;
  char *end;
  char *realend;
  char *converted_nodename;
  char *escaped_refnode;
  char *escaped_refname;
  char *href;

  int i, done;

  /* skip over the '* ' at the start of the line */
  if (parse_menu_line( p+2, &refname, &reffile, &refnode, &end, 0 ))
    {
      html_error("Error parsing menu", p, memchr(p, '\n', 80));
      return;
    }

  /* if both null, we had a '::' and have to set both */
  if (reffile == NULL && refnode == NULL)
    {
      reffile = g_strdup(nodefile);
      refnode = g_strdup(refname);
    }
  /* otherwise we had (file)., and we set node to 'Top' */
  else if (refnode == NULL)
    refnode = g_strdup("Top");
  else if (reffile == NULL)
    reffile = g_strdup(nodefile);

  /* now find the end of the right hand text for this menu line */
  /* it can continue for several lines                          */
  done = 0;
  for (realend = end+1; !done; realend++)
    {
      if (*realend == '\n')
	{ 
	  if (*(realend+1) == '\n')
	    {
	      done = 1;
	      continue;
	    }

	  for (i=1; i<4; i++)
	    if (!isspace((guchar)*(realend+i)) && *(realend+i) != '\n')
	      {
		done = 1;
		break;
	      }
	}
    }
  *menu_end = realend;


  converted_nodename = g_strdup( refnode );
  map_spaces_to_underscores( converted_nodename );

  escaped_refnode = escape_html_chars( refnode );
  escaped_refname = escape_html_chars( refname );

#if 0
  fprintf(f,"<dt><A HREF=\"../%s/%s.html\">%s</A>\n",
	  reffile, converted_nodename, escaped_refname);
#endif

  href = form_info_tag_href( reffile, converted_nodename );
#if 0
  fprintf(f,"<dt><A %s>%s</A>\n", href, escaped_refname );
  fprintf(f,"<dd>");
  if (*end == '.' && *(end+1) == '\n')
    fprintf(f,"%s.\n",escaped_refnode);
  else
    fwrite(end+1, 1, *menu_end - end - 1, f);
#else
  fprintf(f,"<tr>\n\t<td width=30%%>\n\t\t<A %s>%s</A>\n\t</td>\n"
	  "\t<td width=70%%>\n\t\t", 
	  href, escaped_refname );
  if (*end == '.' && *(end+1) == '\n')
    fprintf(f,"%s.\n",escaped_refnode);
  else
    fwrite(end+1, 1, *menu_end - end - 1, f);
  fprintf(f,"\n\t</td>\n</tr>\n");
#endif

  if (href)
    g_free(href);
  if (escaped_refname)
    g_free(escaped_refname);
  if (escaped_refnode)
    g_free(escaped_refnode);
  if (converted_nodename)
    g_free(converted_nodename);
  g_free(refname);
  g_free(reffile);
  g_free(refnode);
}

void write_html_horiz_rule( FILE *f )
{
  fprintf(f, "<HR>\n");
}
