/*
** This program was written by Richard Verhoeven (NL:5482ZX35)
** at the Eindhoven University of Technology. Email: rcb5@win.tue.nl
**
** Permission is granted to distribute, modify and use this program as long
** as this comment is not removed or changed.
*/

/* 
 * gnome-man2html
 * This version modified for GNOME - February 1998
 * Michael Fulbright <msf@redhat.com>
 */


/* 
 * man2html-linux-1.0/1.1
 * This version modified for Redhat/Caldera linux - March 1996.
 * Michael Hamilton <michael@actrix.gen.nz>.
 *
 * man2html-linux-1.2
 * Added support for BSD mandoc pages - I didn't have any documentation
 * on the mandoc macros, so I may have missed some.
 * Michael Hamilton <michael@actrix.gen.nz>.
 *
 * vh-man2html-1.3
 * Renamed to avoid confusion (V for Verhoeven, H for Hamilton).
 *
 * vh-man2html-1.4
 * Now uses /etc/man.config
 * Added support for compressed pages.
 * Added "length-safe" string operations for client input parameters.
 * More secure, -M secured, and client input string lengths checked.
 *
 */

/*
** If you want to use this program for your WWW server, adjust the line
** which defines the CGIBASE or compile it with the -DCGIBASE='"..."' option.
**
** You have to adjust the built-in manpath to your local system. Note that
** every directory should start and end with the '/' and that the first
** directory should be "/" to allow a full path as an argument.
**
** The program first check if PATH_INFO contains some information.
** If it does (t.i. man2html/some/thing is used), the program will look
** for a manpage called PATH_INFO in the manpath.
**
** Otherwise the manpath is searched for the specified command line argument,
** where the following options can be used:
**
** name      name of manpage (csh, printf, xv, troff)
** section   the section (1 2 3 4 5 6 7 8 9 n l 1v ...)
** -M path   an extra directory to look for manpages (replaces "/")
**
** If man2html finds multiple manpages that satisfy the options, an index
** is displayed and the user can make a choice. If only one page is
** found, that page will be displayed.
**
** man2html will add links to the converted manpages. The function add_links
** is used for that. At the moment it will add links as follows, where   
**     indicates what should match to start with:
** ^^^
** Recognition           Item            Link
** ----------------------------------------------------------
** name(*)               Manpage         ../man?/name.*
**     ^
** name@hostname         Email address   mailto:name@hostname
**     ^
** method://string       URL             method://string
**       ^^^
** www.host.name         WWW server      http://www.host.name
** ^^^^
** ftp.host.name         FTP server      ftp://ftp.host.name
** ^^^^
** <file.h>              Include file    file:/usr/include/file.h
**      ^^^
**
** Since man2html does not check if manpages, hosts or email addresses exist,
** some links might not work. For manpages, some extra checks are performed
** to make sure not every () pair creates a link. Also out of date pages
** might point to incorrect places.
**
** The program will not allow users to get system specific files, such as
** /etc/passwd. It will check that "man" is part of the specified file and
** that  "/../" isn't. Even if someone manages to get such file, man2html will
** handle it like a manpage and will usually not produce any output (or crash).
**
** If you find any bugs when normal manpages are converted, please report
** them to me (rcb5@win.tue.nl) after you have checked that man(1) can handle
** the manpage correct.
**
** Known bugs and missing features:
**
**  * Equations are not converted at all.
**  * Tables are converted but some features are not possible in html.
**  * The tabbing environment is converted by counting characters and adding
**    spaces. This might go wrong (outside <PRE>)
**  * Some pages look beter if man2html works in troff mode, especially pages
**    with tables. You can deside at compile time which made you want to use.
**
**    -DNROFF=0     troff mode
**    -DNROFF=1     nroff mode   (default)
**
**    if you install both modes, you should compile with the correct CGIBASE.
**  * Some manpages rely on the fact that troff/nroff is used to convert
**    them and use features which are not descripted in the man manpages.
**    (definitions, calculations, conditionals, requests). I can't guarantee
**    that all these features work on all manpages. (I didn't have the
**    time to look through all the available manpages.)
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define NULL_TERMINATED(n) ((n) + 1)

#define HUGE_STR_MAX  10000
#define LARGE_STR_MAX 2000
#define MED_STR_MAX   500
#define SMALL_STR_MAX 100
#define TINY_STR_MAX  10

#define MAX_MAN_PATHS 100	/* Max number of directories */
#define MAX_ZCATS     10	/* Max number of zcat style programs */
#define MAX_WORDLIST  100


#ifndef TOPLEVELDOC
#define TOPLEVELDOC "/home/httpd/html/man.html"
#endif

#ifndef CGIBASE
#define CGIBASE "http://localhost/cgi-bin/grab-man-page"
#endif

#ifndef NROFF
#define NROFF 1
#endif

static char location_base[NULL_TERMINATED(MED_STR_MAX)] = "";

char *signature = "<HR>\n"
"This document was created by\n"
"<A HREF=\""CGIBASE"\">man2html</A>,\n"
"using the manual pages.<BR>\n"
"Time: %s\n";

/* timeformat for signature */
#define TIMEFORMAT "%T GMT, %B %d, %Y"

/* BSD mandoc Bl/El lists to HTML list types */
#define BL_DESC_LIST   1
#define BL_BULLET_LIST 2
#define BL_ENUM_LIST   4

/* BSD mandoc Bd/Ed example(?) blocks */
#define BD_LITERAL  1
#define BD_INDENT   2

/* Don't bother with free(), it will just slow us down, the ultimate
   free will be exit() anyway.  Hack to fix a malloc bug that I couldn't
   track down (michael@actrix.gen.nz).
   */
#define free(x)

static char *manpath[MAX_MAN_PATHS + 1] = { 
                    "/",	/* for searches when path is absolute
				 * But will be constrained by UNSECURE_MANPATH
				 * setting.
				 */
		    "/usr/X11/man/",
		    "/usr/X11R6/man/",
		    "/usr/man/",
		    "/usr/local/man/",
		    "/usr/exp/man/",
		    "/usr/openwin/man/",
		    "/usr/tex/man/",
		    "/usr/www/man/",
		    "/usr/lang/man/",
		    "/usr/gnu/man/",
		    "/usr/motif/man/",
		    "/usr/titools/man/",
		    "/usr/sunpc/man/",
		    "/usr/ncd/man/",
		    "/usr/newsprint/man/",
		    NULL };

#ifndef DISABLE_ZCATS
static char *zcats[MAX_ZCATS + 1] = {
                    ".Z\0/bin/zcat",
                    ".gz\0/bin/gzip -d -c",
                    NULL };
#endif

static char *sections = "123456789nl";


static char *strgrow(char *old, int len) 
{
  char *new = realloc(old, (strlen(old) + len + 1) * sizeof(char));
  if (!new) {
    fprintf(stderr, "man2html: out of memory");
    exit(EXIT_FAILURE);
  }
  return new;  
}

static char *stralloc(int len)
{					
  /* allocate enough for len + NULL */
  char *new = malloc((len + 1) * sizeof(char));
  if (!new) {
    fprintf(stderr, "man2html: out of memory");
    exit(EXIT_FAILURE);
  }
  return new;
}

static char *strduplicate(char *from)
{				
/* Some systems don't have strdup so lets use our own - which can also
 * check for out of memory. 
 */
  char *new = stralloc(strlen(from));
  strcpy(new, from);
  return new;
}

static char *strmaxcpy(char *to, char *from, int n)
{				/* Assumes space for n plus a null */
  int len = strlen(from);
  strncpy(to, from, n);
  to[(len <= n) ? len : n] = '\0';
  return to;
}

static char *strmaxcat(char *to, char *from, int n)
{				/* Assumes space for n plus a null */
  int to_len = strlen(to);
  if (to_len < n) {
    int from_len = strlen(from);
    int cp = (to_len + from_len <= n) ? from_len : n - to_len;
    strncpy(to + to_len, from, cp);
    to[to_len + cp] = '\0';
  }
  return to;
}

static char *strlimitcpy(char *to, char *from, int n, int limit)
{                               /* Assumes space for limit plus a null */
  int len = n > limit ? limit : n;
  strmaxcpy(to, from, len);
  to[len] = '\0';
  return to;
}

static char *escape_input(char *str)
/* takes string and escapes all metacharacters.  should be used before
   including string in system() or similar call. */
{
  int i,j = 0;
  static char new[NULL_TERMINATED(MED_STR_MAX)];

  if (strlen(str) * 2 + 1 > MED_STR_MAX) {
    fprintf(stderr, 
	    "man2html: escape_input - str too long:\n%-80s...\n",
	    str);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < strlen(str); i++) {
    if (!( ((str[i] >= 'A') && (str[i] <= 'Z')) ||
           ((str[i] >= 'a') && (str[i] <= 'z')) ||
           ((str[i] >= '0') && (str[i] <= '9')) )) {
      new[j] = '\\';
      j++;
    }
    new[j] = str[i];
    j++;
  }
  new[j] = '\0';
  return new;
}                      

static FILE *pipe_open(char *cmd, char *filename)
{
  FILE *pipe_stream = NULL;
  char cmdline[NULL_TERMINATED(LARGE_STR_MAX)];

  if (strlen(filename) * 2 + 1 + strlen(cmd) + 1 > LARGE_STR_MAX) {
    fprintf(stderr, "man2html: pipe_open - str too long:\n%-80s...\n", 
	    filename);
    return NULL;
  }
  strmaxcpy(cmdline, cmd, LARGE_STR_MAX);
  strmaxcat(cmdline, " ", LARGE_STR_MAX);
  strmaxcat(cmdline, escape_input(filename), LARGE_STR_MAX);  

  pipe_stream = popen(cmdline, "r");	/* Potentially dangerious - CGI access to sh */
  /* fprintf(stderr, "openned %s %s\n", cmdline, filename);  */
  return pipe_stream;
}

static int pipe_close(FILE *f)
{
  return pclose(f);
}


static void usage(void)
{
    char buffer[NULL_TERMINATED(LARGE_STR_MAX)];
/* only useful if TOPLEVELDOC actually points at a document */
#if 0
    FILE *toplevel = fopen(TOPLEVELDOC, "r");
#else
    FILE *toplevel = NULL;
#endif
    
    if (!toplevel && errno == ENOENT) {
      fputs("Content-type: text/html\n\n"
	     "<HTML><HEAD>\n"
	     "<TITLE>Unified Docs Manual Pages</TITLE>\n"
	     "</HEAD><BODY>\n"
	     "<H1>Unified Docs Manual Pages</H1>\n"
	     "This is a HyperText interface to the UNIX manpages.\n"
	     "If you see this you have gotten into no-mans-land.\n"
	     "Please let msf@redhat.com know how you got here.\n"
#if 0
	     "<HTML><HEAD>\n"
	     "<TITLE>Manual Pages</TITLE>\n"
	     "</HEAD><BODY>\n"
	     "<H1>Manual Pages</H1>\n"
	     "This is a HyperText interface to the UNIX manpages.\n"
	     "You can enter a program name, the section, an extra\n"
	     "directory (using -M) or a full name. For example\n"
	     "<UL><LI><TT>elm</TT>\n"
	     "<LI><TT>elm 1</TT>\n"
	     "<LI><TT>-M /usr/local/man elm</TT>\n"
	     "<LI><TT>/local/gcc/man/man1/gperf.1</TT>\n"
	     "</UL>\n"
	     "<ISINDEX>\n"
	     "<P>\n"
	     "This man2html converter was written by \n"
	     "<A HREF=\"http://wsinwp01.win.tue.nl:1234/index.html\">"
	     "Richard Verhoeven</A>\n"
#endif
	     "</BODY></HTML>\n", stdout);
      exit(0);      
    }

/* msf - rest is useless unless we point TOPLEVELDOC at something */
#if 0
    if (!toplevel) {
      fprintf(stderr, "man2html: error openning %s: %s\n", TOPLEVELDOC, strerror(errno));
      exit(EXIT_FAILURE);
    }
    while (fgets(buffer, LARGE_STR_MAX, toplevel)) {
        fputs(buffer,stdout);
    }
    if (!feof(toplevel)) {
        fprintf(stderr, "man2html: error reading %s: %s\n", TOPLEVELDOC, strerror(errno));
	exit(EXIT_FAILURE);
    }
    fclose(toplevel);
#endif
    exit(0);
}



/* below this you should not change anything unless you know a lot
** about this program or about troff.
*/


typedef struct STRDEF STRDEF;
struct STRDEF {
    int nr,slen;
    char *st;
    STRDEF *next;
};

typedef struct INTDEF INTDEF;
struct INTDEF {
    int nr;
    int val;
    int incr;
    INTDEF *next;
};

static char NEWLINE[2]="\n";
static char idxlabel[6] = "ixAAA";

#define INDEXFILE "/tmp/manindex.list"

static char *fname;
static FILE *idxfile;

static STRDEF *chardef, *strdef, *defdef;
static INTDEF *intdef;

#define V(A,B) ((A)*256+(B))

static INTDEF standardint[] = {
    { V('n',' '), NROFF,0, NULL },
    { V('t',' '), 1-NROFF,0, NULL },
    { V('o',' '), 1,0, NULL },
    { V('e',' '), 0,0, NULL },
    { V('.','l'), 70,0,NULL },
    { V('.','$'), 0,0, NULL },
    { V('.','A'), NROFF,0, NULL },
    { V('.','T'), 1-NROFF,0, NULL },
    { V('.','V'), 1,0, NULL }, /* the me package tests for this */
    { 0, 0, 0, NULL } };

static STRDEF standardstring[] = {
    { V('R',' '), 1, "&#174;", NULL },
    { V('l','q'), 2, "``", NULL },
    { V('r','q'), 2, "''", NULL },
    { 0, 0, NULL, NULL}
};


static STRDEF standardchar[] = {
    { V('*','*'), 1, "*", NULL  },
    { V('*','A'), 1, "A", NULL  },
    { V('*','B'), 1, "B", NULL  },
    { V('*','C'), 2, "Xi", NULL  },
    { V('*','D'), 5, "Delta", NULL  },
    { V('*','E'), 1, "E", NULL  },
    { V('*','F'), 3, "Phi", NULL  },
    { V('*','G'), 5, "Gamma", NULL  },
    { V('*','H'), 5, "Theta", NULL  },
    { V('*','I'), 1, "I", NULL  },
    { V('*','K'), 1, "K", NULL  },
    { V('*','L'), 6, "Lambda", NULL  },
    { V('*','M'), 1, "M", NULL  },
    { V('*','N'), 1, "N", NULL  },
    { V('*','O'), 1, "O", NULL  },
    { V('*','P'), 2, "Pi", NULL  },
    { V('*','Q'), 3, "Psi", NULL  },
    { V('*','R'), 1, "P", NULL  },
    { V('*','S'), 5, "Sigma", NULL  },
    { V('*','T'), 1, "T", NULL  },
    { V('*','U'), 1, "Y", NULL  },
    { V('*','W'), 5, "Omega", NULL  },
    { V('*','X'), 1, "X", NULL  },
    { V('*','Y'), 1, "H", NULL  },
    { V('*','Z'), 1, "Z", NULL  },
    { V('*','a'), 5, "alpha", NULL },
    { V('*','b'), 4, "beta", NULL },
    { V('*','c'), 2, "xi", NULL },
    { V('*','d'), 5, "delta", NULL },
    { V('*','e'), 7, "epsilon", NULL },
    { V('*','f'), 3, "phi", NULL },
    { V('*','g'), 5, "gamma", NULL },
    { V('*','h'), 5, "theta", NULL },
    { V('*','i'), 4, "iota", NULL },
    { V('*','k'), 5, "kappa", NULL },
    { V('*','l'), 6, "lambda", NULL },
    { V('*','m'), 1, "&#181;", NULL  },
    { V('*','n'), 2, "nu", NULL },
    { V('*','o'), 1, "o", NULL },
    { V('*','p'), 2, "pi", NULL },
    { V('*','q'), 3, "psi", NULL },
    { V('*','r'), 3, "rho", NULL },
    { V('*','s'), 5, "sigma", NULL },
    { V('*','t'), 3, "tau", NULL },
    { V('*','u'), 7, "upsilon", NULL },
    { V('*','w'), 5, "omega", NULL },
    { V('*','x'), 3, "chi", NULL },
    { V('*','y'), 3, "eta", NULL },
    { V('*','z'), 4, "zeta", NULL },
    { V('t','s'), 5, "sigma", NULL },
    { V('+','-'), 1, "&#177;", NULL  },
    { V('1','2'), 1, "&#189;", NULL  },
    { V('1','4'), 1, "&#188;", NULL  },
    { V('3','4'), 1, "&#190;", NULL  },
    { V('F','i'), 3, "ffi", NULL  },
    { V('F','l'), 3, "ffl", NULL  },
    { V('a','a'), 1, "&#180;", NULL  },
    { V('a','p'), 1, "~", NULL  },
    { V('b','r'), 1, "|", NULL  },
    { V('b','u'), 1, "*", NULL  },
    { V('b','v'), 1, "|", NULL  },
    { V('c','i'), 1, "o", NULL  },
    { V('c','o'), 1, "&#169;", NULL  },
    { V('c','t'), 1, "&#162;", NULL  },
    { V('d','e'), 1, "&#176;", NULL  },
    { V('d','g'), 1, "+", NULL  },
    { V('d','i'), 1, "&#247;", NULL  },
    { V('e','m'), 1, "-", NULL  },
    { V('e','m'), 3, "---", NULL },
    { V('e','q'), 1, "=", NULL  },
    { V('e','s'), 1, "&#216;", NULL  },
    { V('f','f'), 2, "ff", NULL  },
    { V('f','i'), 2, "fi", NULL  },
    { V('f','l'), 2, "fl", NULL  },
    { V('f','m'), 1, "&#180;", NULL  },
    { V('g','a'), 1, "`", NULL  },
    { V('h','y'), 1, "-", NULL  },
    { V('l','c'), 2, "|&#175;", NULL  },
    { V('l','f'), 2, "|_", NULL  },
    { V('l','k'), 1, "<FONT SIZE=+2>{</FONT>", NULL  },
    { V('m','i'), 1, "-", NULL  },
    { V('m','u'), 1, "&#215;", NULL  },
    { V('n','o'), 1, "&#172;", NULL  },
    { V('o','r'), 1, "|", NULL  },
    { V('p','l'), 1, "+", NULL  },
    { V('r','c'), 2, "&#175;|", NULL  },
    { V('r','f'), 2, "_|", NULL  },
    { V('r','g'), 1, "&#174;", NULL  },
    { V('r','k'), 1, "<FONT SIZE=+2>}</FONT>", NULL  },
    { V('r','n'), 1, "&#175;", NULL  },
    { V('r','u'), 1, "_", NULL  },
    { V('s','c'), 1, "&#167;", NULL  },
    { V('s','l'), 1, "/", NULL  },
    { V('s','q'), 2, "[]", NULL  },
    { V('u','l'), 1, "_", NULL  },
    { 0, 0, NULL, NULL  }
};

/* default: print code */


static char eqndelimopen=0, eqndelimclose=0;
static char escapesym='\\', nobreaksym='\'', controlsym='.', fieldsym=0, padsym=0;

static char *buffer=NULL;
static int buffpos=0, buffmax=0;
static int scaninbuff=0;
static int itemdepth=0;
static int dl_set[20]= { 0 };
static int still_dd=0;
static int tabstops[20] = { 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96 };
static int maxtstop=12;
static int curpos=0;

static char *scan_troff(char *c, int san, char **result);
static char *scan_troff_mandoc(char *c, int san, char **result);

static char **argument=NULL;

static char charb[TINY_STR_MAX];

static void print_sig(void)
{
    char datbuf[NULL_TERMINATED(MED_STR_MAX)];
    struct tm *timetm;
    time_t clock;
    datbuf[0]='\0';
#ifdef TIMEFORMAT
    clock=time(NULL);
    timetm=gmtime(&clock);
    strftime(datbuf,MED_STR_MAX,TIMEFORMAT, timetm);
#endif
    printf(signature, datbuf);
}

static char *expand_char(int nr)
{
  STRDEF *h;
  h=chardef;
  if (!nr) return NULL;
  while (h)
      if (h->nr==nr) {
	  curpos+=h->slen;
	  return h->st;
      } else
	  h=h->next;
  charb[0]=nr/256;
  charb[1]=nr%256;
  charb[2]='\0';  
  if (charb[0] == '<') {	/* Fix up <= */
    charb[4] = charb[1];
    strncpy(charb, "&lt;", 4);
    charb[5] = '\0';
  }
  curpos+=2;
  return charb;
}

static char *expand_string(int nr)
{
  STRDEF *h=strdef;
  if (!nr) return NULL;
  while (h)
      if (h->nr==nr) {
	  curpos+=h->slen;
	  return h->st;
    } else
	h=h->next;
  return NULL;
}

static char *trim_compress_suffix(char *filename) 
{
  static char result[NULL_TERMINATED(MED_STR_MAX)];
  int trim;
  if (strcmp(filename + strlen(filename) - 3, ".gz") == 0) {
    trim = 3;
  }
  else if (strcmp(filename + strlen(filename) - 2, ".Z") == 0
	   || strcmp(filename + strlen(filename) - 2, ".z") == 0) {
    trim = 2;
  }
  else 
    trim = 0;

  strlimitcpy(result, filename, strlen(filename) - trim, MED_STR_MAX);
  result[strlen(filename) - trim + 1] = '\0';
  return result;
}

static char *read_man_page(char *filename) 
{
  int man_pipe = 0;
  char *man_buf = NULL;
  int i;

#ifndef DISABLE_ZCATS
  for (i = 0; zcats[i]; i++) {
    if (strcmp(filename + strlen(filename) - strlen(zcats[i]), zcats[i]) == 0) {
      man_pipe = 1;
      break;
    }
  }
#endif

  if (man_pipe) {  /* Input from a pipe */
#ifndef DISABLE_ZCATS
    FILE *man_stream = NULL;
    int upto;
    int len;
    man_stream = pipe_open(zcats[i] + strlen(zcats[i]) + 1, filename);  
    /* fprintf(stderr, "openned .%s.\n", filename); */
    if (man_stream) {
      man_buf = stralloc(LARGE_STR_MAX + 5);
      man_buf[0] = '\n';
      for (upto = 1; 
	   (len = fread(man_buf + upto, 1, LARGE_STR_MAX, man_stream)) > 0;
	   upto += len) { 
	man_buf = strgrow(man_buf, LARGE_STR_MAX);
      }      
      man_buf[upto] = '\n';
      man_buf[upto + 1] = man_buf[upto + 2] = '\0';
      pipe_close(man_stream);
    }
#endif
  }
  else { /* Non-pipe input */
    FILE *man_stream = NULL;
    struct stat stbuf;
    int buf_size;
    if (stat(filename, &stbuf) == -1) { 
      return NULL;
    }
    buf_size = stbuf.st_size;
    man_buf = stralloc(buf_size+5);
    man_pipe = 0;
    man_stream = fopen(filename, "r");
    if (man_stream) {
      man_buf[0] = '\n';
      if (fread(man_buf+1, 1, buf_size, man_stream) == buf_size) {
	man_buf[buf_size] = '\n';
	man_buf[buf_size + 1] = man_buf[buf_size + 2] = '\0';
      }
      else {
	man_buf = NULL;
      }
      fclose(man_stream);
    }
  }
  /* fprintf(stderr, "read .%s.\n", filename); */
  return man_buf;
}


static char outbuffer[NULL_TERMINATED(HUGE_STR_MAX)];
static int obp=0;
static int no_newline_output=0;
static int newline_for_fun=0;
static int output_possible=0;
static int out_length=0;

static void add_links(char *c)
{
    /*
    ** Add the links to the output.
    ** At the moment the following are recognized:
    **
    ** name(*)                 -> ../man?/name.*
    ** method://string         -> method://string
    ** www.host.name           -> http://www.host.name
    ** ftp.host.name           -> ftp://ftp.host.name
    ** name@host               -> mailto:name@host
    ** <name.h>                -> file:/usr/include/name.h   (guess)
    **
    ** Other possible links to add in the future:
    **
    ** /dir/dir/file  -> file:/dir/dir/file
    */
    int i,j,nr;
    char *f, *g,*h;
    char *idtest[6]; /* url, mailto, www, ftp, manpage */
    out_length+=strlen(c);
    /* search for (section) */
    nr=0;
    idtest[0]=strstr(c+1,"://");
    idtest[1]=strchr(c+1,'@');
    idtest[2]=strstr(c,"www.");
    idtest[3]=strstr(c,"ftp.");
    idtest[4]=strchr(c+1,'(');
    idtest[5]=strstr(c+1,".h&gt;");
    for (i=0; i<6; i++) nr += (idtest[i]!=NULL);
    while (nr) {
	j=-1;
	for (i=0; i<6; i++)
	    if (idtest[i] && (j<0 || idtest[i]<idtest[j])) j=i;
	switch (j) {
	case 5: /* <name.h> */
	    f=idtest[5];
	    h=f+2;
	    g=f;
	    while (g>c && g[-1]!=';') g--;
	    if (g!=c) {
		char t;
		t=*g;
		*g='\0';
		fputs(c, stdout);
		*g=t;*h='\0';
		printf("<A HREF=\"file:/usr/include/%s\">%s</A>&gt;", g,g);
		c=f+6;
	    } else {
		f[5]='\0';
		fputs(c, stdout);
		f[5]=';';
		c=f+5;
	    }
	    break;
	case 4: /* manpage */
	    f=idtest[j];
	    /* check section */
	    g=strchr(f,')');
	    if (g && f-g<6 && (isalnum(f[-1]) || f[-1]=='>') &&
		((isdigit(f[1]) && f[1]!='0' &&
		  (f[2]==')' || (isalpha(f[2]) && f[3]==')') || f[2]=='X')) ||
		 (f[2]==')' && (f[1]=='n' || f[1]=='l')))) {
		/* this might be a link */
		h=f-1;
		/* skip html makeup */
		while (h>c && *h=='>') {
		    while (h!=c && *h!='<') h--;
		    if (h!=c) h--;
		}
		if (isalnum(*h)) {
		    char t,sec,subsec, *e;
		    e=h+1;
		    sec=f[1];
		    subsec=f[2];
		    if ((subsec=='X' && f[3]!=')')|| subsec==')') subsec='\0';
		    while (h>c && (isalnum(h[-1]) || h[-1]=='_' ||
				    h[-1]=='-' || h[-1]=='.'))
			h--;
		    t=*h;
		    *h='\0';
		    fputs(c, stdout);
		    *h=t;
		    t=*e;
		    *e='\0';
		    if (subsec)
			    /* change to whatever you're using */
			printf("<A HREF=\""
			       "file:/usr/man/man%c/%s.%c%c\">%s</A>", 
			       sec, h, sec, tolower(subsec), h);
/*msf - this is obsolete I think */
#if 0
			printf("<A HREF=\""
			       "%s"
			       "?man%c/%s.%c%c\">%s</A>", location_base,
			       sec, h, sec, tolower(subsec), h);
#endif
		    else
			printf("<A HREF=\""
			       "file:/usr/man/man%c/%s.%c\">%s</A>", 
			       sec, h, sec, h);
#if 0
			printf("<A HREF=\""
			       "/usr/man%s"
			       "?doctype=man&amp;docname=%s&amp;"
			       "docsec=%c\">%s</A>", location_base,
			       h, sec, h);
#endif
		    *e=t;
		    c=e;
		}
	    }
	    *f='\0';
	    fputs(c, stdout);
	    *f='(';
	    idtest[4]=f-1;
	    c=f;
	    break; /* manpage */
	case 3: /* ftp */
	case 2: /* www */
	    g=f=idtest[j];
	    while (*g && (isalnum(*g) || *g=='_' || *g=='-' || *g=='+' ||
			  *g=='.')) g++;
	    if (g[-1]=='.') g--;
	    if (g-f>4) {
		char t;
		t=*f; *f='\0';
		fputs(c, stdout);
		*f=t; t=*g;*g='\0';
		printf("<A HREF=\"%s://%s\">%s</A>", (j==3?"ftp":"http"),
		       f,f);
		*g=t;
		c=g;
	    } else {
		f[3]='\0';
		fputs(c, stdout);
		c=f+3;
		f[3]='.';
	    }
	    break;
	case 1: /* mailto */
	    g=f=idtest[1];
	    while (g>c && (isalnum(g[-1]) || g[-1]=='_' || g[-1]=='-' ||
			   g[-1]=='+' || g[-1]=='.' || g[-1]=='%')) g--;
	    h=f+1;
	    while (*h && (isalnum(*h) || *h=='_' || *h=='-' || *h=='+' ||
			  *h=='.')) h++;
	    if (*h=='.') h--;
	    if (h-f>4 && f-g>1) {
		char t;
		t=*g;
		*g='\0';
		fputs(c, stdout);
		*g=t;t=*h;*h='\0';
		printf("<A HREF=\"mailto:%s\">%s</A>",g,g);
		*h=t;
		c=h;
	    } else {
		*f='\0';
		fputs(c, stdout);
		*f='@';
		idtest[1]=c;
		c=f;
	    }
	    break;
	case 0: /* url */
	    g=f=idtest[0];
	    while (g>c && isalpha(g[-1]) && islower(g[-1])) g--;
	    h=f+3;
	    while (*h && !isspace(*h) && *h!='<' && *h!='>' && *h!='"' &&
		   *h!='&') h++;
	    if (f-g>2 && f-g<7 && h-f>3) {
		char t;
		t=*g;
		*g='\0';
		fputs(c, stdout);
		*g=t; t=*h; *h='\0';
		printf("<A HREF=\"%s\">%s</A>", g,g);
		*h=t;
		c=h;
	    } else {
		f[1]='\0';
		fputs(c, stdout);
		f[1]='/';
		c=f+1;
	    }
	    break;
	default:
	    break;
	}
	nr=0;
	if (idtest[0] && idtest[0]<c) idtest[0]=strstr(c+1,"://");
	if (idtest[1] && idtest[1]<c) idtest[1]=strchr(c+1,'@');
	if (idtest[2] && idtest[2]<c) idtest[2]=strstr(c,"www.");
	if (idtest[3] && idtest[3]<c) idtest[3]=strstr(c,"ftp.");
	if (idtest[4] && idtest[4]<c) idtest[4]=strchr(c+1,'(');
	if (idtest[5] && idtest[5]<c) idtest[5]=strstr(c+1,".h&gt;");
	for (i=0; i<6; i++) nr += (idtest[i]!=NULL);
    }
    fputs(c, stdout);
}

static int current_font=0;
static int current_size=0;
static int fillout=1;

static void out_html(char *c)
{
  if (!c) return;
  if (no_newline_output) {
      int i=0;
      no_newline_output=1;
      while (c[i]) {
	  if (!no_newline_output) c[i-1]=c[i];
	  if (c[i]=='\n') no_newline_output=0;
	  i++;
      }
      if (!no_newline_output) c[i-1]=0;
  }
  if (scaninbuff) {
      while (*c) {
	  if (buffpos>=buffmax) {
	      char *h;
	      h=realloc(buffer, buffmax*2);
	      if (!h) return;
	      buffer=h;
	      buffmax=buffmax*2;
	  }
	  buffer[buffpos++]=*c++;
      }
  } else
      if (output_possible) {
	  while (*c) {
	      outbuffer[obp++]=*c;
	      if (*c=='\n' || obp > HUGE_STR_MAX) {
		  outbuffer[obp]='\0';
		  add_links(outbuffer);
		  obp=0;
	      }
	      c++;
	  }
      }
}

#define FO0 ""
#define FC0 ""
#define FO1 "<I>"
#define FC1 "</I>"
#define FO2 "<B>"
#define FC2 "</B>"
#define FO3 "<TT>"
#define FC3 "</TT>"

static char *switchfont[16] = { ""     , FC0 FO1, FC0 FO2, FC0 FO3,
			 FC1 FO0, ""     , FC1 FO2, FC1 FO3,
			 FC2 FO0, FC2 FO1, ""     , FC2 FO3,
			 FC3 FO0, FC3 FO1, FC3 FO2, ""      };

static char *change_to_font(int nr)
{
  int i;
  switch (nr) {
  case '0': nr++;
  case '1': case '2': case '3': case '4': nr=nr-'1'; break;
  case V('C','W'): nr=3; break;
  case 'L': nr=3; break;
  case 'B': nr=2; break;
  case 'I': nr=1; break;
  case 'P': case 'R': nr=0; break;
  case 0: case 1: case 2: case 3: break;
  default: nr=0; break;
  }
  i= current_font*4+nr%4;
  current_font=nr%4;
  return switchfont[i];
}

static char sizebuf[200];

static char *change_to_size(int nr)
{
  int i;
  switch (nr) {
  case '0': case '1': case '2': case '3': case '4': case '5': case '6':
  case '7': case '8': case '9': nr=nr-'0'; break;
  case '\0': break;
  default: nr=current_size+nr; if (nr>9) nr=9; if (nr< -9) nr=-9; break;
  }
  if (nr==current_size) return "";
  i=current_font;
  sizebuf[0]='\0';
  strcat(sizebuf, change_to_font(0));
  if (current_size) strcat(sizebuf, "</FONT>");
  current_size=nr;
  if (nr) {
    int l;
    strcat(sizebuf, "<FONT SIZE=");
    l=strlen(sizebuf);
    if (nr>0) sizebuf[l++]='+'; else sizebuf[l++]='-',nr=-nr;
    sizebuf[l++]=nr+'0';
    sizebuf[l++]='>';
    sizebuf[l]='\0';
  }
  strcat(sizebuf, change_to_font(i));
  return sizebuf;
}

static int asint=0;
static int intresult=0;

#define SKIPEOL while (*c && *c++!='\n')

static int skip_escape=0;
static int single_escape=0;

static char *scan_escape(char *c)
{
    char *h=NULL;
    char b[5];
    INTDEF *intd;
    int exoutputp,exskipescape;
    int i,j;

    intresult=0;
    switch (*c) {
    case 'e': h="\\"; curpos++;break;
    case '0':
    case ' ': h="&nbsp;";curpos++; break;
    case '|': h=""; break;
    case '"': SKIPEOL; c--; h=""; break;
    case '$':
	if (argument) {
	    c++;
	    i=(*c -'1');
	    if (!(h=argument[i])) h="";
	}
	break;
    case 'z':
	c++;
	if (*c=='\\') { c=scan_escape(c+1); c--;h=""; }
	else {
	    b[0]=*c;
	    b[1]='\0';
	    h="";
	}
	break;
    case 'k': c++; if (*c=='(') c+=2;
    case '^':
    case '!':
    case '%':
    case 'a':
    case 'd':
    case 'r':
    case 'u':
    case '\n':
    case '&': h=""; break;
    case '(':
	c++;
	i= c[0]*256+c[1];
	c++;
	h = expand_char(i);
	break;
    case '*':
	c++;
	if (*c=='(') {
	    c++;
	    i= c[0]*256+c[1];
	    c++;
	} else
	    i= *c *256+' ';
	h = expand_string(i);
	break;
    case 'f':
	c++;	
	if (*c=='\\') {
	    c++;
	    c=scan_escape(c);
	    c--;
	    i=intresult;
	} else 	if (*c != '(')
	    i=*c;
	else {
	    c++;
	    i=c[0]*256+c[1];
	    c++;
	}
	if (!skip_escape) h=change_to_font(i); else h="";
	break;
    case 's':
	c++;
	j=0;i=0;
	if (*c=='-') {j= -1; c++;} else if (*c=='+') {j=1; c++;}
	if (*c=='0') c++; else if (*c=='\\') {
	    c++;
	    c=scan_escape(c);
	    i=intresult; if (!j) j=1;
	} else
	    while (isdigit(*c) && (!i || (!j && i<4))) i=i*10+(*c++)-'0';
	if (!j) { j=1; if (i) i=i-10; }
	if (!skip_escape) h=change_to_size(i*j); else h="";
	c--;
	break;
    case 'n':
	c++;
	j=0;
	switch (*c) {
	case '+': j=1; c++; break;
	case '-': j=-1; c++; break;
	default: break;
	}
	if (*c=='(') {
	    c++;
	    i=V(c[0],c[1]);
	    c=c+1;
	} else {
	    i=V(c[0],' ');
	}
	intd=intdef;
	while (intd && intd->nr!=i) intd=intd->next;
	if (intd) {
	    intd->val=intd->val+j*intd->incr;
	    intresult=intd->val;
	} else {
	    switch (i) {
	    case V('.','s'): intresult=current_size; break;
	    case V('.','f'): intresult=current_font; break;
	    default: intresult=0; break;
	    }
	}
	h="";
	break;
    case 'w':
	c++;
	i=*c;
	c++;
	exoutputp=output_possible;
	exskipescape=skip_escape;
	output_possible=0;
	skip_escape=1;
	j=0;
	while (*c!=i) {
	    j++;
	    if (*c==escapesym) c=scan_escape(c+1); else c++;
	}
	output_possible=exoutputp;
	skip_escape=exskipescape;
	intresult=j;
	break;
    case 'l': h="<HR>"; curpos=0;
    case 'b':
    case 'v':
    case 'x':
    case 'o':
    case 'L':
    case 'h':
	c++;
	i=*c;
	c++;
	exoutputp=output_possible;
	exskipescape=skip_escape;
	output_possible=0;
	skip_escape=1;
	while (*c != i)
	    if (*c==escapesym) c=scan_escape(c+1);
	    else c++;
	output_possible=exoutputp;
	skip_escape=exskipescape;
	break;
    case 'c': no_newline_output=1; break;
    case '{': newline_for_fun++; h="";break;
    case '}': if (newline_for_fun) newline_for_fun--; h="";break;
    case 'p': h="<BR>\n";curpos=0; break;
    case 't': h="\t";curpos=(curpos+8)&0xfff8; break;
    case '<': h="&lt;";curpos++; break;
    case '>': h="&gt;";curpos++; break;
    case '\\': if (single_escape) { c--; break;}
    default: b[0]=*c; b[1]=0; h=b; curpos++; break;
    }
    c++;
    if (!skip_escape) out_html(h);
    return c;
}

typedef struct TABLEITEM TABLEITEM;

struct TABLEITEM {
    char *contents;
    int size,align,valign,colspan,rowspan,font,vleft,vright,space,width;
    TABLEITEM *next;
};

static TABLEITEM emptyfield = {NULL,0,0,0,1,1,0,0,0,0,0,NULL};

typedef struct TABLEROW TABLEROW;

struct TABLEROW {
    TABLEITEM *first;
    TABLEROW *prev, *next;
};

static char *tableopt[]= { "center", "expand", "box", "allbox", "doublebox",
			   "tab", "linesize", "delim", NULL };
static int tableoptl[] = { 6,6,3,6,9,3,8,5,0};


static void clear_table(TABLEROW *table)
{
    TABLEROW *tr1,*tr2;
    TABLEITEM *ti1,*ti2;

    tr1=table;
    while (tr1->prev) tr1=tr1->prev;
    while (tr1) {
	ti1=tr1->first;
	while (ti1) {
	    ti2=ti1->next;
	    if (ti1->contents) free(ti1->contents);
	    free(ti1);
	    ti1=ti2;
	}
	tr2=tr1;
	tr1=tr1->next;
	free(tr2);
    }
}

static char *scan_expression(char *c, int *result);

static char *scan_format(char *c, TABLEROW **result, int *maxcol)
{
    TABLEROW *layout, *currow;
    TABLEITEM *curfield;
    int i,j;
    if (*result) {
	clear_table(*result);
    }
    layout= currow=(TABLEROW*) malloc(sizeof(TABLEROW));
    currow->next=currow->prev=NULL;
    currow->first=curfield=(TABLEITEM*) malloc(sizeof(TABLEITEM));
    *curfield=emptyfield;
    while (*c && *c!='.') {
	switch (*c) {
	case 'C': case 'c': case 'N': case 'n':
	case 'R': case 'r': case 'A': case 'a':
	case 'L': case 'l': case 'S': case 's':
	case '^': case '_':
	    if (curfield->align) {
		curfield->next=(TABLEITEM*)malloc(sizeof(TABLEITEM));
		curfield=curfield->next;
		*curfield=emptyfield;
	    }
	    curfield->align=toupper(*c);
	    c++;
	    break;
	case 'i': case 'I': case 'B': case 'b':
	    curfield->font = toupper(*c);
	    c++;
	    break;
	case 'f': case 'F':
	    c++;
	    curfield->font = toupper(*c);
	    c++;
	    if (!isspace(*c)) c++;
	    break;
	case 't': case 'T': curfield->valign='t'; c++; break;
	case 'p': case 'P':
	    c++;
	    i=j=0;
	    if (*c=='+') { j=1; c++; }
	    if (*c=='-') { j=-1; c++; }
	    while (isdigit(*c)) i=i*10+(*c++)-'0';
	    if (j) curfield->size= i*j; else curfield->size=j-10;
	    break;
	case 'v': case 'V':
	case 'w': case 'W':
	    c=scan_expression(c+2,&curfield->width);
	    break;
	case '|':
	    if (curfield->align) curfield->vleft++;
	    else curfield->vright++;
	    c++;
	    break;
	case 'e': case 'E':
	    c++;
	    break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    i=0;
	    while (isdigit(*c)) i=i*10+(*c++)-'0';
	    curfield->space=i;
	    break;
	case ',': case '\n':
	    currow->next=(TABLEROW*)malloc(sizeof(TABLEROW));
	    currow->next->prev=currow;
	    currow=currow->next;
	    currow->next=NULL;
	    curfield=currow->first=(TABLEITEM*)malloc(sizeof(TABLEITEM));
	    *curfield=emptyfield;
	    c++;
	    break;
	default:
	    c++;
	    break;
	}
    }
    if (*c=='.') while (*c++!='\n');
    *maxcol=0;
    currow=layout;
    while (currow) {
	curfield=layout->first;
	i=0;
	while (curfield) {
	    i++;
	    curfield=curfield->next;
	}
	if (i>*maxcol) *maxcol=i;
	currow=currow->next;
    }
    *result=layout;
    return c;
}

static TABLEROW *next_row(TABLEROW *tr)
{
    if (tr->next) {
	tr=tr->next;
	if (!tr->next) next_row(tr);
	return tr;
    } else {
	TABLEITEM *ti, *ti2;
	tr->next=(TABLEROW*)malloc(sizeof(TABLEROW));
	tr->next->prev=tr;
	ti=tr->first;
	tr=tr->next;
	tr->next=NULL;
	if (ti) tr->first=ti2=(TABLEITEM*) malloc(sizeof(TABLEITEM));
	else tr->first=ti2=NULL;
	while (ti!=ti2) {
	    *ti2=*ti;
	    ti2->contents=NULL;
	    if ((ti=ti->next)) {
		ti2->next=(TABLEITEM*) malloc(sizeof(TABLEITEM));
	    }
	    ti2=ti2->next;
	}
	return tr;
    }
}

static char itemreset[20]="\\fR\\s0";

static char *scan_table(char *c)
{
    char *t, *h, *g;
    int center=0, expand=0, box=0, border=0, linesize=1;
    int i,j,maxcol=0, finished=0;
    int oldfont, oldsize,oldfillout;
    char itemsep='\t';
    TABLEROW *layout=NULL, *currow, *ftable;
    TABLEITEM *curfield;
    while (*c++!='\n');
    h=c;
    if (*h=='.') return c-1;
    oldfont=current_font;
    oldsize=current_size;
    oldfillout=fillout;
    out_html(change_to_font(0));
    out_html(change_to_size(0));
    if (!fillout) {
	fillout=1;
	out_html("</PRE>");
    }
    while (*h && *h!='\n') h++;
    if (h[-1]==';') {
	/* scan table options */
	while (c<h) {
	    while (isspace(*c)) c++;
	    for (i=0; tableopt[i] && strncmp(tableopt[i],c,tableoptl[i]);i++);
	    c=c+tableoptl[i];
	    switch (i) {
	    case 0: center=1; break;
	    case 1: expand=1; break;
	    case 2: box=1; break;
	    case 3: border=1; break;
	    case 4: box=2; break;
	    case 5: while (*c++!='('); itemsep=*c++; break;
	    case 6: while (*c++!='('); linesize=0;
		while (isdigit(*c)) linesize=linesize*10+(*c++)-'0';
		break;
	    case 7: while (*c!=')') c++;
	    default: break;
	    }
	    c++;
	}
	c=h+1;
    }
    /* scan layout */
    c=scan_format(c,&layout, &maxcol);
    currow=layout;
    next_row(currow);
    curfield=layout->first;
    i=0;
    while (!finished) {
	/* search item */
	h=c;
	if ((*c=='_' || *c=='=') && (c[1]==itemsep || c[1]=='\n')) {
	    if (c[-1]=='\n' && c[1]=='\n') {
		if (currow->prev) {
		    currow->prev->next=(TABLEROW*) malloc(sizeof(TABLEROW));
		    currow->prev->next->next=currow;
		    currow->prev->next->prev=currow->prev;
		    currow->prev=currow->prev->next;
		} else {
		    currow->prev=layout=(TABLEROW*) malloc(sizeof(TABLEROW));
		    currow->prev->prev=NULL;
		    currow->prev->next=currow;
		}
		curfield=currow->prev->first=
		    (TABLEITEM*) malloc(sizeof(TABLEITEM));
		*curfield=emptyfield;
		curfield->align=*c;
		curfield->colspan=maxcol;
		curfield=currow->first;
		c=c+2;
	    } else {
		if (curfield) {
		    curfield->align=*c;
		    do {
			curfield=curfield->next;
		    } while (curfield && curfield->align=='S');
		}
		if (c[1]=='\n') {
		    currow=next_row(currow);
		    curfield=currow->first;
		}
		c=c+2;
	    }
	} else if (*c=='T' && c[1]=='{') {
	    h=c+2;
	    c=strstr(h,"\nT}");
	    c++;
	    *c='\0';
	    g=NULL;
	    scan_troff(h,0,&g);
	    scan_troff(itemreset, 0,&g);
	    *c='T';
	    c+=3;
	    if (curfield) {
		curfield->contents=g;
		do {
		    curfield=curfield->next;
		} while (curfield && curfield->align=='S');
	    } else
		if (g) free(g);
	    if (c[-1]=='\n') {
		currow=next_row(currow);
		curfield=currow->first;
	    }
	} else if (*c=='.' && c[1]=='T' && c[2]=='&' && c[-1]=='\n') {
	    TABLEROW *hr;
	    while (*c++!='\n');
	    hr=currow;
	    currow=currow->prev;
	    hr->prev=NULL;
	    c=scan_format(c,&hr, &i);
	    hr->prev=currow;
	    currow->next=hr;
	    currow=hr;
	    next_row(currow);
	    curfield=currow->first;
	} else if (*c=='.' && c[1]=='T' && c[2]=='E' && c[-1]=='\n') {
	    finished=1;
	    while (*c++!='\n');
	    if (currow->prev)
		currow->prev->next=NULL;
	    currow->prev=NULL;
	    clear_table(currow);
	} else if (*c=='.' && c[-1]=='\n' && !isdigit(c[1])) {
	    /* skip troff request inside table (usually only .sp ) */
	    while (*c++!='\n');
	} else {
	    h=c;
	    while (*c && (*c!=itemsep || c[-1]=='\\') &&
		   (*c!='\n' || c[-1]=='\\')) c++;
	    i=0;
	    if (*c==itemsep) {i=1; *c='\n'; }
	    if (h[0]=='\\' && h[2]=='\n' &&
		(h[1]=='_' || h[1]=='^')) {
		if (curfield) {
		    curfield->align=h[1];
		    do {
			curfield=curfield->next;
		    } while (curfield && curfield->align=='S');
		}
		h=h+3;
	    } else {
		g=NULL;
		h=scan_troff(h,1,&g);
		scan_troff(itemreset,0,&g);
		if (curfield) {
		    curfield->contents=g;
		    do {
			curfield=curfield->next;
		    } while (curfield && curfield->align=='S');
		} else if (g) free(g);
	    }
	    if (i) *c=itemsep;
	    c=h;
	    if (c[-1]=='\n') {
		currow=next_row(currow);
		curfield=currow->first;
	    }
	}
    }
    /* calculate colspan and rowspan */
    currow=layout;
    while (currow->next) currow=currow->next;
    while (currow) {
	TABLEITEM *ti, *ti1=NULL, *ti2=NULL;
	ti=currow->first;
	if (currow->prev) ti1=currow->prev->first;
	while (ti) {
	    switch (ti->align) {
	    case 'S':
		if (ti2) {
		    ti2->colspan++;
		    if (ti2->rowspan<ti->rowspan) ti2->rowspan=ti->rowspan;
		}
		break;
	    case '^':
		if (ti1) ti1->rowspan++;
	    default:
		if (!ti2) ti2=ti;
		else {
		    do {
			ti2=ti2->next;
		    } while (ti2 && curfield->align=='S');
		}
		break;
	    }
	    ti=ti->next;
	    if (ti1) ti1=ti1->next;
	}
	currow=currow->prev;
    }
    /* produce html output */
    if (center) out_html("<CENTER>");
    if (box==2) out_html("<TABLE BORDER><TR><TD>");
    out_html("<TABLE");
    if (box || border) {
	out_html(" BORDER");
	if (!border) out_html("><TR><TD><TABLE");
	if (expand) out_html(" WIDTH=100%");
    }
    out_html(">\n");
    currow=layout;
    while (currow) {
	j=0;
	out_html("<TR VALIGN=top>");
	curfield=currow->first;
	while (curfield) {
	    if (curfield->align!='S' && curfield->align!='^') {
		out_html("<TD");
		switch (curfield->align) {
		case 'N':
		    curfield->space+=4;
		case 'R':
		    out_html(" ALIGN=right");
		    break;
		case 'C':
		    out_html(" ALIGN=center");
		default:
		    break;
		}
		if (!curfield->valign && curfield->rowspan>1)
		    out_html(" VALIGN=center");
		if (curfield->colspan>1) {
		    char buf[5];
		    out_html(" COLSPAN=");
		    sprintf(buf, "%i", curfield->colspan);
		    out_html(buf);
		}
		if (curfield->rowspan>1) {
		    char buf[5];
		    out_html(" ROWSPAN=");
		    sprintf(buf, "%i", curfield->rowspan);
		    out_html(buf);
		}
		j=j+curfield->colspan;
		out_html(">");
		if (curfield->size) out_html(change_to_size(curfield->size));
		if (curfield->font) out_html(change_to_font(curfield->font));
		switch (curfield->align) {
		case '=': out_html("<HR><HR>"); break;
		case '_': out_html("<HR>"); break;
		default:
		    if (curfield->contents) out_html(curfield->contents);
		    break;
		}
		if (curfield->space)
		    for (i=0; i<curfield->space;i++) out_html("&nbsp;");
		if (curfield->font) out_html(change_to_font(0));
		if (curfield->size) out_html(change_to_size(0));
		if (j>=maxcol && curfield->align>'@' && curfield->align!='_')
		    out_html("<BR>");
		out_html("</TD>");
	    }
	    curfield=curfield->next;
	}
	out_html("</TR>\n");
	currow=currow->next;
    }
    if (box && !border) out_html("</TABLE>");
    out_html("</TABLE>");
    if (box==2) out_html("</TABLE>");
    if (center) out_html("</CENTER>\n");
    else out_html("\n");
    if (!oldfillout) out_html("<PRE>");
    fillout=oldfillout;
    out_html(change_to_size(oldsize));
    out_html(change_to_font(oldfont));
    return c;
}

static char *scan_expression(char *c, int *result)
{
    int value=0,value2,j=0,sign=1,opex=0;
    char oper='c';

    if (*c=='!') {
	c=scan_expression(c+1, &value);
	value= (!value);
    } else if (*c=='n') {
	c++;
	value=NROFF;
    } else if (*c=='t') {
	c++;
	value=1-NROFF;
    } else if (*c=='\'' || *c=='"' || *c<' ' || (*c=='\\' && c[1]=='(')) {
	/* ?string1?string2?
	** test if string1 equals string2.
	*/
	char *st1=NULL, *st2=NULL, *h;
	char *tcmp=NULL;
	char sep;
	sep=*c;
	if (sep=='\\') {
	    tcmp=c;
	    c=c+3;
	}
	c++;
	h=c;
	while (*c!= sep && (!tcmp || strncmp(c,tcmp,4))) c++;
	*c='\n';
	scan_troff(h, 1, &st1);
	*c=sep;
	if (tcmp) c=c+3;
	c++;
	h=c;
	while (*c!=sep && (!tcmp || strncmp(c,tcmp,4))) c++;
	*c='\n';
	scan_troff(h,1,&st2);
	*c=sep;
	if (!st1 && !st2) value=1;
	else if (!st1 || !st2) value=0;
	else value=(!strcmp(st1, st2));
	if (st1) free(st1);
	if (st2) free(st2);
	if (tcmp) c=c+3;
	c++;
    } else {
	while (*c && !isspace(*c) && *c!=')') {
	    opex=0;
	    switch (*c) {
	    case '(':
		c=scan_expression(c+1, &value2);
		value2=sign*value2;
		opex=1;
		break;
	    case '.':
	    case '0': case '1':
	    case '2': case '3':
	    case '4': case '5':
	    case '6': case '7':
	    case '8': case '9': {
		int num=0,denum=1;
		value2=0;
		while (isdigit(*c)) value2=value2*10+((*c++)-'0');
		if (*c=='.') {
		    c++;
		    while (isdigit(*c)) {
			num=num*10+((*c++)-'0');
			denum=denum*10;
		    }
		}
		if (isalpha(*c)) {
		    /* scale indicator */
		    switch (*c) {
		    case 'i': /* inch -> 10pt */
			value2=value2*10+(num*10+denum/2)/denum;
			num=0;
			break;
		    default:
			break;
		    }
		    c++;
		}
		value2=value2+(num+denum/2)/denum;
		value2=sign*value2;
		opex=1;
		break;
	    }
	    case '\\':
		c=scan_escape(c+1);
		value2=intresult*sign;
		if (isalpha(*c)) c++; /* scale indicator */
		opex=1;
		break;
	    case '-':
		if (oper) { sign=-1; c++; break; }
	    case '>':
	    case '<':
	    case '+':
	    case '/':
	    case '*':
	    case '%':
	    case '&':
	    case '=':
	    case ':':
		if (c[1]=='=') oper=(*c++) +16; else oper=*c;
		c++;
		break;
	    default: c++; break;
	    }
	    if (opex) {
		sign=1;
		switch (oper) {
		case 'c': value=value2; break;
		case '-': value=value-value2; break;
		case '+': value=value+value2; break;
		case '*': value=value*value2; break;
		case '/': if (value2) value=value/value2; break;
		case '%': if (value2) value=value%value2; break;
		case '<': value=(value<value2); break;
		case '>': value=(value>value2); break;
		case '>'+16: value=(value>=value2); break;
		case '<'+16: value=(value<=value2); break;
		case '=': case '='+16: value=(value==value2); break;
		case '&': value = (value && value2); break;
		case ':': value = (value || value2); break;
		default: fprintf(stderr, "man2html: unknown operator %c.\n", oper);
		}
		oper=0;
	    }
	}
	if (*c==')') c++;
    }
    *result=value;
    return c;
}

static void trans_char(char *c, char s, char t)
{
    char *sl=c;
    int slash=0;
    while (*sl!='\n' || slash) {
	if (!slash) {
	    if (*sl==escapesym)
		slash=1;
	    else if (*sl==s)
		*sl=t;
	} else slash=0;
	sl++;
    }
}

static char *fill_words(char *c, char *words[], int *n)
{
    char *sl=c;
    int slash=0;
    int skipspace=0;
    *n=0;
    words[*n]=sl;
    while (*sl && (*sl!='\n' || slash)) {
	if (!slash) {
	    if (*sl=='"') {
		*sl='\a';
		skipspace=!skipspace;
	    } else if (*sl==escapesym)
		slash=1;
	    else if ((*sl==' ' || *sl=='\t') && !skipspace) {
		*sl='\n';
		if (words[*n]!=sl) (*n)++;
		words[*n]=sl+1;
	    }
	} else {
	    if (*sl=='"') {
		sl--;
		*sl='\n';
		if (words[*n]!=sl) (*n)++;
		sl++;
		while (*sl && *sl !='\n') sl++;
		words[*n]=sl;
		sl--;
	    }
	    slash=0;
	}
	sl++;
    }
    if (sl!=words[*n]) (*n)++;
    return sl;
}

static char *abbrev_list[] = {
    "GSBG", "Getting Started ",
    "SUBG", "Customizing SunOS",
    "SHBG", "Basic Troubleshooting",
    "SVBG", "SunView User's Guide",
    "MMBG", "Mail and Messages",
    "DMBG", "Doing More with SunOS",
    "UNBG", "Using the Network",
    "GDBG", "Games, Demos &amp; Other Pursuits",
    "CHANGE", "SunOS 4.1 Release Manual",
    "INSTALL", "Installing SunOS 4.1",
    "ADMIN", "System and Network Administration",
    "SECUR", "Security Features Guide",
    "PROM", "PROM User's Manual",
    "DIAG", "Sun System Diagnostics",
    "SUNDIAG", "Sundiag User's Guide",
    "MANPAGES", "SunOS Reference Manual",
    "REFMAN", "SunOS Reference Manual",
    "SSI", "Sun System Introduction",
    "SSO", "System Services Overview",
    "TEXT", "Editing Text Files",
    "DOCS", "Formatting Documents",
    "TROFF", "Using <B>nroff</B> and <B>troff</B>",
    "INDEX", "Global Index",
    "CPG", "C Programmer's Guide",
    "CREF", "C Reference Manual",
    "ASSY", "Assembly Language Reference",
    "PUL", "Programming Utilities and Libraries",
    "DEBUG", "Debugging Tools",
    "NETP", "Network Programming",
    "DRIVER", "Writing Device Drivers",
    "STREAMS", "STREAMS Programming",
    "SBDK", "SBus Developer's Kit",
    "WDDS", "Writing Device Drivers for the SBus",
    "FPOINT", "Floating-Point Programmer's Guide",
    "SVPG", "SunView 1 Programmer's Guide",
    "SVSPG", "SunView 1 System Programmer's Guide",
    "PIXRCT", "Pixrect Reference Manual",
    "CGI", "SunCGI Reference Manual",
    "CORE", "SunCore Reference Manual",
    "4ASSY", "Sun-4 Assembly Language Reference",
    "SARCH", "<FONT SIZE=-1>SPARC</FONT> Architecture Manual",
    "KR", "The C Programming Language",
    NULL, NULL };

static char *lookup_abbrev(char *c)
{
    int i=0;

    if (!c) return "";
    while (abbrev_list[i] && strcmp(c,abbrev_list[i])) i=i+2;
    if (abbrev_list[i]) return abbrev_list[i+1];
    else return c;
}

static char *section_list[] = {
    "1", "User Commands ",
    "1C", "User Commands",
    "1G", "User Commands",
    "1S", "User Commands",
    "1V", "User Commands ",
    "2", "System Calls",
    "2V", "System Calls",
    "3", "C Library Functions",
    "3C", "Compatibility Functions",
    "3F", "Fortran Library Routines",
    "3K", "Kernel VM Library Functions",
    "3L", "Lightweight Processes Library",
    "3M", "Mathematical Library",
    "3N", "Network Functions",
    "3R", "RPC Services Library",
    "3S", "Standard I/O Functions",
    "3V", "C Library Functions",
    "3X", "Miscellaneous Library Functions",
    "4", "Devices and Network Interfaces",
    "4F", "Protocol Families",
    "4I", "Devices and Network Interfaces",
    "4M", "Devices and Network Interfaces",
    "4N", "Devices and Network Interfaces",
    "4P", "Protocols",
    "4S", "Devices and Network Interfaces",
    "4V", "Devices and Network Interfaces",
    "5", "File Formats",
    "5V", "File Formats",
    "6", "Games and Demos",
    "7", "Environments, Tables, and Troff Macros",
    "7V", "Environments, Tables, and Troff Macros",
    "8", "Maintenance Commands",
    "8C", "Maintenance Commands",
    "8S", "Maintenance Commands",
    "8V", "Maintenance Commands",
    "L", "Local Commands",
/* for Solaris: 
    "1", "User Commands",
    "1B", "SunOS/BSD Compatibility Package Commands",
    "1b", "SunOS/BSD Compatibility Package Commands",
    "1C", "Communication Commands ",
    "1c", "Communication Commands",
    "1F", "FMLI Commands ",
    "1f", "FMLI Commands",
    "1G", "Graphics and CAD Commands ",
    "1g", "Graphics and CAD Commands ",
    "1M", "Maintenance Commands",
    "1m", "Maintenance Commands",
    "1S", "SunOS Specific Commands",
    "1s", "SunOS Specific Commands",
    "2", "System Calls",
    "3", "C Library Functions",
    "3B", "SunOS/BSD Compatibility Library Functions",
    "3b", "SunOS/BSD Compatibility Library Functions",
    "3C", "C Library Functions",
    "3c", "C Library Functions",
    "3E", "C Library Functions",
    "3e", "C Library Functions",
    "3F", "Fortran Library Routines",
    "3f", "Fortran Library Routines",
    "3G", "C Library Functions",
    "3g", "C Library Functions",
    "3I", "Wide Character Functions",
    "3i", "Wide Character Functions",
    "3K", "Kernel VM Library Functions",
    "3k", "Kernel VM Library Functions",
    "3L", "Lightweight Processes Library",
    "3l", "Lightweight Processes Library",
    "3M", "Mathematical Library",
    "3m", "Mathematical Library",
    "3N", "Network Functions",
    "3n", "Network Functions",
    "3R", "Realtime Library",
    "3r", "Realtime Library",
    "3S", "Standard I/O Functions",
    "3s", "Standard I/O Functions",
    "3T", "Threads Library",
    "3t", "Threads Library",
    "3W", "C Library Functions",
    "3w", "C Library Functions",
    "3X", "Miscellaneous Library Functions",
    "3x", "Miscellaneous Library Functions",
    "4", "File Formats",
    "4B", "SunOS/BSD Compatibility Package File Formats",
    "4b", "SunOS/BSD Compatibility Package File Formats",
    "5", "Headers, Tables, and Macros",
    "6", "Games and Demos",
    "7", "Special Files",
    "7B", "SunOS/BSD Compatibility Special Files",
    "7b", "SunOS/BSD Compatibility Special Files",
    "8", "Maintenance Procedures",
    "8C", "Maintenance Procedures",
    "8c", "Maintenance Procedures",
    "8S", "Maintenance Procedures",
    "8s", "Maintenance Procedures",
    "9", "DDI and DKI",
    "9E", "DDI and DKI Driver Entry Points",
    "9e", "DDI and DKI Driver Entry Points",
    "9F", "DDI and DKI Kernel Functions",
    "9f", "DDI and DKI Kernel Functions",
    "9S", "DDI and DKI Data Structures",
    "9s", "DDI and DKI Data Structures",
    "L", "Local Commands",
*/
    NULL, "Misc. Reference Manual Pages",
    NULL, NULL
};

static char *section_name(char *c)
{
    int i=0;

    if (!c) return "";
    while (section_list[i] && strcmp(c,section_list[i])) i=i+2;
    if (section_list[i+1]) return section_list[i+1];
    else return c;
}

static char manidx[NULL_TERMINATED(HUGE_STR_MAX)];
static int subs=0;
static int mip=0;
static char label[5]="lbAA";

static void add_to_index(int level, char *item)
{
    char *c=NULL;
    label[3]++;
    if (label[3]>'Z') {
	label[3]='A';
	label[2]++;
    }
    if (level != subs) {
	if (subs) {
	    strmaxcpy(manidx+mip, "</DL>\n", HUGE_STR_MAX - mip);
	    mip+=6;
	} else {
	    strmaxcpy(manidx+mip, "<DL>\n", HUGE_STR_MAX - mip);
	    mip+=5;
	}
    }
    subs=level;
    scan_troff(item, 1, &c);
    sprintf(manidx+mip, "<DT><A HREF=\"#%s\">%s</A><DD>\n", label, c);
    if (c) free(c);
    while (manidx[mip]) mip++;
}

static char *skip_till_newline(char *c)
{
    int lvl=0;

    while (*c && *c!='\n' || lvl>0) {
	if (*c=='\\') {
	    c++;
	    if (*c=='}') lvl--; else if (*c=='{') lvl++;
	}
	c++;
    }
    c++;
    if (lvl<0 && newline_for_fun) {
	newline_for_fun = newline_for_fun+lvl;
	if (newline_for_fun<0) newline_for_fun=0;
    }
    return c;
}

static int ifelseval=0;

static char *scan_request(char *c)
{
				  /* BSD Mandoc stuff */
    static int mandoc_synopsis=0; /* True if we are in the synopsis section */
    static int mandoc_command=0;  /* True if this is mandoc page */
    static int mandoc_bd_options; /* Only copes with non-nested Bd's */

    int i,j,mode=0;
    char *h;
    char *wordlist[MAX_WORDLIST];
    int words;
    char *sl;
    STRDEF *owndef;
    while (*c==' ' || *c=='\t') c++;
    if (c[0]=='\n') return c+1;
    if (c[1]=='\n') j=1; else j=2;
    while (c[j]==' ' || c[j]=='\t') j++;
    if (c[0]==escapesym) {
	/* some pages use .\" .\$1 .\} */
	/* .\$1 is too difficult/stuppid */
	if (c[1]=='$') c=skip_till_newline(c);
	else
	    c = scan_escape(c+1);
    } else {
	i=V(c[0],c[1]);
	switch (i) {
	case V('a','b'):
	    h=c+j;
	    while (*h && *h !='\n') h++;
	    *h='\0';
	    if (scaninbuff && buffpos) {
		buffer[buffpos]='\0';
		puts(buffer);
	    }
	    /* fprintf(stderr, "%s\n", c+2); */
	    exit(0);
	    break;
	case V('d','i'):
	    {
		STRDEF *de;
		int oldcurpos=curpos;
		c=c+j;
		i=V(c[0],c[1]);
		if (*c=='\n') { c++;break; }
		while (*c && *c!='\n') c++;
		c++;
		h=c;
		while (*c && strncmp(c,".di",3)) while (*c && *c++!='\n');
		*c='\0';
		de=strdef;
		while (de && de->nr !=i) de=de->next;
		if (!de) {
		    de=(STRDEF*) malloc(sizeof(STRDEF));
		    de->nr=i;
		    de->slen=0;
		    de->next=strdef;
		    de->st=NULL;
		    strdef=de;
		} else {
		    if (de->st) free(de->st);
		    de->slen=0;
		    de->st=NULL;
		}
		scan_troff(h,0,&de->st);
		*c='.';
		while (*c && *c++!='\n');
		break;
	    }
	case V('d','s'):
	    mode=1;
	case V('a','s'):
	    {
		STRDEF *de;
		int oldcurpos=curpos;
		c=c+j;
		i=V(c[0],c[1]);
		j=0;
		while (c[j] && c[j]!='\n') j++;
		if (j<3) { c=c+j; break; }
		if (c[1]==' ') c=c+1; else c=c+2;
		while (isspace(*c)) c++;
		if (*c=='"') c++;
		de=strdef;
		while (de && de->nr != i) de=de->next;
		single_escape=1;
		curpos=0;
		if (!de) {
		    char *h;
		    de=(STRDEF*) malloc(sizeof(STRDEF));
		    de->nr=i;
		    de->slen=0;
		    de->next=strdef;
		    de->st=NULL;
		    strdef=de;
		    h=NULL;
		    c=scan_troff(c, 1, &h);
		    de->st=h;
		    de->slen=curpos;
		} else {
		    if (mode) {
			char *h=NULL;
			c=scan_troff(c, 1, &h);
			free(de->st);
			de->slen=0;
			de->st=h;
		    } else
			c=scan_troff(c,1,&de->st);
		    de->slen+=curpos;
		}
		single_escape=0;
		curpos=oldcurpos;
	    }
	    break;
	case V('b','r'):
	    if (still_dd) out_html("<DD>");
	    else out_html("<BR>\n");
	    curpos=0;
	    c=c+j;
	    if (c[0]==escapesym) { c=scan_escape(c+1); }
	    c=skip_till_newline(c);break;
	case V('c','2'):
	    c=c+j;
	    if (*c!='\n') { nobreaksym=*c; }
	    else nobreaksym='\'';
	    c=skip_till_newline(c);
	    break;
	case V('c','c'):
	    c=c+j;
	    if (*c!='\n') { controlsym=*c; }
	    else controlsym='.';
	    c=skip_till_newline(c);
	    break;
	case V('c','e'):
	    c=c+j;
	    if (*c=='\n') { i=1; }
	    else {
		i=0;
		while ('0'<=*c && *c<='9') {
		    i=i*10+*c-'0';
		    c++;
		}
	    }
	    c=skip_till_newline(c);
	    /* center next i lines */
	    if (i>0) {
		out_html("<CENTER>\n");
		while (i && *c) {
		    char *line=NULL;
		    c=scan_troff(c,1, &line);
		    if (line && strncmp(line, "<BR>", 4)) {
			out_html(line);
			out_html("<BR>\n");
			i--;
		    }
		}
		out_html("</CENTER>\n");
		curpos=0;
	    }
	    break;
	case V('e','c'):
	    c=c+j;
	    if (*c!='\n') { escapesym=*c; }
	    else escapesym='\\';
	    break;
	    c=skip_till_newline(c);
	case V('e','o'):
	    escapesym='\0';
	    c=skip_till_newline(c);
	    break;
	case V('e','x'):
	    exit(0);
	    break;
	case V('f','c'):
	    c=c+j;
	    if  (*c=='\n') {
		fieldsym=padsym='\0';
	    } else {
		fieldsym=c[0];
		padsym=c[1];
	    }
	    c=skip_till_newline(c);
	    break;
	case V('f','i'):
	    if (!fillout) {
		out_html(change_to_font(0));
		out_html(change_to_size('0'));
		out_html("</PRE>\n");
	    }
	    curpos=0;
	    fillout=1;
	    c=skip_till_newline(c);
	    break;
	case V('f','t'):
	    c=c+j;
	    if (*c=='\n') {
		out_html(change_to_font(0));
	    } else {
		if (*c==escapesym) {
		    int fn;
		    c=scan_expression(c, &fn);
		    c--;
		    out_html(change_to_font(fn));
		} else {
		    out_html(change_to_font(*c));
		    c++;
		}
	    }
	    c=skip_till_newline(c);
	    break;
	case V('e','l'):
	    /* .el anything : else part of if else */
	    if (ifelseval) {
		c=c+j;
		c[-1]='\n';
		c=scan_troff(c,1,NULL);
	    } else
		c=skip_till_newline(c+j);
	    break;
	case V('i','e'):
	    /* .ie c anything : then part of if else */	
	case V('i','f'):
	    /* .if c anything
	     * .if !c anything
	     * .if N anything
	     * .if !N anything
	     * .if 'string1'string2' anything
	     * .if !'string1'string2' anything
	     */
	    c=c+j;
	    c=scan_expression(c, &i);
	    ifelseval=!i;
	    if (i) {
		*c='\n';
		c++;
		c=scan_troff(c,1,NULL);
	    } else
		c=skip_till_newline(c);
	    break;
	case V('i','g'):
	    {
		char *endwith="..\n";
		i=3;
		c=c+j;
		if (*c!='\n') {
		    endwith=c-1;i=1;
		    c[-1]='.';
		    while (*c && *c!='\n') c++,i++;
		}
		c++;
		while (*c && strncmp(c,endwith,i)) while (*c++!='\n');
		while (*c++!='\n');
		break;
	    }
	case V('n','f'):
	    if (fillout) {
		out_html(change_to_font(0));
		out_html(change_to_size('0'));
		out_html("<PRE>\n");
	    }
	    curpos=0;
	    fillout=0;
	    c=skip_till_newline(c);
	    break;
	case V('p','s'):
	    c=c+j;
	    if (*c=='\n') {
		out_html(change_to_size('0'));
	    } else {
		j=0;i=0;
		if (*c=='-') { j= -1;c++; } else if (*c=='+') { j=1;c++;}
		c=scan_expression(c, &i);
		if (!j) { j=1; if (i>5) i=i-10; }
		out_html(change_to_size(i*j));
	    }
	    c=skip_till_newline(c);
	    break;
	case V('s','p'):
	    c=c+j;
	    if (fillout) out_html("<P>"); else {
		out_html(NEWLINE);
		NEWLINE[0]='\n';
	    }
	    curpos=0;
	    c=skip_till_newline(c);
	    break;
	case V('s','o'):
	    {
		FILE *f;
		struct stat stbuf;
		int l = 0;
		char *buf;
		char *name=NULL;
		curpos=0;
		c=c+j;
		if (*c=='/') {
		    h=c;
		} else {
		    h=c-3;
		    h[0]='.';
		    h[1]='.';
		    h[2]='/';
		}
		while (*c!='\n') c++;
		*c='\0';
		scan_troff(h,1, &name);
		if (name[3]=='/') h=name+3; else h=name;
		if (stat(h, &stbuf)!=-1) l=stbuf.st_size;
		buf = stralloc(l+4);
#if NOCGI
                if (!out_length) {
		    char *t,*s;
		    t=strrchr(fname, '/');
		    if (!t) t=fname;
		    fprintf(stderr, "ln -s %s.html %s.html\n", h, t);
		    s=strrchr(t, '.');if (!s) s=t;
		    printf("<HTML><HEAD><TITLE> Manpage of %s</TITLE>\n"
			   "</HEAD><BODY>\n"
			   "See the manpage for <A HREF=\"%s.html\">%s</A>.\n"
			   "</BODY></HTML>\n",
			   s, h, h);
		} else
#endif
                {
		    /* this works alright, except for section 3 */
		    buf=read_man_page(h);
		    if (!buf) {
		      
			fprintf(stderr, "man2html: unable to open or read file %s.\n",
				h); 
			out_html("<BLOCKQUOTE>"
				 "man2html: unable to open or read file.\n");
			out_html(h);
			out_html("</BLOCKQUOTE>\n");
		    } 
		    else {
			buf[0]=buf[l]='\n';
			buf[l+1]=buf[l+2]='\0';
			scan_troff(buf+1,0,NULL);
		    }
		    if (buf) free(buf);
		}
		*c++='\n';
		break;
	    }
	case V('t','a'):
	    c=c+j;
	    j=0;
	    while (*c!='\n') {
		sl=scan_expression(c, &tabstops[j]);
		if (*c=='-' || *c=='+') tabstops[j]+=tabstops[j-1];
		c=sl;
		while (*c==' ' || *c=='\t') c++;
		j++;
	    }
	    maxtstop=j;
	    curpos=0;
	    break;
	case V('t','i'):
	    /*while (itemdepth || dl_set[itemdepth]) {
		out_html("</DL>\n");
		if (dl_set[itemdepth]) dl_set[itemdepth]=0;
		else itemdepth--;
	    }*/
	    out_html("<BR>\n");
	    c=c+j;
	    c=scan_expression(c, &j);
	    for (i=0; i<j; i++) out_html("&nbsp;");
	    curpos=j;
	    c=skip_till_newline(c);
	    break;
	case V('t','m'):
	    c=c+j;
	    h=c;
	    while (*c!='\n') c++;
	    *c='\0';
	    /* fprintf(stderr,"%s\n", h); */
	    *c='\n';
	    break;
	case V('B',' '):
	case V('B','\n'):
	case V('I',' '):
	case V('I','\n'):
            /* parse one line in a certain font */
	    out_html(change_to_font(*c));
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    c=scan_troff(c, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('O','P'):  /* groff manpages use this construction */
            /* .OP a b : [ <B>a</B> <I>b</I> ] */
	    mode=1;
	    c[0]='B'; c[1]='I';
	    out_html(change_to_font('R'));
	    out_html("[");
	    curpos++;
	case V('B','R'):
	case V('B','I'):
	case V('I','B'):
	case V('I','R'):
	case V('R','B'):
	case V('R','I'):
	    {
		char font[2] ;
		font[0] = c[0]; font[1] = c[1];
		c=c+j;
		if (*c=='\n') c++;
		sl=fill_words(c, wordlist, &words);
		c=sl+1;
		/* .BR name (section)
		** indicates a link. It will be added in the output routine.
		*/
		for (i=0; i<words; i++) {
		    if (mode) { out_html(" "); curpos++; }
		    wordlist[i][-1]=' ';
		    out_html(change_to_font(font[i&1]));
		    scan_troff(wordlist[i],1,NULL);
		}
		out_html(change_to_font('R'));
		if (mode) { out_html(" ]"); curpos++;}
		out_html(NEWLINE); if (!fillout) curpos=0; else curpos++;
	    }
	    break;
	case V('D','T'):
	    for (j=0;j<20; j++) tabstops[j]=(j+1)*8;
	    maxtstop=20;
	    c=skip_till_newline(c); break;
	case V('I','P'):
	    sl=fill_words(c+j, wordlist, &words);
	    c=sl+1;
            if (!dl_set[itemdepth]) {
		out_html("<DL COMPACT>\n");
		dl_set[itemdepth]=1;
	    }
	    out_html("<DT>");
            if (words) {
		scan_troff(wordlist[0], 1,NULL);
	    }
	    out_html("<DD>");
	    curpos=0;
	    break;
	case V('T','P'):
	    if (!dl_set[itemdepth]) {
		out_html("<DL COMPACT>\n");
		dl_set[itemdepth]=1;
	    }
	    out_html("<DT>");
	    c=skip_till_newline(c);
	    /* somewhere a definition ends with '.TP' */
	    if (!*c) still_dd=1; else {
		c=scan_troff(c,1,NULL);
		out_html("<DD>");
	    }
	    curpos=0;
	    break;
	case V('I','X'):
            /* general index */
	    sl = fill_words(c+j, wordlist, &words);
	    c=sl+1;
	    j=4;
	    while (idxlabel[j]=='Z') idxlabel[j--]='A';
	    idxlabel[j]++;
#ifdef MAKEINDEX
            fprintf(idxfile, "%s@%s@", fname, idxlabel);
	    for (j=0; j<words; j++) {
		h=NULL;
		scan_troff(wordlist[j], 1, &h);
		fprintf(idxfile, "_\b@%s", h);
		free(h);
	    }
	    fprintf(idxfile,"\n");
#endif
            out_html("<A NAME=\"");
	    out_html(idxlabel);
	    /* this will not work in mosaic (due to a bug).
	    ** Adding '&nbsp;' between '>' and '<' solves it, but creates
	    ** some space. A normal space does not work.
	    */
	    out_html("\"></A>");
	    break;
	case V('L','P'):
	case V('P','P'):
	    if (dl_set[itemdepth]) {
		out_html("</DL>\n");
		dl_set[itemdepth]=0;
	    }
	    if (fillout) out_html("<P>\n"); else {
		out_html(NEWLINE);
		NEWLINE[0]='\n';
	    }
	    curpos=0;
	    c=skip_till_newline(c);
	    break;
	case V('H','P'):
	    if (!dl_set[itemdepth]) {
		out_html("<DL COMPACT>");
		dl_set[itemdepth]=1;
	    }
	    out_html("<DT>\n");
	    still_dd=1;
	    c=skip_till_newline(c);
	    curpos=0;
	    break;
	case V('P','D'): 
	    c=skip_till_newline(c); 
	    break;
	case V('R','s'):	/* BSD mandoc */
	case V('R','S'):
	    sl=fill_words(c+j, wordlist, &words);
	    j=1;
	    if (words>0) scan_expression(wordlist[0], &j);
	    if (j>=0) {
		itemdepth++;
		dl_set[itemdepth]=0;
		out_html("<DL COMPACT><DT><DD>");
		c=skip_till_newline(c);
		curpos=0;
		break;
	    }
	case V('R','e'):	/* BSD mandoc */
	case V('R','E'):
	    if (itemdepth > 0) {
		if (dl_set[itemdepth]) out_html("</DL>");
		out_html("</DL>\n");
		itemdepth--;
	    }
	    c=skip_till_newline(c);
	    curpos=0;
	    break;
	case V('S','B'):
	    out_html(change_to_size(-1));
	    out_html(change_to_font('B'));
	    c=scan_troff(c+j, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html(change_to_size('0'));
	    break;
	case V('S','M'):
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html(change_to_size(-1));
	    trans_char(c,'"','\a');
	    c=scan_troff(c,1,NULL);
	    out_html(change_to_size('0'));
	    break;
	case V('S','s'):	/* BSD mandoc */
	    mandoc_command = 1;
	case V('S','S'):
	    mode=1;
	case V('S','h'):	/* BSD mandoc */
				/* hack for fallthru from above */
	    mandoc_command = !mode || mandoc_command; 
	case V('S','H'):
	    c=c+j;
	    if (*c=='\n') c++;
	    while (itemdepth || dl_set[itemdepth]) {
		out_html("</DL>\n");
		if (dl_set[itemdepth]) dl_set[itemdepth]=0;
		else if (itemdepth > 0) itemdepth--;
	    }
	    out_html(change_to_font(0));
	    out_html(change_to_size(0));
	    if (!fillout) {
		fillout=1;
		out_html("</PRE>");
	    }
	    trans_char(c,'"', '\a');
	    add_to_index(mode, c);
	    out_html("<A NAME=\"");
	    out_html(label);
	    /* &nbsp; for mosaic users */
	    if (mode) out_html("\">&nbsp;</A>\n<H3>");
	    else out_html("\">&nbsp;</A>\n<H2>");
	    mandoc_synopsis = strncmp(c, "SYNOPSIS", 8) == 0;
	    c = mandoc_command ? scan_troff_mandoc(c,1,NULL) : scan_troff(c,1,NULL);
	    if (mode) out_html("</H3>\n");
	    else out_html("</H2>\n");
	    curpos=0;
	    break;
	case V('T','S'):
	    c=scan_table(c);
	    break;
	case V('D','t'):	/* BSD mandoc */
	    mandoc_command = 1;
	case V('T','H'):
	    if (!output_possible) {
		sl = fill_words(c+j, wordlist, &words);
		if (words>1) {
		    for (i=1; i<words; i++) wordlist[i][-1]='\0';
		    *sl='\0';
		    output_possible=1;
		    out_html("<HTML><HEAD><TITLE>Manpage of ");
		    out_html(wordlist[0]);
		    out_html("</TITLE>\n</HEAD><BODY>\n<H1>");
		    out_html(wordlist[0]);
		    out_html("</H1>\nSection: ");
		    if (words>4) out_html(wordlist[4]);
		    else
			out_html(section_name(wordlist[1]));
		    out_html(" (");
		    out_html(wordlist[1]);
		    if (words>2) {
			out_html(")<BR>Updated: ");
			scan_troff(wordlist[2], 1, NULL);
		    } else out_html(")");
		    out_html("<BR><A HREF=\"#index\">Index</A>\n");
		    *sl='\n';
		    fputs("<BR><A HREF=\""
			  "/index.html"
			  "\">Return to Main Contents</A>\n", stdout);
		    out_html("<HR>\n");
		    if (mandoc_command) out_html("<BR>BSD mandoc<BR>");
		}
		c=sl+1;
	    } else c=skip_till_newline(c);
	    curpos=0;
	    break;
	case V('T','X'):
	    sl=fill_words(c+j, wordlist, &words);
	    *sl='\0';
	    out_html(change_to_font('I'));
	    if (words>1) wordlist[1][-1]='\0';
	    c=lookup_abbrev(wordlist[0]);
	    curpos+=strlen(c);
	    out_html(c);
	    out_html(change_to_font('R'));
	    if (words>1)
		out_html(wordlist[1]);
	    *sl='\n';
	    c=sl+1;
	    break;
	case V('r','m'):
            /* .rm xx : Remove request, macro or string */
	case V('r','n'):
            /* .rn xx yy : Rename request, macro or string xx to yy */
	    {
		STRDEF *de;
		c=c+j;
		i=V(c[0],c[1]);
		c=c+2;
		while (isspace(*c) && *c!='\n') c++;
		j=V(c[0],c[1]);
		while (*c && *c!='\n') c++;
		c++;
		de=strdef;
		while (de && de->nr!=j) de=de->next;
		if (de) {
		    if (de->st) free(de->st);
		    de->nr=0;
		}
		de=strdef;
		while (de && de->nr!=i) de=de->next;
		if (de) de->nr=j;
		break;
	    }
	case V('n','x'):
            /* .nx filename : next file. */
	case V('i','n'):
            /* .in +-N : Indent */
	    c=skip_till_newline(c);
	    break;
	case V('n','r'):
            /* .nr R +-N M: define and set number register R by +-N;
	    **  auto-increment by M
	    */
	    {
		INTDEF *intd;
		c=c+j;
		i=V(c[0],c[1]);
		c=c+2;
		intd=intdef;
		while (intd && intd->nr!=i) intd=intd->next;
		if (!intd) {
		    intd = (INTDEF*) malloc(sizeof(INTDEF));
		    intd->nr=i;
		    intd->val=0;
		    intd->incr=0;
		    intd->next=intdef;
		    intdef=intd;
		}
		while (*c==' ' || *c=='\t') c++;
		c=scan_expression(c,&intd->val);
		if (*c!='\n') {
		    while (*c==' ' || *c=='\t') c++;
		    c=scan_expression(c,&intd->incr);
		}
		c=skip_till_newline(c);
		break;
	    }
	case V('a','m'):
            /* .am xx yy : append to a macro. */
            /* define or handle as .ig yy */
	    mode=1;
	case V('d','e'):
            /* .de xx yy : define or redefine macro xx; end at .yy (..) */
            /* define or handle as .ig yy */
	    {
		STRDEF *de;
		int olen=0;
		c=c+j;
		sl=fill_words(c, wordlist, &words);
		i=V(c[0],c[1]);j=2;
		if (words==1) wordlist[1]=".."; else {
		    wordlist[1]--;
		    wordlist[1][0]='.';
		    j=3;
		}
		c=sl+1;
		sl=c;
		while (*c && strncmp(c,wordlist[1],j)) c=skip_till_newline(c);
		de=defdef;
		while (de && de->nr!= i) de=de->next;
		if (mode && de) olen=strlen(de->st);
		j=olen+c-sl;
		h = stralloc(j*2+4);
		if (h) {
		    for (j=0; j<olen; j++)
			h[j]=de->st[j];
		    if (!j || h[j-1]!='\n')
			h[j++]='\n';
		    while (sl!=c) {
			if (sl[0]=='\\' && sl[1]=='\\') {
			    h[j++]='\\'; sl++;
			} else
			    h[j++]=*sl;
			sl++;
		    }
		    h[j]='\0';
		    if (de) {
			if (de->st) free(de->st);
			de->st=h;
		    } else {
			de = (STRDEF*) malloc(sizeof(STRDEF));
			de->nr=i;
			de->next=defdef;
			de->st=h;
			defdef=de;
		    }
		}
	    }
	    c=skip_till_newline(c);
	    break;
	case V('B','l'):	/* BSD mandoc */
	  {
	    char list_options[NULL_TERMINATED(MED_STR_MAX)]; 
	    char *nl = strchr(c,'\n');
	    c=c+j;
	    if (dl_set[itemdepth]) {  /* These things can nest. */
	        itemdepth++;
	    }
	    if (nl) {		  /* Parse list options */
	        strlimitcpy(list_options, c, nl - c, MED_STR_MAX);
	    }
	    if (strstr(list_options, "-bullet")) { /* HTML Unnumbered List */
	        dl_set[itemdepth] = BL_BULLET_LIST;
	        out_html("<UL>\n");
	    }
	    else if (strstr(list_options, "-enum")) { /* HTML Ordered List */
	        dl_set[itemdepth] = BL_ENUM_LIST;
	        out_html("<OL>\n");
	    }
	    else {		  /* HTML Descriptive List */
	        dl_set[itemdepth] = BL_DESC_LIST;
	        out_html("<DL COMPACT>\n");
	    }
	    if (fillout) out_html("<P>\n"); else {
		out_html(NEWLINE);
		NEWLINE[0]='\n';
	    }
	    curpos=0;
	    c=skip_till_newline(c);
	    break;
	  }
	case V('E','l'):	/* BSD mandoc */
	    c=c+j;
	    if (dl_set[itemdepth] & BL_DESC_LIST) {
		out_html("</DL>\n");
	    }
	    else if (dl_set[itemdepth] & BL_BULLET_LIST) {
		out_html("</UL>\n");
	    }
	    else if (dl_set[itemdepth] & BL_ENUM_LIST) {
		out_html("</OL>\n");
	    }
	    dl_set[itemdepth]=0;
	    if (itemdepth > 0) itemdepth--;
	    if (fillout) out_html("<P>\n"); else {
		out_html(NEWLINE);
		NEWLINE[0]='\n';
	    }
	    curpos=0;
	    c=skip_till_newline(c);
	    break;
	case V('I','t'):	/* BSD mandoc */
	    c=c+j;
	    if (strncmp(c, "Xo", 2) == 0 && isspace(*(c+2))) {
	        c = skip_till_newline(c); 
	    }
	    if (dl_set[itemdepth] & BL_DESC_LIST) {
	        out_html("<DT>");
		out_html(change_to_font('B'));
	        if (*c=='\n') {	  /* Don't allow embedded comms after a newline */
		    c++;	  
		    c=scan_troff(c,1,NULL);
		}
		else {		  /* Do allow embedded comms on the same line. */
		    c=scan_troff_mandoc(c,1,NULL);
		}
		out_html(change_to_font('R'));
		out_html(NEWLINE);
		out_html("<DD>");
	    }
	    else if (dl_set[itemdepth] & (BL_BULLET_LIST | BL_ENUM_LIST)) {
	        out_html("<LI>");
		c=scan_troff_mandoc(c,1,NULL);
		out_html(NEWLINE);
	    }
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('B','k'):	/* BSD mandoc */
	case V('E','k'):	/* BSD mandoc */
	case V('D','d'):	/* BSD mandoc */
	case V('O','s'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('B','t'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    out_html(" is currently in beta test.");
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('B','x'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html("BSD ");
	    c=scan_troff_mandoc(c, 1, NULL);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('D','l'):	/* BSD mandoc */
	    c=c+j;
	    out_html(NEWLINE);
	    out_html("<BLOCKQUOTE>");	    
	    out_html(change_to_font('L'));
	    if (*c=='\n') c++;
	    c=scan_troff_mandoc(c, 1, NULL);	    
	    out_html(change_to_font('R'));
	    out_html("</BLOCKQUOTE>");	    
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('B','d'):	/* BSD mandoc */
	  {			/* Seems like a kind of example/literal mode */
	    char bd_options[NULL_TERMINATED(MED_STR_MAX)]; 
	    char *nl = strchr(c,'\n');
	    c=c+j;
	    if (nl) {
	      strlimitcpy(bd_options, c, nl - c, MED_STR_MAX);
	    }
	    out_html(NEWLINE);
	    mandoc_bd_options = 0; /* Remember options for terminating Bl */
	    if (strstr(bd_options, "-offset indent")) {
	        mandoc_bd_options |= BD_INDENT;
	        out_html("<BLOCKQUOTE>\n");
	    }
	    if (   strstr(bd_options, "-literal")
		|| strstr(bd_options, "-unfilled")) {
	        if (fillout) {
		    mandoc_bd_options |= BD_LITERAL;
		    out_html(change_to_font(0));
		    out_html(change_to_size('0'));
		    out_html("<PRE>\n");
		}
		curpos=0;
		fillout=0;
	    }
	    c=skip_till_newline(c);
	    break;
	  }
	case V('E','d'):	/* BSD mandoc */
	    if (mandoc_bd_options & BD_LITERAL) {
	        if (!fillout) {
		    out_html(change_to_font(0));
		    out_html(change_to_size('0'));
		    out_html("</PRE>\n");
		}
	    }
	    if (mandoc_bd_options & BD_INDENT)
	        out_html("</BLOCKQUOTE>\n");
	    curpos=0;
	    fillout=1;
	    c=skip_till_newline(c);
	    break;
	case V('B','e'):	/* BSD mandoc */
	    c=c+j;
	    if (fillout) out_html("<P>"); else {
		out_html(NEWLINE);
		NEWLINE[0]='\n';
	    }
	    curpos=0;
	    c=skip_till_newline(c);
	    break;
	case V('X','r'):	/* BSD mandoc */
	    {
	      /* Translate xyz 1 to xyz(1) 
	       * Allow for multiple spaces.  Allow the section to be missing.
	       */
	      char buff[NULL_TERMINATED(MED_STR_MAX)];
	      char *bufptr;
	      trans_char(c,'"','\a');
	      bufptr = buff;
	      c = c+j;
	      if (*c == '\n') c++; /* Skip spaces */
	      while (isspace(*c) && *c != '\n') c++;
	      while (isalnum(*c)) { /* Copy the xyz part */
		*bufptr = *c;
		bufptr++; if (bufptr >= buff + MED_STR_MAX) break;
		c++;
	      }
	      while (isspace(*c) && *c != '\n') c++;	/* Skip spaces */
	      if (isdigit(*c)) { /* Convert the number if there is one */
		*bufptr = '(';
		bufptr++; 
		if (bufptr < buff + MED_STR_MAX) {
		  while (isalnum(*c)) { 
		    *bufptr = *c; 
		    bufptr++; if (bufptr >= buff + MED_STR_MAX) break;
		    c++;
		  }
		  if (bufptr < buff + MED_STR_MAX) {
		    *bufptr = ')';
		    bufptr++;
		  }
		}
	      }

	      while (*c != '\n') { /* Copy the remainder */
		if (!isspace(*c)) {
		  *bufptr = *c;
		  bufptr++; if (bufptr >= buff + MED_STR_MAX) break;
		}
		c++;
	      }
	      *bufptr = '\n';
	      scan_troff_mandoc(buff, 1, NULL);

	      out_html(NEWLINE);
	      if (fillout) curpos++; else curpos=0;
	    }
	    break;	  
	case V('F','l'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    out_html("-");
	    if (*c!='\n') {
	        out_html(change_to_font('B'));
	        c=scan_troff_mandoc(c, 1, NULL);
	        out_html(change_to_font('R'));
            }
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('P','a'):	/* BSD mandoc */
	case V('P','f'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('P','p'):	/* BSD mandoc */
	    if (fillout) out_html("<P>\n"); else {
		out_html(NEWLINE);
		NEWLINE[0]='\n';
	    }
	    curpos=0;
	    c=skip_till_newline(c);
	    break;
	case V('D','q'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html("``");
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html("''");
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('O','p'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html(change_to_font('R'));
	    out_html("[");
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html("]");
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('O','o'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html(change_to_font('R'));
	    out_html("[");
	    c=scan_troff_mandoc(c, 1, NULL);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('O','c'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html("]");
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('P','q'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html("(");
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(")");
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('Q','l'):	/* BSD mandoc */
	  {			/* Single quote first word in the line */
	    char *sp;
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    sp = c;	  
	    do {		/* Find first whitespace after the 
				 * first word that isn't a mandoc macro 
				 */
	      while (*sp && isspace(*sp)) sp++;	    
	      while (*sp && !isspace(*sp)) sp++;
	    } while (*sp && isupper(*(sp-2)) && islower(*(sp-1)));

				/* Use a newline to mark the end of text to 
				 * be quoted 
				 */
	    if (*sp) *sp = '\n';
	    out_html("`");	/* Quote the text */
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html("'");
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	  }
	case V('S','q'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html("`");
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html("'");
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('A','r'):	/* BSD mandoc */
            /* parse one line in italics */
	    out_html(change_to_font('I'));
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') {	/* An empty Ar means "file ..." */
	        out_html("file ...");
	    }
	    else {
	        c=scan_troff_mandoc(c, 1, NULL);
	    }
	    out_html(change_to_font('R'));
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('A','d'):	/* BSD mandoc */
	case V('E','m'):	/* BSD mandoc */
	case V('V','a'):	/* BSD mandoc */
	case V('X','c'):	/* BSD mandoc */
            /* parse one line in italics */
	    out_html(change_to_font('I'));
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('N','d'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html(" - ");
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('N','m'):	/* BSD mandoc */
	  {
	    static char mandoc_name[NULL_TERMINATED(SMALL_STR_MAX)] = "";
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (mandoc_synopsis) {    /* Break lines only in the Synopsis. 
				       * The Synopsis section seems to be treated
				       * as a special case - Bummer!
				       */
	        static int count = 0; /* Don't break on the first Nm */
	        if (count) {
		    out_html("<BR>");
		}
		else {
		    char *end = strchr(c, '\n');
		    if (end) {	/* Remember the name for later. */
		        strlimitcpy(mandoc_name, c, end - c, SMALL_STR_MAX);
		    }
		}
		count++;
	    }			
	    out_html(change_to_font('B'));
	    while (*c == ' '|| *c == '\t') c++;
	    if (*c == '\n') {	/* If Nm has no argument, use one from an earlier
				 * Nm command that did have one.  Hope there aren't
				 * too many commands that do this.
				 */
	        out_html(mandoc_name);
	    } 
	    else {
	        c=scan_troff_mandoc(c, 1, NULL);
	    }
	    out_html(change_to_font('R'));
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	  }
	case V('C','d'):	/* BSD mandoc */
	case V('C','m'):	/* BSD mandoc */
	case V('I','c'):	/* BSD mandoc */
	case V('M','s'):	/* BSD mandoc */
	case V('O','r'):	/* BSD mandoc */
	case V('S','y'):	/* BSD mandoc */
            /* parse one line in bold */
	    out_html(change_to_font('B'));
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('D','v'):	/* BSD mandoc */
	case V('E','v'):	/* BSD mandoc */
	case V('F','r'):	/* BSD mandoc */
	case V('L','i'):	/* BSD mandoc */
	case V('N','o'):	/* BSD mandoc */
	case V('N','s'):	/* BSD mandoc */
	case V('T','n'):	/* BSD mandoc */
	case V('n','N'):	/* BSD mandoc */
	    trans_char(c,'"','\a');
	    c=c+j;
	    if (*c=='\n') c++;
	    out_html(change_to_font('B'));
	    c=scan_troff_mandoc(c, 1, NULL);
	    out_html(change_to_font('R'));
	    out_html(NEWLINE);
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('%','A'):	/* BSD mandoc biblio stuff */
	case V('%','D'):
	case V('%','N'):
	case V('%','O'):
	case V('%','P'):
	case V('%','Q'):
	case V('%','V'):
	    c=c+j;
	    if (*c=='\n') c++;
	    c=scan_troff(c, 1, NULL); /* Don't allow embedded mandoc coms */
	    if (fillout) curpos++; else curpos=0;
	    break;
	case V('%','B'):
	case V('%','J'):
	case V('%','R'):
	case V('%','T'):
	    c=c+j;
	    out_html(change_to_font('I'));
	    if (*c=='\n') c++;
	    c=scan_troff(c, 1, NULL); /* Don't allow embedded mandoc coms */
	    out_html(change_to_font('R'));
	    if (fillout) curpos++; else curpos=0;
	    break;
	default:
            /* search macro database of self-defined macros */
	    owndef = defdef;
	    while (owndef && owndef->nr!=i) owndef=owndef->next;
	    if (owndef) {
		char **oldargument;
		int deflen;
		int onff;
		sl=fill_words(c+j, wordlist, &words);
		c=sl+1;
		*sl='\0';
		for (i=1;i<words; i++) wordlist[i][-1]='\0';
		for (i=0; i<words; i++) {
		    char *h=NULL;
		    if (mandoc_command) {
		      scan_troff_mandoc(wordlist[i],1,&h);
		    }
		    else {
		      scan_troff(wordlist[i],1,&h);
		    }
		    wordlist[i]=h;
		}
		for (i=words;i<20; i++) wordlist[i]=NULL;
		deflen = strlen(owndef->st);
		for (i=0; owndef->st[deflen+2+i]=owndef->st[i]; i++);
		oldargument=argument;
		argument=wordlist;
		onff=newline_for_fun;
		if (mandoc_command) {
		  scan_troff_mandoc(owndef->st+deflen+2, 0, NULL);
		}
		else {
		  scan_troff(owndef->st+deflen+2, 0, NULL);
		}
		newline_for_fun=onff;
		argument=oldargument;
		for (i=0; i<words; i++) if (wordlist[i]) free(wordlist[i]);
		*sl='\n';
	    }
	    else if (mandoc_command && 
		     ((isupper(*c) && islower(*(c+1)))
		      || (islower(*c) && isupper(*(c+1))))
		     ) {	/* Let through any BSD mandoc commands that haven't
				 * been delt with.
				 * I don't want to miss anything out of the text.
				 */
	        char buf[4];
		strncpy(buf,c,2);
		buf[2] = ' ';	
		buf[3] = '\0';
		out_html(buf);	/* Print the command (it might just be text). */
	        c=c+j;
	        trans_char(c,'"','\a');
		if (*c=='\n') c++;
		out_html(change_to_font('R'));
		c=scan_troff(c, 1, NULL);
		out_html(NEWLINE);
		if (fillout) curpos++; else curpos=0;
	    }
	    else {
		c=skip_till_newline(c);
	    }
	    break;
	}
    }
    if (fillout) { out_html(NEWLINE); curpos++; }
    NEWLINE[0]='\n';
    return c;
}

static void flush(void)
{
}

static int contained_tab=0;
static int mandoc_line=0;	/* Signals whether to look for embedded mandoc
				 * commands.
				 */

static char *scan_troff(char *c, int san, char **result)
{   /* san : stop at newline */
    char *h;
    char intbuff[NULL_TERMINATED(MED_STR_MAX)];
    int ibp=0;
    int i;
#define FLUSHIBP  if (ibp) { intbuff[ibp]=0; out_html(intbuff); ibp=0; }
    char *exbuffer;
    int exbuffpos, exbuffmax, exscaninbuff, exnewline_for_fun;
    int usenbsp=0;

    exbuffer=buffer;
    exbuffpos=buffpos;
    exbuffmax=buffmax;
    exnewline_for_fun=newline_for_fun;
    exscaninbuff=scaninbuff;
    newline_for_fun=0;
    if (result) {
	if (*result) {
	    buffer=*result;
	    buffpos=strlen(buffer);
	    buffmax=buffpos;
	} else {
	    buffer = stralloc(LARGE_STR_MAX);
	    buffpos=0;
	    buffmax=LARGE_STR_MAX;
	}
	scaninbuff=1;
    }
    h=c;
    /* start scanning */

    while (*h && (!san || newline_for_fun || *h!='\n')) {

	if (*h==escapesym) {
	    h++;
	    FLUSHIBP;
	    h = scan_escape(h);
	} else if (*h==controlsym && h[-1]=='\n') {
	    h++;
	    FLUSHIBP;
	    h = scan_request(h);
	    if (san && h[-1]=='\n') h--;
	} else if (mandoc_line
		   && *(h) && isupper(*(h))
		   && *(h+1) && islower(*(h+1)) 
		   && *(h+2) && isspace(*(h+2))) {
	    /* BSD imbedded command eg ".It Fl Ar arg1 Fl Ar arg2" */
	    FLUSHIBP;
	    h = scan_request(h);
	    if (san && h[-1]=='\n') h--;
	} else if (*h==nobreaksym && h[-1]=='\n') {
	    h++;
	    FLUSHIBP;
	    h = scan_request(h);
	    if (san && h[-1]=='\n') h--;
	} else {
	    int mx;
	    if (h[-1]=='\n' && still_dd && isalnum(*h)) {
		/* sometimes a .HP request is not followed by a .br request */
		FLUSHIBP;
		out_html("<DD>");
		curpos=0;
		still_dd=0;
	    }
	    switch (*h) {
	    case '&':
		intbuff[ibp++]='&';
		intbuff[ibp++]='a';
		intbuff[ibp++]='m';
		intbuff[ibp++]='p';
		intbuff[ibp++]=';';
		curpos++;
		break;
	    case '<':
		intbuff[ibp++]='&';
		intbuff[ibp++]='l';
		intbuff[ibp++]='t';
		intbuff[ibp++]=';';
		curpos++;
		break;
	    case '>':
		intbuff[ibp++]='&';
		intbuff[ibp++]='g';
		intbuff[ibp++]='t';
		intbuff[ibp++]=';';
		curpos++;
		break;
	    case '"':
		intbuff[ibp++]='&';
		intbuff[ibp++]='q';
		intbuff[ibp++]='u';
		intbuff[ibp++]='o';
		intbuff[ibp++]='t';
		intbuff[ibp++]=';';
		curpos++;
		break;
	    case '\n':
		if (h[-1]=='\n' && fillout) {
		    intbuff[ibp++]='<';
		    intbuff[ibp++]='P';
		    intbuff[ibp++]='>';
		}
		if (contained_tab && fillout) {
		    intbuff[ibp++]='<';
		    intbuff[ibp++]='B';
		    intbuff[ibp++]='R';
		    intbuff[ibp++]='>';
		}
		contained_tab=0;
		curpos=0;
		usenbsp=0;
		intbuff[ibp++]='\n';
		break;
	    case '\t':
		{
		    int curtab=0;
		    contained_tab=1;
		    FLUSHIBP;
		    /* like a typewriter, not like TeX */
		    tabstops[19]=curpos+1;
		    while (curtab<maxtstop && tabstops[curtab]<=curpos)
			curtab++;
		    if (curtab<maxtstop) {
			if (!fillout) {
			    while (curpos<tabstops[curtab]) {
				intbuff[ibp++]=' ';
				if (ibp>480) { FLUSHIBP; }
				curpos++;
			    }
			} else {
			    out_html("<TT>");
			    while (curpos<tabstops[curtab]) {
				out_html("&nbsp;");
				curpos++;
			    }
			    out_html("</TT>");
			}
		    }
		}
		break;
	    default:
		if (*h==' ' && (h[-1]=='\n' || usenbsp)) {
		    FLUSHIBP;
		    if (!usenbsp && fillout) {
			out_html("<BR>");
			curpos=0;
		    }
		    usenbsp=fillout;
		    if (usenbsp) out_html("&nbsp;"); else intbuff[ibp++]=' ';
		} else if (*h>31 && *h<127) intbuff[ibp++]=*h;
		else if (((unsigned char)(*h))>127) {
		    intbuff[ibp++]='&';
		    intbuff[ibp++]='#';
		    intbuff[ibp++]='0'+((unsigned char)(*h))/100;
		    intbuff[ibp++]='0'+(((unsigned char)(*h))%100)/10;
		    intbuff[ibp++]='0'+((unsigned char)(*h))%10;
		    intbuff[ibp++]=';';
		}
		curpos++;
		break;
	    }
	    if (ibp > (MED_STR_MAX - 20)) FLUSHIBP;
	    h++;
	}
    }
    FLUSHIBP;
    if (buffer) buffer[buffpos]='\0';
    if (san && *h) h++;
    newline_for_fun=exnewline_for_fun;
    if (result) {
	*result = buffer;
	buffer=exbuffer;
	buffpos=exbuffpos;
	buffmax=exbuffmax;
	scaninbuff=exscaninbuff;
    }
    return h;
}


static char *scan_troff_mandoc(char *c, int san, char **result)
{
    char *ret, *end = c;
    int oldval = mandoc_line;
    mandoc_line = 1;
    while (*end && *end != '\n') {
        end++;
    }

    if (end > c + 2
        && ispunct(*(end - 1)) 
	&& isspace(*(end - 2)) && *(end - 2) != '\n') {
      /* Don't format lonely punctuation E.g. in "xyz ," format
       * the xyz and then append the comma removing the space.
       */
        *(end - 2) = '\n';
	ret = scan_troff(c, san, result);
        *(end - 2) = *(end - 1);
        *(end - 1) = ' ';
    }
    else {
	ret = scan_troff(c, san, result);
    }
    mandoc_line = oldval;
    return ret;
}


static char *sectionname=NULL;
static STRDEF *foundpages=NULL;

static int search_manpath_all(char *name)
{
    char smfbuf[NULL_TERMINATED(LARGE_STR_MAX)];
    char cmpbuf[NULL_TERMINATED(MED_STR_MAX)];
    int i,j,n,l,nr=0;
    DIR *dr;
    struct dirent *de;
    STRDEF *h=NULL;

    strmaxcpy(cmpbuf,name, MED_STR_MAX - 2);
    if (strlen(name) > MED_STR_MAX) {
      fprintf(stderr, 
	      "man2html: search_manpath_all - name too long: \n%-80s...\n",
	      name);
      exit(EXIT_FAILURE);
    }

    n=strlen(name);
    cmpbuf[n++]='.';
    cmpbuf[n+1]='\0';
    for (i=0; manpath[i]; i++) {
	strmaxcpy(smfbuf, manpath[i], LARGE_STR_MAX - 10);
	l=strlen(smfbuf);
	strcpy(smfbuf+l, "man");
	l+=3;
	smfbuf[l+1]='\0';
	for (j=0; sections[j]; j++) {
	    smfbuf[l]=sections[j];
	    cmpbuf[n]=sections[j];
	    if ((dr=opendir(smfbuf))) {
		while ((de=readdir(dr))) {
		    if (!strncasecmp(de->d_name, cmpbuf, n+1)) {
			if (h) {
			    h->next=(STRDEF*) malloc(sizeof(STRDEF));
			    h=h->next;
			} else
			    h=foundpages=(STRDEF*) malloc(sizeof(STRDEF));
			h->nr=i*256+j;
			nr++;
			h->st = strduplicate(de->d_name);
			h->next=NULL;
		    }
		}
		closedir(dr);
	    }
	}
    }
    return nr;
}

static int search_manpath_section(char *name, char* section)
{
    char smfbuf[NULL_TERMINATED(LARGE_STR_MAX)];
    char cmpbuf[NULL_TERMINATED(SMALL_STR_MAX)];
    int i,j,n,l,nr=0;
    DIR *dr;
    struct dirent *de;
    STRDEF *h=NULL;

    if (!section) return search_manpath_all(name);
    j=0;
    while (sections[j] && sections[j]!=section[0]) j++;
    if (!sections[j]) return search_manpath_all(name);
    if (strlen(name) > SMALL_STR_MAX) {
      fprintf(stderr, 
	      "man2html: search_manpath_section - name too long: \n%-80s...\n",
	      name);
      exit(EXIT_FAILURE);
    }
    strmaxcpy(cmpbuf,name, SMALL_STR_MAX);
    n=strlen(name);
    cmpbuf[n++]='.';
    cmpbuf[n++]=section[0];
    cmpbuf[n]='\0';
    for (i=0; manpath[i]; i++) {
	strmaxcpy(smfbuf, manpath[i], LARGE_STR_MAX - 10);
	l=strlen(smfbuf);
	strcpy(smfbuf+l, "man");
	l+=3;
	smfbuf[l]=section[0];
	smfbuf[l+1]='\0';
	if ((dr=opendir(smfbuf))) {
	    while ((de=readdir(dr))) {
		if (!strncasecmp(de->d_name, cmpbuf, n)) {
		    int stlen;
		    if (h) {
			h->next=(STRDEF*) malloc(sizeof(STRDEF));
			h=h->next;
		    } else
			h=foundpages=(STRDEF*) malloc(sizeof(STRDEF));
		    h->nr=i*256+j;
		    nr++;
		    h->st = strduplicate(de->d_name);
		    h->next=NULL;
		}
	    }
	    closedir(dr);
	}
    }
    return nr;
}

static char smfbuf[NULL_TERMINATED(LARGE_STR_MAX)];

static char *search_manpath(char *name)
{
    int i,j,n,l,nr=0;
    struct stat stbuf;

    for (i=0; manpath[i]; i++) {
	if (strlen(name) + strlen(manpath[i]) > LARGE_STR_MAX) {
	    fprintf(stderr, 
		    "man2html: search_manpath - name too long: \n%-80s...\n",
		    name);
	    exit(EXIT_FAILURE);
        }
	strmaxcpy(smfbuf, manpath[i], LARGE_STR_MAX);
	strmaxcat(smfbuf, name, LARGE_STR_MAX);
	if (stat(smfbuf, &stbuf) !=-1) return smfbuf;
    }
    return NULL;
}

static void get_man_config()
{
#ifdef MAN_CONFIG
  FILE *config = NULL;
  int num_paths = 1;			/* Zero is reserved for searches */
  int num_zcats = 0;
  char buffer[NULL_TERMINATED(MED_STR_MAX + 1)]; /* Allow for adding a "/" */

  config = fopen(MAN_CONFIG, "r");
  if (!config) {
    return;				/* Assume no config. */
  }

  while (fgets(buffer, MED_STR_MAX, config)) {
    char *dir, *line = buffer;
    line = line + strspn(line, " \t"); /* Skip spaces and tabs */
    if (num_paths < MAX_MAN_PATHS 
	&& strncmp("MANPATH", line, 7) == 0 
	&& (dir = strchr(line, '/'))) {	/* dir points to start of dir-name */
      char *end_dir = strpbrk(dir, " \t\n") ;
      if (end_dir) {			/* End of dir name. */
	(*end_dir) = '/';		/* Replace newline/space with "/" */
        (*(end_dir + 1)) = '\0';
      }
      manpath[num_paths] = strduplicate(dir);
      num_paths++;
    }
#ifndef DISABLE_ZCATS
    else if (num_zcats < MAX_ZCATS
	     && *line == '.') {
      char *sp, *new;
      line[strlen(line) - 1] = '\0';	/* Remove newline */
      zcats[num_zcats] = strduplicate(line);
      sp = strpbrk(zcats[num_zcats], " \t");
      if (sp) {				/* Chop off trailing spaces */
	(*sp) = '\0';
	num_zcats++;
      }
      else {
	free(zcats[num_zcats]);
      }
    }
#endif
  }
  if (num_paths != 0) {			/* Mark new end of paths list */
    manpath[num_paths] = NULL;
  }
#ifndef DISABLE_ZCATS
  if (num_zcats != 0) {			/* Mark new end of zcats list */
    zcats[num_zcats] = NULL;
  }
#endif

#ifndef UNSECURE_MANPATH
					/* Make sure settable first path is
					 * a valid path.
					 */

  if (strcmp(manpath[0], "/") != 0) {   /* Something other than default */
    int i;
    for (i = 1; manpath[i]; i++) {	/* Set - see if it is a valid prefix */
      if (strncmp(manpath[i], manpath[0], strlen(manpath[i])) == 0) {
	break;				/* valid */
      }
    }
					/* Also check for .. */
    if (!manpath[i] || strstr(manpath[0], "..") != NULL) { /* invalid */
      fprintf(stderr, "man2html: invalid -M path: %s\n", manpath[0]);
      manpath[0] = manpath[1];
    }
  }
#endif
  fclose(config);
#endif
}

void main(int argc, char **argv)
{
    FILE *f;
    char *t=NULL;
    int l,i;char *buf;
    int mopt=0;
    int notinsection=0;
    char *h, *fullname;
    STRDEF *stdf;

    t = getenv("PATH_INFO");

    if (!t || !*t) /* not :  cgi/man2html/mani/name.i */ {
	i=1;
	while (i<argc) {
	    switch (argv[i][0]) {
	    case '-':
		if (argv[i][1]=='M') {	/* First path to search */
		    mopt=1;
		    if (i+1<argc) {
			char *s1, *s2;
			i++;
			l=strlen(argv[i]);
			manpath[0] = stralloc(l+2);
			s1=manpath[0];
			s2=argv[i];
			while ((*s1=*s2)) {
			    if (*s2=='%') {
				s2++;
				*s1=0;
				if (isdigit(*s2)) *s1=*s2-'0'; else
				    *s1=tolower(*s2)-'a'+10;
				s2++;*s1=(*s1)*16;
				if (isdigit(*s2)) *s1=*s1+*s2-'0'; else
				    *s1=*s1+tolower(*s2)-'a'+10;
			    }
			    s1++;
			    s2++;
			}
			if (s1[-1]!='/') { *s1++='/';*s1='\0'; }
		    }
		}
		break;
	    case '1': case '2': case '3': case '4': case '5': case '6':
	    case '7': case '8': case '9': case 'n': case 'l':
					/* Section name eg 1 or 1n */
		if (!argv[i][1]) {
		    sectionname=argv[i];
		    break;
		} else
		    if (!argv[i][2] && isalpha(argv[i][1]) &&
			!isalpha(argv[i][0]) && islower(argv[i][1])) {
			sectionname=argv[i];
			break;
		    }
	    default:
		if (argv[i][0])
		    t=argv[i];
		break;
	    }
	    i++;
	}
	if (t) {			/* Man page name */
	    char *s1, *s2;
	    i++;
	    s2=t;
	    t = s1 = stralloc(strlen(t));
	    while ((*s1=*s2)) {
		if (*s2=='%') {
		    s2++;
		    *s1=0;
		    if (isdigit(*s2)) *s1=*s2-'0'; else
			*s1=tolower(*s2)-'a'+10;
		    s2++;*s1=(*s1)*16;
		    if (isdigit(*s2)) *s1=*s1+*s2-'0'; else
			*s1=*s1+tolower(*s2)-'a'+10;
		}
		s1++;
		s2++;
	    }
	}
    }
    if (!t || !*t) usage();		/* No man page requested */

    get_man_config();

    i=0;
    h=t;

    if (strncmp(CGIBASE, "http:", 5) == 0 && strncmp(CGIBASE, "http://", 7) != 0 
	&& getenv("SERVER_NAME")) {	

        /* Work out name for any redirect */

        char *ptr = CGIBASE;
	char *s = getenv("SERVER_NAME");
	if (strlen(s) > MED_STR_MAX - strlen(CGIBASE) - 7) {
	  fprintf(stderr, 
		  "man2html: error - server name is too long: \n%-80s...\n",
		  s);
	  exit(EXIT_FAILURE);
	}
        strmaxcpy(location_base, "http://", MED_STR_MAX);
        strmaxcat(location_base, getenv("SERVER_NAME"), MED_STR_MAX);
        strmaxcat(location_base, ptr + 5, MED_STR_MAX);	    
    }
    else {
        strcpy(location_base, CGIBASE);
    }

    while (*h) i=i+(*h++ == '/');
    if (i==0) {
	printf("Content-type: text/html\n\n"
	       "<HTML><HEAD>\n<TITLE>Error - no path specified</TITLE>\n"
	       "</HEAD><BODY>\n<H1>Must specify path for man page</H1>\n"
	       "</BODY></HTML>\n");
	exit(0);
/*  msf - old code to search for man pages which have no path specified */
#if 0
	if (sectionname) {
	    char fname[NULL_TERMINATED(LARGE_STR_MAX)];
	    if (strlen(t) + strlen(sectionname) + 6 > LARGE_STR_MAX) {
	        fprintf(stderr, 
			"man2html: error - name too long: man%c/%-80s.%-80s\n",
			sectionname[0],t,sectionname);
	        exit(EXIT_FAILURE);
	    }
	    sprintf(fname, "man%c/%s.%s", sectionname[0],t,sectionname);
	    h=search_manpath(fname);
	    if (h) {
		printf("Location: %s%s\n\n", location_base, h);
		exit(0);
	    } else {
		if (sectionname[1]) fname[strlen(fname)-1]='\0';
		i=search_manpath_section(t,sectionname);
		notinsection=(i>0);
	    }
	}
	if (!i) i=search_manpath_all(t);
	if (i==1 || (i>1 && mopt)) {
	    printf("Location: %s%sman%c/%s\n\n",
		   location_base,
		   manpath[foundpages->nr/256],
		   sections[foundpages->nr%256],
		   foundpages->st);
	    exit(0);
	}
	printf("Content-type: text/html\n\n"
	       "<HTML><HEAD>\n<TITLE>Index to %s manpages.</TITLE>\n"
	       "</HEAD><BODY>\n<H1>Index to %s manpages%s%c</H1>\n", t, t,
	       (notinsection?" for section ":""),
	       (notinsection?sectionname[0]:' '));
	if (!i)
	    printf("Sorry, no manpages available for %s.\n", t);
	else {
	    STRDEF *strd;
	    fputs("<UL>\n", stdout);
	    strd=foundpages;
	    while (strd) {
		printf("<LI><A HREF=\"" CGIBASE "%sman%c/%s\">"
		       "%s</A> (%s)\n", manpath[strd->nr/256],
		       sections[strd->nr%256], strd->st, trim_compress_suffix(strd->st),
		       manpath[strd->nr/256]);
		strd=strd->next;
	    }
	    fputs("</UL>\n", stdout);
	}
	fputs("</BODY></HTML>\n", stdout);
	exit(0);
#endif
    }
/* msf - we're doing this elsewhere, we just want html output */
/*    fputs("Content-type: text/html\n\n", stdout); */

    if (*t == '/') {
      for (i = 1; manpath[i]; i++) {
        if (strncmp(manpath[i], t, strlen (manpath [i])) == 0) {
	  break;
	}
      }
      if (!manpath[i]) {
	fprintf(stderr, "man2html: request for non man file %s\n", t);
	printf("<HTML><HEAD><TITLE>Manpage: Error</TITLE>\n"
	       "</HEAD><BODY>\n<H1>Only manpages are allowed.</H1>\n");
	exit(0);
      }
    }
    else {
      fprintf(stderr, "man2html: request for non man file %s\n", t);
      h=strstr(t, "man"); 
      if (!h || strchr(h, '/') == NULL) {
	printf("<HTML><HEAD><TITLE>Manpage: Error</TITLE>\n"
	       "</HEAD><BODY>\n<H1>Only manpages are allowed</H1>\n");
	exit(0);
      }
    }

    h=strstr(t,"/../");
    if (h) {
        fprintf(stderr, "man2html: attempt to use .. in man2html: %s\n", t);
	fputs("<HTML><HEAD><TITLE>Manpage: Error</TITLE>\n"
	      "</HEAD><BODY>\n<H1>Warning.</H1>\n"
	      "You still try to get files which are manpages. Using the\n"
	      "<B>..</B> construction to get to a different directory will\n"
	      "<B>not</B> work either. If you try this very often, you\n"
	      "will end up in a black list.\n"
	      "</BODY></HTML>\n", stdout);
	exit(0);
    }
    h=search_manpath(t);
    /* msf lets not search entire tree */
#if 0
    if (!h && i>2) {
	char *g;
	g=strrchr(t,'/');
	*g='.';
	h=strrchr(t,'/');
	*g='/';
	h=search_manpath(h);
    }
#endif
    if (!h) {
	printf("<HTML><HEAD><TITLE>No manpage for %s.</TITLE>\n"
	       "</HEAD><BODY>\n<H1>No manpage for %s.</H1>\n"
	       "Sorry, the manpage for %s does not exist%s%s",
	       t,t,t, (sectionname? " in section ":""),
	       (sectionname?sectionname:""));
/* msf - if no file found, we give up */
#if 0
	h=strrchr(t,'.');
	if (h) {
	    *h='\0';
	    sectionname=h+1;
	} else sectionname=NULL;
	h=strrchr(t,'/');
	if (!h) h=t; else h++;
	printf("<HTML><HEAD><TITLE>No manpage for %s.</TITLE>\n"
	       "</HEAD><BODY>\n<H1>No manpage for %s.</H1>\n"
	       "Sorry, the manpage for %s does not exist%s%s",
	       h,h,h, (sectionname? " in section ":""),
	       (sectionname?sectionname:""));
       	i=search_manpath_all(h);
	if (!i) {
	    if (sectionname)
	      fputs(", nor in any other section.\n", stdout);
	    else
	      fputs("in any section.\n", stdout);
	    fputs("<HR>\n"
		  "The links to other manual pages are not always correct.\n"
		  "Normally you will get a list of possible replacements,\n"
		  "but in this case the manual page just can't be found.\n",
		  stdout);
	} else {
	    STRDEF *strd;
	    printf(".\nMaybe you can use %s instead.\n<UL>\n",
		   (i>1?"one of the following pages":"this page"));
	    strd=foundpages;
	    while (strd) {
		printf("<LI><A HREF=\"" CGIBASE "%sman%c/%s\">"
		       "%s</A> (%s)\n", manpath[strd->nr/256],
		       sections[strd->nr%256], strd->st, trim_compress_suffix(strd->st),
		       manpath[strd->nr/256]);
		strd=strd->next;
	    }
	    fputs("</UL>\n", stdout);
	}
#endif
	fputs("</BODY></HTML>\n", stdout);
	exit(0);
    }
    fullname=h;
    t=strrchr(h,'/');
    if (t) {
	*t='\0';
	if (!chdir(h))
	    h=t+1;
	*t='/';
    }
    buf=read_man_page(h);
    if (!buf) {
	t=strrchr(h,'.');
	if (t) *t='\0';
	t=strrchr(h,'/');
	if (!t) t=h; else t++;
	printf("<HTML><HEAD><TITLE>No manpage for %s.</TITLE>\n"
	       "</HEAD><BODY>\n<H1>No manpage for %s.</H1>\n"
	       "Sorry, unable to convert the manpage for %s.\n"
	       "</BODY></HTML>\n", t,t,t);
	exit(0);
    }
    fname=h;
#ifdef MAKEINDEX
    idxfile=fopen(INDEXFILE, "a");
#endif
    stdf=&standardchar[0];
    i=0;
    while (stdf->nr) {
	stdf->next=&standardchar[i];
	stdf=stdf->next;
	i++;
    }
    chardef=&standardchar[0];
    stdf=&standardstring[0];
    i=0;
    while (stdf->nr) {
	stdf->next=&standardstring[i];
	stdf=stdf->next;
	i++;
    }
    strdef=&standardstring[0];
    intdef=&standardint[0];
    i=0;
    while (intdef->nr) {
	intdef->next=&standardint[i];
	intdef=intdef->next;
	i++;
    }
    intdef=&standardint[0];
    defdef=NULL;
    scan_troff(buf+1,0,NULL);
    while (itemdepth || dl_set[itemdepth]) {
	out_html("</DL>\n");
	if (dl_set[itemdepth]) dl_set[itemdepth]=0;
	else if (itemdepth > 0) itemdepth--;
    }
    out_html(change_to_font(0));
    out_html(change_to_size(0));
    if (!fillout) {
	fillout=1;
	out_html("</PRE>");
    }
    out_html(NEWLINE);
    if (output_possible) {
	/* &nbsp; for mosaic users */
	fputs("<HR>\n<A NAME=\"index\">&nbsp;</A><H2>Index</H2>\n<DL>\n", 
	      stdout);
	manidx[mip]=0;
	fputs(manidx,stdout);
	if (subs) fputs("</DL>\n", stdout);
	fputs("</DL>\n", stdout);
	print_sig();
	fputs("</BODY>\n</HTML>\n", stdout);
    } else {
	printf("<HTML><HEAD><TITLE>Invalid Manpage</TITLE></HEAD>\n"
	       "<BODY><H1>Invalid Manpage</H1>\n"
	       "You tried to retrieve an incorrect manpage.\n"
	       "The page does not contain a manpage header and will\n"
	       "not produce any output.\n"
	       "If the page is a formatted manpage, you might want to use\n"
	       "a different man2html (or cat2html) converter.\n"
	       "You can also try to load the\n"
	       "<A HREF=\"file://localhost%s\">plain file</A>\n",
	       fullname);
	print_sig();
	fputs("</BODY>\n</HTML>\n", stdout);
    }
#ifdef MAKEINDEX
    if (idxfile) fclose(idxfile);
#endif
    if (buf) free(buf);
}
