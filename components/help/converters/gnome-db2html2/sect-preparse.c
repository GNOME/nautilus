
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sect-elements.h"
#include "gnome.h"


static void sect_preparse_sect_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_preparse_title_characters (Context *context, const gchar *chars, gint len);
static void sect_preparse_figure_start_element (Context *context, const char *name, const xmlChar **atrs);
static void sect_preparse_set_doctype (Context *context, const char *name, const xmlChar **atrs);
static void sect_preparse_glossterm_start_element (Context *context, const char *name, const xmlChar **atrs);
static void sect_preparse_glossterm_characters (Context *context, const gchar *chars, int len);
static void sect_preparse_glossterm_end_element (Context *context, const char *name, const xmlChar **atrs);
static void sect_preparse_glossentry_start_element (Context *context, const char *name, const xmlChar **atrs);
static void sect_preparse_acronym_characters (Context *context, const gchar *chars, int len);
static void gloss_term_append (Context *context, const gchar *chars, int len);


ElementInfo sect_preparse[] = {
	{ ARTICLE, "article", (startElementSAXFunc) sect_preparse_set_doctype, NULL, NULL},
	{ BOOK, "book", (startElementSAXFunc) sect_preparse_set_doctype, NULL, NULL},
	{ SECTION, "section", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT1, "sect1", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT2, "sect2", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT3, "sect3", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT4, "sect4", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT5, "sect5", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ PARA, "para", NULL, NULL, NULL},
	{ FORMALPARA, "formalpara", NULL, NULL, NULL},
	{ BOOKINFO, "bookinfo", NULL, NULL, NULL},
	{ ARTHEADER, "artheader", NULL, NULL, NULL},
	{ ARTICLEINFO, "articleinfo", NULL, NULL, NULL},
	{ GLOSSARYINFO, "glossaryinfo", NULL, NULL, NULL},
	{ AUTHORGROUP, "authorgroup", NULL, NULL, NULL},
	{ AUTHOR, "author", NULL, NULL, NULL},
	{ FIRSTNAME, "firstname", NULL, NULL, NULL},
	{ OTHERNAME, "othername", NULL, NULL, NULL},
	{ SURNAME, "surname", NULL, NULL, NULL},
	{ AFFILIATION, "affiliation", NULL, NULL, NULL},
	{ EMAIL, "email", NULL, NULL, NULL},
	{ ORGNAME, "orgname", NULL, NULL, NULL},
	{ ADDRESS, "address", NULL, NULL, NULL},
	{ COPYRIGHT, "copyright", NULL, NULL, NULL},
	{ YEAR, "year", NULL, NULL, NULL},
	{ HOLDER, "holder", NULL, NULL, NULL},
	{ TITLE, "title", NULL, NULL, (charactersSAXFunc) sect_preparse_title_characters},
	{ SUBTITLE, "subtitle", NULL, NULL, NULL},
	{ ULINK, "ulink", NULL, NULL, NULL},
	{ XREF, "xref", NULL, NULL, NULL},
	{ FOOTNOTE, "footnote", NULL, NULL, NULL},
	{ FIGURE, "figure", (startElementSAXFunc) sect_preparse_figure_start_element, NULL, NULL},
	{ GRAPHIC, "graphic", NULL, NULL, NULL},
	{ CITETITLE, "citetitle", NULL, NULL, NULL},
	{ APPLICATION, "application", NULL, NULL, NULL},
	{ FILENAME, "filename", NULL, NULL, NULL},
	{ ITEMIZEDLIST, "itemizedlist", NULL, NULL, NULL},
	{ ORDEREDLIST, "orderedlist", NULL, NULL, NULL},
	{ VARIABLELIST, "variablelist", NULL, NULL, NULL},
	{ LISTITEM, "listitem", NULL, NULL, NULL},
	{ PROGRAMLISTING, "programlisting", NULL, NULL, NULL},
	{ SGMLTAG, "sgmltag", NULL, NULL, NULL},
	{ EMPHASIS, "emphasis", NULL, NULL, NULL},
	{ TIP, "tip", NULL, NULL, NULL},
	{ WARNING, "warning", NULL, NULL, NULL},
	{ IMPORTANT, "important", NULL, NULL, NULL},
	{ NOTE, "note", NULL, NULL, NULL},
	{ CDATA, "cdata", NULL, NULL, NULL},
	{ SCREEN, "screen", NULL, NULL, NULL},
	{ SCREENSHOT, "screenshot", NULL, NULL, NULL},
	{ SCREENINFO, "screeninfo", NULL, NULL, NULL},
	{ COMMAND, "command", NULL, NULL, NULL},
	{ REPLACEABLE, "replaceable", NULL, NULL, NULL},
	{ FUNCTION, "function", NULL, NULL, NULL},
	{ GUIBUTTON, "guibutton", NULL, NULL, NULL},
	{ GUIICON, "guiicon", NULL, NULL, NULL},
	{ GUILABEL, "guilabel", NULL, NULL, NULL},
	{ GUIMENU, "guimenu", NULL, NULL, NULL},
	{ GUIMENUITEM, "guimenuitem", NULL, NULL, NULL},
	{ HARDWARE, "hardware", NULL, NULL, NULL},
	{ KEYCAP, "keycap", NULL, NULL, NULL},
	{ KEYCODE, "keycode", NULL, NULL, NULL},
	{ KEYSYM, "keysym", NULL, NULL, NULL},
	{ LITERAL, "literal", NULL, NULL, NULL},
	{ PARAMETER, "parameter", NULL, NULL, NULL},
	{ PROMPT, "prompt", NULL, NULL, NULL},
	{ SYMBOL, "symbol", NULL, NULL, NULL},
	{ USERINPUT, "userinput", NULL, NULL, NULL},
	{ CAUTION, "caution", NULL, NULL, NULL},
	{ LEGALPARA, "legalpara", NULL, NULL, NULL},
        { FIRSTTERM, "firstterm", NULL, NULL, NULL},
        { STRUCTNAME, "structname", NULL, NULL, NULL},
        { STRUCTFIELD, "structfield", NULL, NULL, NULL},
        { FUNCSYNOPSIS, "funcsynopsis", NULL, NULL, NULL},
        { FUNCPROTOTYPE, "funcprototype", NULL, NULL, NULL},
        { FUNCDEF, "funcdef", NULL, NULL, NULL},
        { FUNCPARAMS, "funcparams", NULL, NULL, NULL},
        { PARAMDEF, "paramdef", NULL, NULL, NULL},
        { VOID, "void", NULL, NULL, NULL},
        { GUISUBMENU, "guisubmenu", NULL, NULL, NULL},
        { INTERFACE, "interface", NULL, NULL, NULL},
        { LINK, "link", NULL, NULL, NULL},
        { MENUCHOICE, "menuchoice", NULL, NULL, NULL},
        { TABLE, "table", NULL, NULL, NULL},
        { INFORMALTABLE, "informaltable", NULL, NULL, NULL},
        { ROW, "row",  NULL, NULL, NULL},
        { ENTRY, "entry", NULL, NULL, NULL},
        { THEAD, "thead", NULL, NULL, NULL},
        { TBODY, "tbody", NULL, NULL, NULL},
        { ACRONYM, "acronym", NULL, NULL, (charactersSAXFunc) sect_preparse_acronym_characters},
        { MARKUP, "markup", NULL, NULL, NULL},
        { SIMPLELIST, "simplelist", NULL, NULL, NULL},
        { MEMBER, "member", NULL, NULL, NULL},
        { MOUSEBUTTON, "mousebutton", NULL, NULL, NULL},
        { SUPERSCRIPT, "superscript", NULL, NULL, NULL},
        { SYSTEMITEM, "systemitem", NULL, NULL, NULL},
        { VARNAME, "varname", NULL, NULL, NULL},
        { BLOCKQUOTE, "blockquote", NULL, NULL, NULL},
        { QUOTE, "quote", NULL, NULL, NULL},
        { OPTION, "option", NULL, NULL, NULL},
        { ENVAR, "envar", NULL, NULL, NULL},
        { COMPUTEROUTPUT, "computeroutput", NULL, NULL, NULL},
        { INLINEGRAPHIC, "inlinegraphic", NULL, NULL, NULL},
        { LEGALNOTICE, "legalnotice", NULL, NULL, NULL},
        { QUESTION, "question", NULL, NULL, NULL},
        { ANSWER, "answer", NULL, NULL, NULL},
        { CHAPTER, "chapter", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ PREFACE, "preface", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ TERM, "term", NULL, NULL, NULL},
	{ APPENDIX, "appendix", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ DOCINFO, "docinfo", NULL, NULL, NULL},
	{ GLOSSARY, "glossary", (startElementSAXFunc) sect_preparse_set_doctype, NULL, NULL},
	{ GLOSSDIV, "glossdiv", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ GLOSSENTRY, "glossentry", (startElementSAXFunc) sect_preparse_glossentry_start_element, NULL, NULL},
	{ GLOSSTERM, "glossterm", (startElementSAXFunc) sect_preparse_glossterm_start_element, (endElementSAXFunc) sect_preparse_glossterm_end_element, (charactersSAXFunc) sect_preparse_glossterm_characters},
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
	{ QANDAENTRY, "quandaentry", NULL, NULL, NULL},
	{ QANDASET, "quandaset", NULL, NULL, NULL},
	{ BRIDGEHEAD, "bridgehead", NULL, NULL, NULL},
	{ UNDEFINED, NULL, NULL, NULL, NULL}
};

static void
sect_preparse_set_doctype (Context *context,
			   const char *name,
			   const xmlChar **atrs)
{
	if (g_strcasecmp (name, "article") == 0) {
		context->doctype = ARTICLE_DOC;
	} else if (g_strcasecmp (name, "book") == 0) {
		context->doctype = BOOK_DOC;
	} else if (g_strcasecmp (name, "glossary") == 0) {
	        context->doctype = GLOSSARY_DOC;
	}
}


static void
sect_preparse_sect_start_element (Context *context,
				  const gchar *name,
				  const xmlChar **atrs)
{
	GSList *element_list;
	StackElement *stack_el;
	gchar **atrs_ptr;

	element_list = NULL;
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (PREFACE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CHAPTER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (APPENDIX));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (NOTE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (TIP));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (WARNING));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GRAPHIC));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (CAUTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (IMPORTANT));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (GLOSSDIV));
	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);

	if (stack_el == NULL) {
		if (g_strcasecmp(name, "section") == 0) {
			return;
		}
	}


	switch (name[4]) {
	case 'a':
	        sect1id_stack_add (context, name, atrs);
		break;
	case 't':
		sect1id_stack_add (context, name, atrs);
		break;
	case 's':
		sect1id_stack_add (context, name, atrs);
		break;
	case '1':
		if (context->doctype == ARTICLE_DOC && stack_el->info->index != APPENDIX) {
			sect1id_stack_add (context, name, atrs);
		}
		break;
	case 'n':
	        sect1id_stack_add (context, name, atrs);
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
sect_preparse_title_characters (Context *context,
				const gchar *chars,
				gint len)
{
	GSList *element_list = NULL;
	StackElement *stack_el;
	gchar **atrs_ptr;

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

	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);
	if (stack_el == NULL)
		return;

	atrs_ptr = stack_el->atrs;

	while (atrs_ptr && *atrs_ptr) {
		if (!g_strcasecmp (*atrs_ptr, "id")) {
			gchar *temp;

			temp = g_strndup (chars, len);
			atrs_ptr++;
			if (g_hash_table_lookup (((SectContext *)context->data)->title_hash, *atrs_ptr) == NULL) {
				g_hash_table_insert (((SectContext *)context->data)->title_hash, g_strdup (*atrs_ptr), temp);
			} else {
				g_free (temp);
			}
			break;
		}
		atrs_ptr += 2;
	}
}

static void
sect_preparse_figure_start_element (Context *context,
				    const char *name,
				    const xmlChar **atrs)
{
	gchar **atrs_ptr;
	static gint figure_num = 0;

	
	figure_num++;
	atrs_ptr = (gchar **) atrs;
        while (atrs_ptr && *atrs_ptr) {
                if (g_strcasecmp (*atrs_ptr, "id") == 0) {
                        atrs_ptr++;
			if (context->figure_data == NULL) {
				context->figure_data = g_hash_table_new (g_str_hash, g_str_equal);
			}
			if (g_hash_table_lookup (context->figure_data, *atrs_ptr) == NULL) {
				/* The key is the 'figure id' - The data is the 'sect id' */
                        	g_hash_table_insert (context->figure_data, g_strdup (*atrs_ptr), g_strdup_printf("%d",figure_num));
			}
                        break;
                }
                atrs_ptr += 2;
        }
}

static void
sect_preparse_glossterm_characters (Context *context,
				    const gchar *chars,
				    int len)
{
	gloss_term_append (context, chars, len);
}

static void
sect_preparse_acronym_characters (Context *context,
				  const gchar *chars,
				  int len)
{
	gloss_term_append (context, chars, len);
}

static void
gloss_term_append (Context *context,
		   const gchar *chars,
		   int len)
{
        GList *temp_list;
        char *temp_glossentry;
	char *temp;
	char *new_string;
 	char *origkey;
 	char *origdata;

	if (context->in_glossterm == TRUE) {
		temp = g_strndup (chars, len);
		temp_glossentry = NULL;
		/* get most recent entry to glossentry stack */
		temp_list = g_list_first (context->glossentry_stack);

		if (temp_list != NULL) {
			temp_glossentry = g_strdup_printf ("%s", (char *)temp_list->data);
		} else { return; }
		
		/* check to see if glossary_data hash exists - if not create it */
		if (context->glossary_data == NULL) {
			context->glossary_data = g_hash_table_new (g_str_hash, g_str_equal);
		}
		/* check to see if an entry for temp_glossentry exists - if not create it */
		if (g_hash_table_lookup (context->glossary_data, temp_glossentry) == NULL) {
			/* The key is the 'glossentry id' - The data is the 'glossterm' */
			g_hash_table_insert (context->glossary_data, g_strdup (temp_glossentry), temp);
		} else {
			/* An entry already exists. Add to it. */
	 		if (g_hash_table_lookup_extended (context->glossary_data, temp_glossentry,
 					  (gpointer *)&origkey,
 					  (gpointer *)&origdata)) {
 			new_string = g_strconcat ((gchar *)origdata, temp, NULL );
 			g_free(origdata);
 			g_hash_table_remove(context->glossary_data, origkey);
 			g_hash_table_insert(context->glossary_data, origkey, new_string);
 			g_free(temp_glossentry);
			} 
		}
	}
}

static void
sect_preparse_glossentry_start_element (Context *context,
				  	const gchar *name,
				  	const xmlChar **atrs)
{
	glossentry_stack_add (context, name, atrs);
}

static void
sect_preparse_glossterm_start_element (Context *context,
				  	const gchar *name,
				  	const xmlChar **atrs)
{
	context->in_glossterm = TRUE;
	return;
}

static void
sect_preparse_glossterm_end_element (Context *context,
				  	const gchar *name,
				  	const xmlChar **atrs)
{
	context->in_glossterm = FALSE;
	return;
}
