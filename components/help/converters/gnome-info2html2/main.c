/* little test main() to see how we're doing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"
#include "html.h"
#include "parse.h"
#include "utils.h"
#include "version.h"

int
main(argc, argv)
int argc;
char **argv;
{
  FILE *f;
  char line[250];

  int result;

  NODE *node;

if (!be_quiet)
  printf("info2html Version %s\n",INFO2HTML_VERSION);

  if (argc == 1)
    {
      f = stdin;
      strcpy(work_filename, "STDIN");
    }
  else
    {
      if ((f=fopen(argv[1], "r"))==NULL) {
	fprintf(stderr, "File %s not found.\n",argv[1]);
	exit(1);
      }
      strcpy(work_filename, argv[1]);
    }

  work_line_number = 0;


  /* scan for start of real data */
  for (;1;) {
    fgets(line,250,f);
    if (feof(f))
      {
	fprintf(stderr,"Info file had no contents\n");
	exit(1);
      }

    work_line_number++;
    if (*line == INFO_COOKIE)
      break;

  }

  /* hack, just send to stdout for now */
  fprintf(stdout, "<BODY><HTML>\n");

  /* big loop to identify sections of info files */
  /* NEW PLAN - format on the fly */
  /* No need to store all nodes, etc since we let web server */
  /* handle resolving tags!                                  */
  for (;1;) {
    fgets(line,250,f);
    if (feof(f))
      break;

    work_line_number++;

    /* found a node definition line */
    if (!strncmp(line, "File:", 5))
      {
	node = alloc_node();
	result=read_node( f, line, node );
	if ( result == READ_ERR )
	  {
	    fprintf(stderr, "Error reading the node contents\n");
	    fprintf(stderr, "line was |%s|\n",line);
	    continue;
	  }

	strcpy(work_node,node->nodename);

#ifdef DEBUG
        printf("NEW NODE\n");
	printf("\tFile:|%s|\n\tNode:|%s|\n\tNext:|%s|\n",
	       node->filename, node->nodename,node->next);
	printf("\tPrev:|%s|\n\tUp:|%s|\n\n", node->prev, node->up);
	printf("------------------------------------------------------\n");
#endif
	/* now lets make some html */
	/* first make sure the subdir for this info file exists */
	dump_html_for_node( node );

	if (node)
	  {
	    if ( node->contents )
	      free(node->contents);

	    free(node);
	  }
      }
    else
      continue;
  }
  fprintf(stdout, "</BODY></HTML>\n");
  return 0;
}
