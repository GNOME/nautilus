
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include "sect-elements.h"
#include "toc-elements.h"

static gboolean in_printed_title = FALSE;
typedef struct _TocContext TocContext;
struct _TocContext {
	HeaderInfo *header;
};

static void toc_sect_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void toc_sect_end_element (Context *context, const gchar *name);
static void toc_artheader_end_element (Context *context, const gchar *name);
static void toc_author_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void toc_author_characters (Context *context, const gchar *chars, int len);
static void toc_copyright_characters (Context *context, const gchar *chars, int len);
static void toc_title_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void toc_title_end_element (Context *context, const gchar *name);
static void toc_title_characters (Context *context, const gchar *chars, int len);
static void toc_glossdiv_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void toc_glossdiv_end_element (Context *context, const gchar *name);
static void toc_glossentry_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void toc_glossentry_end_element (Context *context, const gchar *name);
static void toc_glossterm_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void toc_glossterm_end_element (Context *context, const gchar *name);
static void toc_tag_characters (Context *context, const gchar *chars, int len);
static void toc_releaseinfo_characters (Context *context, const gchar *chars, int len);

ElementInfo toc_elements[] = {
	{ ARTICLE, "article", (startElementSAXFunc) article_start_element, NULL, NULL},
	{ BOOK, "book", (startElementSAXFunc) book_start_element, NULL, NULL},
	{ SECTION, "section", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT1, "sect1", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT2, "sect2", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT3, "sect3", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT4, "sect4", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT5, "sect5", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ PARA, "para", NULL, NULL, NULL},
	{ FORMALPARA, "formalpara", NULL, NULL, NULL},
	{ BOOKINFO, "bookinfo", (startElementSAXFunc) artheader_start_element, (endElementSAXFunc) toc_artheader_end_element, NULL},
	{ ARTHEADER, "artheader", (startElementSAXFunc) artheader_start_element, (endElementSAXFunc) toc_artheader_end_element, NULL},
	{ ARTICLEINFO, "articleinfo", (startElementSAXFunc) artheader_start_element, (endElementSAXFunc) toc_artheader_end_element, NULL},
	{ GLOSSARYINFO, "glossaryinfo", (startElementSAXFunc) artheader_start_element, (endElementSAXFunc) toc_artheader_end_element, NULL},
	{ AUTHORGROUP, "authorgroup", NULL, NULL, NULL},
	{ AUTHOR, "author", (startElementSAXFunc) toc_author_start_element, NULL, NULL},
	{ FIRSTNAME, "firstname", NULL, NULL, (charactersSAXFunc) toc_author_characters },
	{ OTHERNAME, "othername", NULL, NULL, (charactersSAXFunc) toc_author_characters },
	{ SURNAME, "surname", NULL, NULL, (charactersSAXFunc) toc_author_characters },
	{ AFFILIATION, "affiliation", NULL, NULL, NULL},
	{ EMAIL, "email", NULL, NULL, (charactersSAXFunc) toc_author_characters },
	{ ORGNAME, "orgname", NULL, NULL, (charactersSAXFunc) toc_author_characters },
	{ ADDRESS, "address", NULL, NULL, NULL},
	{ COPYRIGHT, "copyright", NULL, NULL, NULL},
	{ YEAR, "year", NULL, NULL, (charactersSAXFunc) toc_copyright_characters},
	{ HOLDER, "holder", NULL, NULL, (charactersSAXFunc) toc_copyright_characters},
	{ TITLE, "title", (startElementSAXFunc) toc_title_start_element, (endElementSAXFunc) toc_title_end_element, (charactersSAXFunc) toc_title_characters },
	{ SUBTITLE, "subtitle", (startElementSAXFunc) toc_title_start_element, (endElementSAXFunc) toc_title_end_element, (charactersSAXFunc) toc_title_characters },
	{ ULINK, "ulink", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ XREF, "xref", NULL, NULL, NULL},
	{ FOOTNOTE, "footnote", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ FIGURE, "figure", NULL, NULL, NULL},
	{ GRAPHIC, "graphic", NULL, NULL, NULL},
	{ CITETITLE, "citetitle", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ APPLICATION, "application", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ FILENAME, "filename", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ ITEMIZEDLIST, "itemizedlist", NULL, NULL, NULL},
	{ ORDEREDLIST, "orderedlist", NULL, NULL, NULL},
	{ VARIABLELIST, "variablelist", NULL, NULL, NULL},
	{ LISTITEM, "listitem", NULL, NULL, NULL},
	{ PROGRAMLISTING, "programlisting", NULL, NULL, NULL},
	{ SGMLTAG, "sgmltag", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ EMPHASIS, "emphasis", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ TIP, "tip", NULL, NULL, NULL},
	{ WARNING, "warning", NULL, NULL, NULL},
	{ IMPORTANT, "important", NULL, NULL, NULL},
	{ NOTE, "note", NULL, NULL, NULL},
	{ CDATA, "cdata", NULL, NULL, NULL},
	{ SCREEN, "screen", NULL, NULL, NULL},
	{ SCREENSHOT, "screenshot", NULL, NULL, NULL},
	{ SCREENINFO, "screeninfo", NULL, NULL, NULL},
	{ COMMAND, "command", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ REPLACEABLE, "replaceable", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ FUNCTION, "function", NULL, NULL, (charactersSAXFunc) toc_tag_characters},
	{ GUIBUTTON, "guibutton", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ GUIICON, "guiicon", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ GUILABEL, "guilabel", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ GUIMENU, "guimenu", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ GUIMENUITEM, "guimenuitem", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ HARDWARE, "hardware", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ KEYCAP, "keycap", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ KEYCODE, "keycode", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ KEYSYM, "keysym", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ LITERAL, "literal", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ PARAMETER, "parameter", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ PROMPT, "prompt", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ SYMBOL, "symbol", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ USERINPUT, "userinput", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ CAUTION, "caution", NULL, NULL, NULL},
	{ LEGALPARA, "legalpara", NULL, NULL, NULL},
	{ FIRSTTERM, "firstterm", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ STRUCTNAME, "structname", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ STRUCTFIELD, "structfield", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ FUNCSYNOPSIS, "funcsynopsis", NULL, NULL, NULL},
	{ FUNCPROTOTYPE, "funcprototype", NULL, NULL, NULL},
	{ FUNCDEF, "funcdef", NULL, NULL, NULL},
	{ FUNCPARAMS, "funcparams", NULL, NULL, NULL},
	{ PARAMDEF, "paramdef", NULL, NULL, NULL},
	{ VOID, "void", NULL, NULL, NULL},
	{ GUISUBMENU, "guisubmenu", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ INTERFACE, "interface", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ LINK, "link", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ MENUCHOICE, "menuchoice", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ TABLE, "table", NULL, NULL, NULL},
	{ INFORMALTABLE, "informaltable", NULL, NULL, NULL},
	{ ROW, "row",  NULL, NULL, NULL},
	{ ENTRY, "entry", NULL, NULL, NULL},
	{ THEAD, "thead", NULL, NULL, NULL},
	{ TBODY, "tbody", NULL, NULL, NULL},
	{ ACRONYM, "acronym", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ MARKUP, "markup", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ SIMPLELIST, "simplelist", NULL, NULL, NULL},
	{ MEMBER, "member", NULL, NULL, NULL},
	{ MOUSEBUTTON, "mousebutton", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ SUPERSCRIPT, "superscript", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ SYSTEMITEM, "systemitem", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ VARNAME, "varname", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ BLOCKQUOTE, "blockquote", NULL, NULL, NULL},
	{ QUOTE, "quote", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ OPTION, "option", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ ENVAR, "envar", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ COMPUTEROUTPUT, "computeroutput", NULL, NULL, (charactersSAXFunc) toc_tag_characters },
	{ INLINEGRAPHIC, "inlinegraphic", NULL, NULL, NULL},
	{ LEGALNOTICE, "legalnotice", NULL, NULL, NULL},
	{ QUESTION, "question", NULL, NULL, NULL},
	{ ANSWER, "answer", NULL, NULL, NULL},
	{ CHAPTER, "chapter", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ PREFACE, "preface", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ TERM, "term", NULL, NULL, NULL},
	{ APPENDIX, "appendix", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ DOCINFO, "docinfo", (startElementSAXFunc) artheader_start_element, (endElementSAXFunc) toc_artheader_end_element, NULL},
	{ GLOSSARY, "glossary", (startElementSAXFunc) glossary_start_element, NULL, NULL},
	{ GLOSSDIV, "glossdiv", (startElementSAXFunc) toc_glossdiv_start_element, (endElementSAXFunc) toc_glossdiv_end_element, NULL},
	{ GLOSSENTRY, "glossentry", (startElementSAXFunc) toc_glossentry_start_element, (endElementSAXFunc) toc_glossentry_end_element, NULL},
	{ GLOSSTERM, "glossterm", (startElementSAXFunc) toc_glossterm_start_element, (endElementSAXFunc) toc_glossterm_end_element, (charactersSAXFunc) toc_title_characters},
	{ GLOSSSEE, "glosssee", NULL, NULL, NULL},
	{ GLOSSSEEALSO, "glossseealso", NULL, NULL, NULL},
	{ EXAMPLE, "example", NULL, NULL, NULL},
	{ VARLISTENTRY, "varlistentry", NULL, NULL, NULL},
	{ STREET, "street", NULL, NULL, NULL},
	{ CITY, "city", NULL, NULL, NULL},
	{ COUNTRY, "country", NULL, NULL, NULL},
	{ STATE, "state", NULL, NULL, NULL},
	{ POSTCODE, "postcode", NULL, NULL, NULL},
	{ LITERALLAYOUT, "literallayout", NULL, NULL, NULL},
	{ QANDAENTRY, "qandaentry", NULL, NULL, NULL, },
	{ QANDASET, "qandaset", NULL, NULL, NULL, },
	{ BRIDGEHEAD, "bridgehead", NULL, NULL, NULL, },
	{ RELEASEINFO, "releaseinfo", NULL, NULL, (charactersSAXFunc) toc_releaseinfo_characters, },
	{ UNDEFINED, NULL, NULL, NULL, NULL}
};

gpointer
toc_init_data (void)
{
	TocContext *retval = g_new0 (TocContext, 1);
	retval->header = g_new0 (HeaderInfo, 1);

	return (gpointer) retval;
}

void
toc_free_data (gpointer data)
{
	TocContext *to_free;

	to_free = (TocContext *) data;
	g_free (to_free->header);
	g_free (to_free);
}

static void
toc_sect_start_element (Context *context,
			const gchar *name,
			const xmlChar **atrs)
{
	gchar **atrs_ptr;

	if (g_strcasecmp (name, "section") == 0) {
		return;
	}

	switch (name[4]) {
	case 'a':
		sect1id_stack_add (context, name, atrs);
		context->preface++;
		context->chapter = 0;
		context->sect1 = 0;
		context->sect2 = 0;
		context->sect3 = 0;
		context->sect4 = 0;
		context->sect5 = 0;
		break;
	case 't':
		sect1id_stack_add (context, name, atrs);
		context->chapter++;
		context->sect1 = 0;
		context->sect2 = 0;
		context->sect3 = 0;
		context->sect4 = 0;
		context->sect5 = 0;
		break;
	case 'n':
		sect1id_stack_add (context, name, atrs);
		context->appendix++;
		context->chapter = 0;
		context->sect1 = 0;
		context->sect2 = 0;
		context->sect3 = 0;
		context->sect4 = 0;
		context->sect5 = 0;
		break;
	case '1':
		if (context->doctype == ARTICLE) {
			sect1id_stack_add (context, name, atrs);
		}
		context->sect1++;
		context->sect2 = 0;
		context->sect3 = 0;
		context->sect4 = 0;
		context->sect5 = 0;
		break;
	case '2':
		context->sect2++;
		context->sect3 = 0;
		context->sect4 = 0;
		context->sect5 = 0;
		break;
	case '3':
		context->sect3++;
		context->sect4 = 0;
		context->sect5 = 0;
		break;
	case '4':
		context->sect4++;
		context->sect5 = 0;
		break;
	case '5':
		context->sect4++;
		break;
	default:
		break;
	}

	atrs_ptr = (gchar **) atrs;
	while (atrs_ptr && *atrs_ptr) {
		if (!g_strcasecmp (*atrs_ptr, "id")) {
			atrs_ptr++;
			((StackElement *)context->stack->data)->atrs = g_new0 (gchar *, 3);
			((StackElement *)context->stack->data)->atrs[0] = g_strdup ("id");
			((StackElement *)context->stack->data)->atrs[1] = g_strdup (*atrs_ptr);
			break;
		}
		atrs_ptr += 2;
	}
}

static void
toc_sect_end_element (Context *context,
		      const gchar *name)
{
	if (g_strcasecmp (name, "section") == 0) {
		return;
	}

	switch (name[4]) {
	case 'a':
		/* FIXME bugzilla.gnome.org 44410: should chapter be set to zero? */
	        context->preface = 0;
		context->sect1 = 0;
		break;
	case 't':
		context->sect1 = 0;
		break;
	case '1':
		context->sect2 = 0;
		break;
	case '2':
		context->sect3 = 0;
		break;
	case '3':
		context->sect4 = 0;
		break;
	case '4':
		context->sect5 = 0;
		break;
	case 'n':
		/* FIXME bugzilla.gnome.org 44410: should chapter be set to zero? */
		context->appendix = 0;
		context->sect1 = 0;
		break;
	default:
		break;
	}
}

static gboolean artheader_printed = FALSE;

static void
toc_artheader_end_element (Context *context, const gchar *name)
{
	GSList *ptr;
	AuthorInfo *author;
	HeaderInfo *header = ((TocContext *) context->data)->header;
	if (artheader_printed == TRUE) return;

		if (header->title)
		g_print ("<TITLE>%s</TITLE>\n</HEAD>\n", header->title);
	else
		g_print ("<TITLE>%s</TITLE>\n</HEAD>\n", _("GNOME Documentation"));

	g_print ("<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#0000FF\" VLINK=\"#840084\" ALINK=\"#0000FF\">\n");
	g_print ("<TABLE><TR><TD VALIGN=\"TOP\">\n");
	g_print ("<IMG SRC=\"file:///usr/share/pixmaps/gnome-logo-icon.png\" BORDER=\"0\" ALT=\"GNOME\">\n");
	g_print ("</TD><TD VALIGN=\"BOTTOM\">\n");
	if (header->title)
		g_print ("<H1>%s</H1>\n", header->title);
	g_print ("</TD></TABLE><BR>\n");
	if (header->subtitle)
		g_print ("<H2>%s</H2>\n", header->subtitle);

	for (ptr = header->authors; ptr; ptr = ptr->next) {
		g_print ("<H3> by ");
		author = (AuthorInfo *) ptr->data;
		if (author->firstname)
			g_print ("%s ", author->firstname);
		if (author->othername)
			g_print ("%s ", author->othername);
		if (author->surname)
			g_print ("%s", author->surname);
		g_print ("</H3>\n");
		if (author->orgname)
			g_print ("%s<BR>", author->orgname);
		if (author->email)
			g_print ("<tt>&lt;%s&gt;</tt>\n", author->email);
		g_print ("<BR>");
	}
	g_print ("<P>");
	if ((header->copyright_holder) && (header->copyright_year)) {
		g_print ("<A HREF=\"gnome-help:%s?legalnotice\">%s</A> &copy; %s %s %s",
			 context->base_file, _("Copyright"), header->copyright_year, _("by"),header->copyright_holder);
	}
	if (header->releaseinfo) 
		g_print ("<P><FONT SIZE=\"-1\"<I>%s</I></FONT></P>", header->releaseinfo);
	g_print ("<HR>\n<H2>%s</H2>\n\n", _("Table of Contents"));
	g_print ("<P>\n");
	artheader_printed = TRUE;
}

static void
toc_author_start_element (Context *context,
			  const gchar *name,
			  const xmlChar **atrs)
{
	GSList *element_list = NULL;
	AuthorInfo *author;
	ElementIndex index;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (ARTHEADER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (BOOKINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (DOCINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSARYINFO));
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	switch (index) {
	case ARTHEADER:
	case BOOKINFO:
	case DOCINFO:
	case GLOSSARYINFO:
		break;
	default:
		return;
	};
	author = g_new0 (AuthorInfo, 1);
	((TocContext *) context->data)->header->authors = g_slist_prepend (((TocContext *) context->data)->header->authors, author);
}

static void
toc_author_characters (Context *context, const gchar *chars, int len)
{
	GSList *element_list = NULL;
	AuthorInfo *author;
	ElementIndex index;
	char *temp;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (ARTHEADER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (BOOKINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (DOCINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSARYINFO));
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	switch (index) {
	case ARTHEADER:
	case BOOKINFO:
	case DOCINFO:
	case GLOSSARYINFO:
		break;
	default:
		return;
	};

	author = (AuthorInfo *) ((TocContext *) context->data)->header->authors->data;
	g_return_if_fail (author != NULL);
	temp = g_strndup (chars, len);
	if (((StackElement *) context->stack->data)->info->index == FIRSTNAME)
		author->firstname = temp;
	else if (((StackElement *) context->stack->data)->info->index == OTHERNAME)
		author->othername = temp;
	else if (((StackElement *) context->stack->data)->info->index == SURNAME)
		author->surname = temp;
	else if (((StackElement *) context->stack->data)->info->index == EMAIL) {
		author->email = temp;
	} else if (((StackElement *) context->stack->data)->info->index == ORGNAME)
		author->orgname = temp;
	else
		g_free (temp);
}

static void
toc_copyright_characters (Context *context,
			  const gchar *chars,
			  int len)
{
	GSList *element_list = NULL;
	ElementIndex index;
	gchar *temp;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (COPYRIGHT));
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	temp = g_strndup (chars, len);

	/* FIXME: Memory is leaked here, especially if we have multiple
	 * copyright holders. Not a big priority though. */
	switch (index) {
	case COPYRIGHT:
		if (((StackElement *) context->stack->data)->info->index == YEAR)
			((TocContext *) context->data)->header->copyright_year = temp;
		else if (((StackElement *) context->stack->data)->info->index == HOLDER) {
			if (((TocContext *) context->data)->header->copyright_holder == NULL)
				  ((TocContext *)context->data)->header->copyright_holder = temp;
			else ((TocContext *)context->data)->header->copyright_holder = g_strconcat (((TocContext *)context->data)->header->copyright_holder, ", ", temp, NULL);
		}
		break;
	default:
		g_free (temp);
		break;
	};
}

static void
toc_title_start_element (Context *context,
			 const gchar *name,
			 const xmlChar **atrs)
{
	GSList *element_list = NULL;
	StackElement *stack_el;
	gchar **atrs_ptr;
	gboolean print_link;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (PREFACE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CHAPTER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (APPENDIX));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (NOTE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TIP));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (WARNING));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GRAPHIC));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CAUTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TABLE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (IMPORTANT));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSDIV));	
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (QANDASET));	
	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);
	if (stack_el == NULL)
		return;

	switch (stack_el->info->index) {
	case PREFACE:
	case CHAPTER:
	case SECT1:
	case SECTION:
	case APPENDIX:
	case GLOSSDIV:
		in_printed_title = TRUE;
		if (context->sect1 == 0) {
			g_print ("<DT>");
		} else if (context->sect2 == 0) {
			if (context->chapter > 0) {
				g_print ("<DT>&nbsp;&nbsp;");
			} else {
				g_print ("<DT>");
			}

		} else if (context->sect3 == 0) {
			g_print ("<DT>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
		} else {
			g_print ("<DT>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
		}

		if (context->preface > 0) {
		       if (context->sect1 == 0) g_print ("%s:<BR>", _("PREFACE"));
		       if (context->sect1 > 0) g_print ("&nbsp;&nbsp;%d", context->sect1); 
		        
		} else if (context->chapter > 0) {
			g_print ("%d", context->chapter);
			if (context->sect1 > 0) g_print (".%d", context->sect1);
									
		} else if (context->appendix > 0) {
			if (context->sect1 == 0) {
				g_print ("%s:<BR>", _("APPENDIX"));
			} else if (context->sect1 > 0) {
				g_print ("&nbsp;&nbsp;%d",context->sect1);
			}
		} else {
			if (context->sect1 > 0) g_print ("%d", context->sect1);
		}
		
		
		if (context->sect2 > 0) g_print (".%d", context->sect2);
                if (context->sect3 > 0) g_print (".%d", context->sect3);
                if (context->sect4 > 0) g_print (".%d", context->sect4);
                if (context->sect5 > 0) g_print (".%d", context->sect5);

		/* Don't print the "." if you are in the preface or appendix or glossdiv title */
		if ((stack_el->info->index != PREFACE) &&
		    (stack_el->info->index != APPENDIX) &&
		    (stack_el->info->index != GLOSSDIV)) {
		       g_print (".&nbsp;&nbsp;");
		}
		
		/* Only print the link if we are the chapter tag, or the sect1 tag
		 * (and the document is an article) or preface or appendix or glossdiv */
		print_link = (((stack_el->info->index == SECT1) && (context->doctype != BOOK_DOC)
				&& (context->appendix == 0))
				|| stack_el->info->index == CHAPTER
		                || stack_el->info->index == PREFACE
				|| stack_el->info->index == APPENDIX
				|| stack_el->info->index == GLOSSDIV);

		if (print_link) {
			g_print ("<A href=\"gnome-help:%s", context->base_file);

			atrs_ptr = (stack_el->atrs);
			while (atrs_ptr && *atrs_ptr) {
				if (!g_strcasecmp (*atrs_ptr, "id")) {
					atrs_ptr++;
					g_print ("?%s", *atrs_ptr);
					break;
				}
				atrs_ptr += 2;
			}
			g_print ("\">");
		}
		break;
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
		in_printed_title = TRUE;
		if (context->sect2 == 0) {
			g_print ("<DT>");
		} else if (context->sect3 == 0) {
			g_print ("<DT>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
		} else {
			g_print ("<DT>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
		}
		if (context->chapter > 0) {
			g_print ("%d", context->chapter);
			if (context->sect1 > 0) g_print (".%d", context->sect1);
		} else {
			if (context->sect1 > 0) g_print ("%d", context->sect1);
		}
		if (context->sect2 > 0) g_print (".%d", context->sect2);
		if (context->sect3 > 0) g_print (".%d", context->sect3);
		if (context->sect4 > 0) g_print (".%d", context->sect4);
		if (context->sect5 > 0) g_print (".%d", context->sect5);
		g_print (".&nbsp;&nbsp;");
	default:
		break;
	};

}

static void
toc_title_end_element (Context *context,
		       const gchar *name)
{
	GSList *element_list = NULL;
	ElementIndex index;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (PREFACE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CHAPTER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (APPENDIX));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (NOTE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TIP));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (WARNING));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GRAPHIC));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CAUTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TABLE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (IMPORTANT));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSDIV));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (QANDASET));

	index = find_first_parent (context, element_list);

	in_printed_title = FALSE;
	switch (index) {
	case PREFACE:
	case APPENDIX:
	case GLOSSDIV:
		g_print ("</A></DT>\n");
		break;
	case CHAPTER:
		g_print ("</A></DT>\n");
		break;
	case SECT1:
	case SECTION:
		if (context->doctype == ARTICLE_DOC) {
			g_print ("</A></DT>\n");
		} else {
			g_print ("</DT>\n");
		}
		break;
	case SECT2:
		g_print ("</DT>\n");
		break;
	case SECT3:
	case SECT4:
	case SECT5:
		g_print ("</DT>\n");
		break;
	default:
		break;
	};

	g_slist_free (element_list);
}

static void
toc_title_characters (Context *context, const gchar *chars, int len)
{
	GSList *element_list = NULL;
	ElementIndex index;
	char *temp;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (PREFACE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CHAPTER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (APPENDIX));	
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (ARTHEADER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (BOOKINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (NOTE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TIP));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (WARNING));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GRAPHIC));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CAUTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TABLE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (IMPORTANT));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (DOCINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSDIV));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSTERM));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSARYINFO));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (QANDASET));

	index = find_first_parent (context, element_list);

	temp = g_strndup (chars, len);

	switch (index) {
	case PREFACE:
	case CHAPTER:
	case SECT1:
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
	case SECTION:
	case APPENDIX:
	case GLOSSDIV:
	case GLOSSTERM:
		g_print ("%s", temp);
		g_free (temp);
		break;
	case ARTHEADER:
	case BOOKINFO:
	case DOCINFO:
	case GLOSSARYINFO:
		if (((StackElement *)context->stack->data)->info->index == TITLE)
			((TocContext *) context->data)->header->title = temp;
		else if (((StackElement *)context->stack->data)->info->index == SUBTITLE)
			((TocContext *) context->data)->header->subtitle = temp;
		break;
	default:
		g_free (temp);
		break;
	};

	g_slist_free (element_list);
}

static void
toc_glossdiv_start_element (Context *context,
			    const gchar *name,
			    const xmlChar **atrs)
{
	gchar **atrs_ptr;

	atrs_ptr = (gchar **) atrs;
	while (atrs_ptr && *atrs_ptr) {
        	if (!g_strcasecmp (*atrs_ptr, "id")) {
			atrs_ptr++;
			((StackElement *)context->stack->data)->atrs = g_new0 (gchar *, 3);
			((StackElement *)context->stack->data)->atrs[0] = g_strdup ("id");
			((StackElement *)context->stack->data)->atrs[1] = g_strdup (*atrs_ptr);
			break;
		}
		atrs_ptr += 2;
	}  
}

static void
toc_glossdiv_end_element (Context *context,
		      const gchar *name)
{
/*  FIXME: what do we do here? */
}

static void
toc_glossentry_start_element (Context *context,
			      const gchar *name,
			      const xmlChar **atrs)
{
	gchar **atrs_ptr;

	atrs_ptr = (gchar **) atrs;
	while (atrs_ptr && *atrs_ptr) {
		if (!g_strcasecmp (*atrs_ptr, "id")) {
			atrs_ptr++;
			((StackElement *)context->stack->data)->atrs = g_new0 (gchar *, 3);
			((StackElement *)context->stack->data)->atrs[0] = g_strdup ("id");
			((StackElement *)context->stack->data)->atrs[1] = g_strdup (*atrs_ptr);
			break;
		}
		atrs_ptr += 2;
	}  
}

static void
toc_glossentry_end_element (Context *context,
			    const gchar *name)
{
/*  FIXME: what do we do here? */
}

static void
toc_glossterm_start_element (Context *context,
			     const gchar *name,
			     const xmlChar **atrs)
{
	StackElement *stack_el;
	gchar **atrs_ptr;
	GSList *element_list = NULL;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSENTRY));
	stack_el = find_first_element (context, element_list);

	g_print ("<DT>");
	g_print ("&nbsp;&nbsp;<A href=\"gnome-help:%s", context->base_file);

	atrs_ptr = (stack_el->atrs);
	while (atrs_ptr && *atrs_ptr) {
		if (!g_strcasecmp (*atrs_ptr, "id")) {
			atrs_ptr++;
			g_print ("?%s", *atrs_ptr);
			break;
		}
		atrs_ptr += 2;
	}
	g_print ("\">");
	in_printed_title = TRUE;
}

static void
toc_glossterm_end_element (Context *context,
			   const gchar *name)
{
	g_print ("</A>\n");
	in_printed_title = FALSE;
}

static void
toc_tag_characters (Context *context, const gchar *chars, int len)
{
	GSList *element_list = NULL;
	ElementIndex index;
	char *temp;

	if (in_printed_title == FALSE) {
		return;
	}
	
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TITLE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSTERM));

	index = find_first_parent (context, element_list);
	temp = g_strndup (chars, len);

	switch (index) {
	case TITLE:
	case GLOSSTERM:
		temp = g_strndup (chars, len);
		sect_print (context, "%s", temp);
		g_free (temp);
		break;
	default:
		g_free (temp);
		break;
	};

	g_slist_free (element_list);

}

static void
toc_releaseinfo_characters (Context *context, const gchar *chars, int len)
{
	char *temp;
	temp = g_strndup (chars, len);
	((TocContext *) context->data)->header->releaseinfo = temp;
	
}
