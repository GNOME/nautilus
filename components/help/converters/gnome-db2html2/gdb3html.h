#ifndef __GDB3HTML_H__
#define __GDB3HTML_H__

#include <parser.h>
#include <parserInternals.h>
#include <glib.h>
#include <string.h>

typedef enum ElementIndex {
	ARTICLE = 0,
	BOOK,
	SECTION,
	SECT1,
	SECT2,
	SECT3, /* 5 */
	SECT4,
	SECT5,
	PARA,
	FORMALPARA,
	BOOKINFO, /* 10 */
	ARTHEADER,
	ARTICLEINFO,
	AUTHORGROUP,
	AUTHOR,
	FIRSTNAME,
	OTHERNAME, /* 16 */
	SURNAME,
	AFFILIATION,
	EMAIL,
	ORGNAME,
	ADDRESS, /* 21 */
	COPYRIGHT,
	YEAR,
	HOLDER,
	TITLE,
	SUBTITLE,  /* 26 */
	ULINK,
	XREF,
	FOOTNOTE,
	FIGURE,
	GRAPHIC, /* 31 */
	CITETITLE,
	APPLICATION,
	FILENAME,
	ITEMIZEDLIST,
	ORDEREDLIST, /* 36 */
	VARIABLELIST,
	LISTITEM,
	PROGRAMLISTING,
	SGMLTAG,
	EMPHASIS, /* 41 */
	TIP,
	WARNING,
	IMPORTANT,
	NOTE,
	CDATA, /* 46 */
	SCREEN,
	SCREENSHOT,
	SCREENINFO,
	COMMAND,
	REPLACEABLE, /* 51 */
	FUNCTION,
	GUIBUTTON,
	GUIICON,
	GUILABEL,
	GUIMENU, /* 56 */
	GUIMENUITEM,
	HARDWARE,
	KEYCAP,
	KEYCODE,
	KEYSYM, /* 61 */
	LITERAL,
	PARAMETER,
	PROMPT,
	SYMBOL,
	USERINPUT, /* 66 */
	CAUTION,
	LEGALPARA,
	FIRSTTERM,
	STRUCTNAME,
	STRUCTFIELD, /* 71 */
	FUNCSYNOPSIS,
	FUNCPROTOTYPE,
	FUNCDEF,
	FUNCPARAMS,
	PARAMDEF, /* 76 */
	VOID,
	GUISUBMENU,
	INTERFACE,
	LINK, /* 80 */
	MENUCHOICE, 
	UNDEFINED /* 82 */
} ElementIndex;

typedef struct _ElementInfo ElementInfo;
struct _ElementInfo {
	ElementIndex index;
	gchar *name;
	startElementSAXFunc start_element_func;
	endElementSAXFunc end_element_func;
	charactersSAXFunc characters_func;
};

typedef struct _StackElement StackElement;
struct _StackElement {
	ElementInfo *info;
	gchar **atrs;
};


typedef struct _Context Context;
struct _Context {
	ElementInfo *elements;
	gchar *base_file;
	gchar *target_section;
	GList *stack;
	gpointer data;

	/* determine the "depth" that the current section is on.
	 * only applies to section */
	gint sect1;
	gint sect2;
	gint sect3;
	gint sect4;
	gint sect5;

	GSList *footnotes;
};

/* useful structs */
typedef struct AuthorInfo {
	gchar *firstname;
	gchar *othername;
	gchar *surname;
	gchar *email;
	gchar *orgname;
} AuthorInfo;

typedef struct HeaderInfo {
	gchar *title;
	gchar *subtitle;
	gchar *copyright_year;
	gchar *copyright_holder;
	GSList *authors;
} HeaderInfo;

typedef struct FigureInfo {
	gchar *title;
	gchar *alt;
	gchar *img;
	gchar *id;
} FigureInfo;

void article_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void article_end_element (Context *context, const gchar *name);
void artheader_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void html_em_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void html_em_end_element (Context *context, const gchar *name);
void html_tt_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void html_tt_end_element (Context *context, const gchar *name);
void para_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void para_end_element (Context *context, const gchar *name);
void ulink_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void ulink_end_element (Context *context, const gchar *name);
StackElement *find_first_element (Context *context, GSList *args);
ElementIndex find_first_parent (Context *context, GSList *args);

#endif
