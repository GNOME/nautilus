#ifndef __GDB3HTML_H__
#define __GDB3HTML_H__

#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/SAX.h>
#include <libxml/xmlmemory.h>
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
	GLOSSARYINFO,
	AUTHORGROUP,
	AUTHOR, /* 15 */
	FIRSTNAME,
	OTHERNAME, 
	SURNAME,
	AFFILIATION,
	EMAIL, /* 20 */
	ORGNAME,
	ADDRESS, 
	COPYRIGHT,
	YEAR,
	HOLDER, /* 25 */
	TITLE,
	SUBTITLE,  
	ULINK,
	XREF,
	FOOTNOTE,
	FIGURE,
	GRAPHIC, /* 32 */
	CITETITLE,
	APPLICATION,
	FILENAME,
	ITEMIZEDLIST,
	ORDEREDLIST, /* 37 */
	VARIABLELIST,
	LISTITEM,
	PROGRAMLISTING,
	SGMLTAG,
	EMPHASIS, /* 42 */
	TIP,
	WARNING,
	IMPORTANT,
	NOTE,
	CDATA, /* 47 */
	SCREEN,
	SCREENSHOT,
	SCREENINFO,
	COMMAND,
	REPLACEABLE, /* 52 */
	FUNCTION,
	GUIBUTTON,
	GUIICON,
	GUILABEL,
	GUIMENU, /* 57 */
	GUIMENUITEM,
	HARDWARE,
	KEYCAP,
	KEYCODE,
	KEYSYM, /* 62 */
	LITERAL,
	PARAMETER,
	PROMPT,
	SYMBOL,
	USERINPUT, /* 67 */
	CAUTION,
	LEGALPARA,
	FIRSTTERM,
	STRUCTNAME,
	STRUCTFIELD, /* 72 */
	FUNCSYNOPSIS,
	FUNCPROTOTYPE,
	FUNCDEF,
	FUNCPARAMS,
	PARAMDEF, /* 77 */
	VOID,
	GUISUBMENU,
	INTERFACE,
	LINK, /* 81 */
	MENUCHOICE, 
	TABLE,
	INFORMALTABLE,
	ROW,
	ENTRY, /* 86 */
	THEAD,
	TBODY,
	ACRONYM,
	MARKUP, /* 90 */
	SIMPLELIST,
	MEMBER,
	MOUSEBUTTON,
	SUPERSCRIPT,
	SYSTEMITEM, /* 95 */
	VARNAME,
	BLOCKQUOTE,
	QUOTE,
	OPTION,
	ENVAR, /* 100 */
	COMPUTEROUTPUT,
	INLINEGRAPHIC,
	LEGALNOTICE,
	QUESTION,
	ANSWER, /* 105 */
	CHAPTER,
	PREFACE,
	TERM,
	APPENDIX,
	DOCINFO,
	GLOSSARY, /* 110 */
	GLOSSDIV,
	GLOSSENTRY,
	GLOSSTERM,
	GLOSSSEE,
	GLOSSSEEALSO, /* 115 */
	EXAMPLE,
	VARLISTENTRY,
	STREET,
	CITY,
	COUNTRY, /* 120 */
	STATE,
	POSTCODE,
	LITERALLAYOUT,
	QANDAENTRY,
	QANDASET,
	BRIDGEHEAD,
	RELEASEINFO, /* 125 */
	UNDEFINED /* 126 */
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

typedef enum DocType {
	/* Need the DOC prefix to avoid conflicts with the above */
	ARTICLE_DOC,
	BOOK_DOC,
	GLOSSARY_DOC
} DocType;


typedef struct _Context Context;
struct _Context {
	ElementInfo *elements;
	gchar *base_file;
	gchar *base_path;
	gchar *target_section;
	GList *stack;
	gpointer data;
	GHashTable *figure_data;
	GHashTable *glossary_data;
	GList *glossentry_stack;
	GList *sect1id_stack;

	/* determine the "depth" that the current section is on.
	 * only applies to section and chapter */
        gint preface;
	gint chapter;
	gint sect1;
	gint sect2;
	gint sect3;
	gint sect4;
	gint sect5;
	gint appendix;

	gboolean empty_element; /* This is to determine if the element is
				   empty or not */
	gboolean in_glossterm;
	gboolean start_address_character;
	DocType doctype;
	GSList *footnotes;
	xmlParserCtxtPtr ParserCtxt;
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
	gchar *releaseinfo;
	GSList *authors;
} HeaderInfo;

typedef struct FigureInfo {
	gchar *title;
	gchar *alt;
	gchar *img;
	gchar *id;
} FigureInfo;

void article_start_element (Context *context, const char *name, const xmlChar **atrs);
void book_start_element (Context *context, const char *name, const xmlChar **atrs);
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
void sect1id_stack_add (Context *context, const char *name, const xmlChar **atrs);
void glossentry_stack_add (Context *context, const char *name, const xmlChar **atrs);
StackElement *find_first_element (Context *context, GSList *args);
ElementIndex find_first_parent (Context *context, GSList *args);
void print_footer (const char *prev, const char *home, const char *next);
void glossary_start_element (Context *context, const char *name, const xmlChar **atrs);

#endif
