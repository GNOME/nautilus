/* little test main() to see how we're doing */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnome.h>
#include <zlib.h>
#ifdef HAVE_LIBBZ2
#include <bzlib.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include "data.h"
#include "html.h"
#include "parse.h"
#include "utils.h"
#include "version.h"

/* be quiet or not? */
static int be_quiet=1;

/* line_number we're on */
static int work_line_number;
static char *requested_nodename=NULL;
static struct poptOption options[] = {
  {NULL, 'a', POPT_ARG_STRING, &requested_nodename},
  {NULL, 'b', POPT_ARG_STRING, &OverrideBaseFilename},
  {NULL, 'g', POPT_ARG_NONE, &galeon_mode},
  {NULL}
};

static int
file_exists(const char *fn)
{
  struct stat sbuf;

  return (stat(fn, &sbuf) == 0);
}

int
main(int argc, char **argv)
{
	gzFile f = NULL;
	int bz = 0;
#ifdef HAVE_LIBBZ2
	BZFILE *bf=NULL;
#endif
	char line[250];
	poptContext ctx;
	int result;
	int foundit=0;
	int i, n;

	char convanc[1024];
	NODE *node;

	const char **args;
	char *fixup_args[512];
	int curarg;
	
	if (!be_quiet)
		printf("info2html Version %s\n",INFO2HTML_VERSION);

	ctx = poptGetContext("gnome-info2html2", argc, argv, options, 0); 

	while(poptGetNextOpt(ctx) >= 0)
	  /**/ ;

	args = poptGetArgs(ctx);
	curarg = 0;
	if(!args)
	  return 1;

	for(n = 0; args[n]; n++) /* */;
	if(n == 1 && !file_exists(args[0]))
	  {
	    /* As strtok destroys the string it parses and g_getenv returns a pointer to
	       the actually env var, we have to duplicate the var before parsing it. */
	    char *ctmp, *infopath = g_strdup(g_getenv("INFOPATH"));
	    char *dirs[64], *ext = NULL;
	    int ndirs;
	    char buf[PATH_MAX];

	    /* First, find the directory that the info file is in. */
	    dirs[0] = "/usr/info";
	    dirs[1] = "/usr/share/info";
	    /* We now have at least one directory to look in. This is
	     * necessary because we may not have an 'INFOPATH' set */
	    ndirs = 2;
	    if(infopath)
	      for(ndirs = 2, ctmp = strtok(infopath, ":"); ndirs < 64 && ctmp; ndirs++, ctmp = strtok(NULL, ":"))
		dirs[ndirs] = strdup(ctmp);

	    for(i = 0; i < ndirs; i++)
	      {
		ext = "";
		sprintf(buf, "%s/%s.info", dirs[i], args[0]);
		if(file_exists(buf))
		  break;
		ext = ".gz";
		sprintf(buf, "%s/%s.info.gz", dirs[i], args[0]);
		if(file_exists(buf))
		  break;
#ifdef HAVE_LIBBZ2
		ext = ".bz2";
		sprintf(buf, "%s/%s.info.bz2", dirs[i], args[0]);		
		if(file_exists(buf)) {
		  bz = 1;
		  break;
		}
#endif
	      }
	    if(i >= ndirs) {
		    printf ("<HTML><HEAD><TITLE>Document not found</TITLE>\n"
			    "</HEAD><BODY BGCOLOR=\"#FFFFFF\">The info document \"%s\" could not be found. It may have been removed from your system.\n"
			    "</BODY></HTML>\n", args[0]);
		    return 2;
	    }

	    n = i;

	    for(i = 0; ; i++)
	      {
		if(i)
		  sprintf(buf, "%s/%s.info-%d%s", dirs[n], args[0], i, ext);
		else
		  sprintf(buf, "%s/%s.info%s", dirs[n], args[0], ext);

		if(!file_exists(buf))
		  {
		    fixup_args[i] = NULL;
		    break;
		  }

		fixup_args[i] = strdup(buf);
	      }
	    args = (const char **)fixup_args;
	  }
	
	if(requested_nodename)
	  {
	    char *s, *t;
	    int  len;
	    /* strip off quotes */
	    for (s=requested_nodename; *s == '\"'; ) {
	      len = strlen( s );
	      memmove(s, s+1, len);
	    }

	    t = s + strlen(s) - 1;
	    while (*t == '\"')
	      t--;

	    *(t+1) = '\0';

	    /* convert anchor so matching works */
	    map_spaces_to_underscores(requested_nodename);
	  }

	work_line_number = 0;

	/* hack, just send to stdout for now */
	fprintf(stdout, "<BODY><HTML>\n");
	
	/* big loop to identify sections of info files */
	/* NEW PLAN - format on the fly */
	/* No need to store all nodes, etc since we let web server */
	/* handle resolving tags!                                  */
	for (;1 || !foundit || !requested_nodename;) {
	  if(bz)
	    {
#ifdef HAVE_LIBBZ2
	      if(!bf)
		{
		  if(args && args[curarg])
		    {
		      bf = bzopen(args[curarg++], "r");
		      if(!f)
			break;
		      num_files_left = args[curarg]?1:0;
		      for(work_line_number = 0, bzread(bf, line, sizeof(line)); *line != INFO_COOKIE;
			  bzread(bf, line, sizeof(line)), work_line_number++)
			/**/ ;
		    }
		  else
		    break;
		}
	      if(!bzread(bf, line, sizeof(line)))
		{
		  bzclose(bf);
		  bf = NULL;
		  continue;
		}
#else
	      g_assert_not_reached();
#endif
	    }
	  else
	    {
	      if(!f)
		{
		  if(args && args[curarg])
		    {
		      f = gzopen(args[curarg++], "r");
		      if(!f)
			break;
		      num_files_left = args[curarg]?1:0;
		      for(work_line_number = 0, gzgets(f, line, sizeof(line)); *line != INFO_COOKIE;
			  gzgets(f, line, sizeof(line)), work_line_number++)
			/**/ ;
		    }
		  else
		    break;
		}
	      if(!gzgets(f, line, sizeof(line)))
		{
		  gzclose(f);
		  f = NULL;
		  continue;
		}
	    }
		
	  work_line_number++;
		
		/* found a node definition line */
	  if (!strncmp(line, "File:", 5)) {
	    node = alloc_node();
	    result=read_node( f, line, node );
	    if ( result == READ_ERR ) {
	      fprintf(stderr, "Error reading the node "
		      "contents\n");
	      fprintf(stderr, "line was |%s|\n",line);
	      continue;
	    }
			
	    /* see if this is the requested node name */
	    strncpy(convanc, node->nodename, sizeof(convanc));
	    map_spaces_to_underscores(convanc);
	    if (requested_nodename && 
		strcmp(requested_nodename, convanc)) {
#ifdef DEBUG			    
	      fprintf(stderr, "skipping ->%s<-\n",
		      node->nodename);
#endif				

	      continue;
	    }

	    foundit = 1;
	    strcpy(work_node,node->nodename);

	    BaseFilename = node->filename;
#ifdef DEBUG
	    printf("NEW NODE\n");
	    printf("\tFile:|%s|\n\tNode:|%s|\n\tNext:|%s|\n",
		   node->filename, node->nodename,node->next);
	    printf("\tPrev:|%s|\n\tUp:|%s|\n\n", 
		   node->prev, node->up);
	    printf("-------------------------------------------"
		   "-----------\n");
#endif
	    /* now lets make some html */
	    dump_html_for_node( node );
			
	    if (node) {
	      if ( node->contents )
		free(node->contents);
				
	      g_free(node);
	      BaseFilename = NULL;
	    }
	  }
	  else
	    continue;
	}

	if (!foundit && requested_nodename) {
	  fprintf(stderr, "Requested node <b>%s</b> not found\n",
		  requested_nodename);
	  exit(1);
	}

	fprintf(stdout, "</BODY></HTML>\n");
	return 0;
}
