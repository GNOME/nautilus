/* little test main() to see how we're doing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnome.h>
#include <zlib.h>

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
  {NULL}
};

int
main(int argc, char **argv)
{
	gzFile f = NULL;
	char line[250];
	poptContext ctx;
	int result;
	int foundit=0;

	char convanc[1024];
	NODE *node;

	const char **args;
	int curarg;
	
	if (!be_quiet)
		printf("info2html Version %s\n",INFO2HTML_VERSION);

	ctx = poptGetContext("gnome-info2html2", argc, argv, options, 0);

	while(poptGetNextOpt(ctx) >= 0)
	  /**/ ;

	args = poptGetArgs(ctx);
	curarg = 0;

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
	  if(!f) {
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
				
	      free(node);
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
