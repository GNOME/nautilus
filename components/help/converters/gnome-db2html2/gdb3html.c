#include <config.h>
#include "gdb3html.h"
#include "toc-elements.h"
#include "sect-elements.h"
#include "sect-preparse.h"
#include "gnome.h"

#if 0
#define ERROR_OUTPUT
#endif

/* Generic functions.  Used by both elements and toc_elements */
void
article_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	g_print ("<HTML>\n");
}

void
article_end_element (Context *context, const gchar *name)
{
	g_print ("</BODY>\n</HTML>\n");
}

void
ulink_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	gint url_found = FALSE;
	GSList *list;
	gchar **atrs_ptr;
	ElementIndex index;

	list = g_slist_prepend (NULL, GINT_TO_POINTER (FOOTNOTE));
	index = find_first_parent (context, list);

	atrs_ptr = (gchar **) atrs;
	while (atrs_ptr && *atrs_ptr) {
		if (!g_strcasecmp (*atrs_ptr, "url")) {
			atrs_ptr++;
			if (index == UNDEFINED) {
				g_print ("<A HREF=\"%s\">", *atrs_ptr);
			} else {
				GString *footnote;
				list = g_slist_last (context->footnotes);

				footnote = (GString *) list->data;
				g_string_append (footnote, "<A HREF=\"");
				g_string_append (footnote, *atrs_ptr);
				g_string_append (footnote, "\">");
			}
				
				
			url_found = TRUE;
			break;
		}
		atrs_ptr += 2;
	}
	if (!url_found)
		g_print ("<A>");
}


void
ulink_end_element (Context *context, const gchar *name)
{
	GSList *list;
	ElementIndex index;

	list = g_slist_prepend (NULL, GINT_TO_POINTER (FOOTNOTE));
	index = find_first_parent (context, list);

	if (index == UNDEFINED) {
		g_print ("</A>\n");
	} else {
		GString *footnote;

		list = g_slist_last (context->footnotes);

		footnote = (GString *) list->data;
		g_string_append (footnote, "</A>\n");
	}
}

void
html_em_start_element (Context *context,
		       const gchar *name,
		       const xmlChar **atrs)
{
	g_print ("<EM>");
}

void
html_em_end_element (Context *context,
		     const gchar *name)
{
	g_print ("</EM>");
}

void
html_tt_start_element (Context *context,
		       const gchar *name,
		       const xmlChar **atrs)
{
	g_print ("<TT>");
}

void
html_tt_end_element (Context *context,
		     const gchar *name)
{
	g_print ("</TT>");
}

void
artheader_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	g_print ("<HEAD>\n");
}

StackElement *
find_first_element (Context *context, GSList *args)
{
	GList *ptr;
	GSList *element_ptr;

	for (ptr = context->stack; ptr; ptr = ptr->next) {
		for (element_ptr = args; element_ptr; element_ptr = element_ptr->next) {
			if (((StackElement*) ptr->data)->info &&
			    ((StackElement*) ptr->data)->info->index == GPOINTER_TO_INT (element_ptr->data))
				return (StackElement *) ptr->data;
		}
	}
	return NULL;
}

ElementIndex
find_first_parent (Context *context, GSList *args)
{
	StackElement *stack_el;

	stack_el = find_first_element (context, args);
	if (stack_el == NULL)
		return UNDEFINED;
	else
		return stack_el->info->index;
		
}


/* helper functions */

static ElementInfo *
find_element_info (ElementInfo *elements,
		   const gchar *name)
{
	while (elements->name != NULL) {
		if (!g_strcasecmp (elements->name, name))
			return elements;
		elements++;
	}

	return NULL;
}

/* our callbacks for the xmlSAXHandler */

static xmlEntityPtr
get_entity (Context *context, const gchar *name)
{
#ifdef ERROR_OUTPUT
	g_print ("in getEntity:%s\n", name);
#endif

	return xmlGetPredefinedEntity (name);
}

static void
start_document (Context *context)
{
}

static void
end_document (Context *context)
{

}

static void
start_element(Context *context,
	      const gchar *name,
	      const xmlChar **attrs)
{
	ElementInfo *element;
	StackElement *stack_el = g_new0 (StackElement, 1);

	element = find_element_info (context->elements, name);

	stack_el->info = element;
	context->stack = g_list_prepend (context->stack, stack_el);

	if (element && element->start_element_func)
		(* element->start_element_func) (context, name, attrs);
	if (!g_strcasecmp (name, "xref")) {
		context->stack = g_list_remove_link (context->stack, context->stack);
	} else if (!g_strcasecmp (name, "void")) {
		context->stack = g_list_remove_link (context->stack, context->stack);
	}
}

static void
end_element (Context *context,
	     const gchar *name)
{
	ElementInfo *element;
	StackElement *stack_el;
	gchar **atrs_ptr;

	element = find_element_info (context->elements, name);
	stack_el = (StackElement *) context->stack->data;
	if (stack_el->info != element) {
		/* Prolly a tag we ignored */
		return;
	}
	if (element && element->end_element_func)
		(* element->end_element_func) (context, name);

	context->stack = g_list_remove_link (context->stack, context->stack);

	atrs_ptr = stack_el->atrs;
	while (atrs_ptr && *atrs_ptr) {
		g_free (*atrs_ptr);
		atrs_ptr++;
	};
	g_free (stack_el->atrs);
	g_free (stack_el);
}

static void
characters (Context *context,
	    const gchar *chars,
	    int len)
{
	ElementInfo *element;

	if (context->stack == NULL)
		return;
	element = ((StackElement *)context->stack->data)->info;

	if (element && element->characters_func)
		(* element->characters_func) (context, chars, len);
}

static void
comment (Context *context, const char *msg)
{
#ifdef ERROR_OUTPUT
	g_log("XML", G_LOG_LEVEL_MESSAGE, "%s", msg);
#endif
}

static void
warning (Context *context, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
#ifdef ERROR_OUTPUT
	g_logv("XML", G_LOG_LEVEL_WARNING, msg, args);
#endif
	va_end(args);
}

static void
error (Context *context, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
#ifdef ERROR_OUTPUT
	g_logv("XML", G_LOG_LEVEL_CRITICAL, msg, args);
#endif
	va_end(args);
}

static void
fatal_error (Context *context, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
#ifdef ERROR_OUTPUT
	g_logv("XML", G_LOG_LEVEL_ERROR, msg, args);
#endif
	va_end(args);
}

static void
cdata_block (Context *context, const xmlChar *value, int len)
{
	ElementInfo *element;
	StackElement *stack_el = g_new0 (StackElement, 1);

	element = find_element_info (context->elements, "cdata");
	stack_el->info = element;

	context->stack = g_list_prepend (context->stack, stack_el);

	if (element && element->characters_func)
		(* element->characters_func) (context, value, len);

	context->stack = g_list_remove_link (context->stack, context->stack);
}


static xmlSAXHandler parser = {
	NULL,  /* internalSubset */
	NULL, /* isStandalone */
	NULL, /* hasInternalSubset */
	NULL, /* hasExternalSubset */
	NULL, /* resolveEntity */
	(getEntitySAXFunc) get_entity, /* getEntity */
	NULL, /* entityDecl */
	NULL, /* notationDecl */
	NULL, /* attributeDecl */
	NULL, /* elementDecl */
	NULL, /* unparsedEntityDecl */
	NULL, /* setDocumentLocator */
	(startDocumentSAXFunc) start_document, /* startDocument */
	(endDocumentSAXFunc) end_document, /* endDocument */
	(startElementSAXFunc) start_element, /* startElement */
	(endElementSAXFunc) end_element, /* endElement */
	NULL, /* reference */
	(charactersSAXFunc) characters, /* characters */
	NULL, /* ignorableWhitespace */
	NULL, /* processingInstruction */
	(commentSAXFunc) comment, /* comment */
	(warningSAXFunc) warning, /* warning */
	(errorSAXFunc) error, /* error */
	(fatalErrorSAXFunc) fatal_error, /* fatalError */
	NULL, /*parameterEntity */
	(cdataBlockSAXFunc) cdata_block
};

static void
parse_file (gchar *filename, gchar *section)
{
	Context *context = g_new0 (Context, 1);

	if (section) {
		context->target_section = g_strdup (section);
		context->elements = sect_preparse;
		context->data = sect_init_data ();
		context->base_file = g_strdup (filename);

		if (xmlSAXUserParseFile (&parser, context, context->base_file) < 0) {
			g_error ("error");
		};
		context->elements = sect_elements;
		if (xmlSAXUserParseFile (&parser, context, context->base_file) < 0) {
			g_error ("error");
		};
	} else {
		context->elements = toc_elements;
		context->data = toc_init_data ();
		context->base_file = g_strdup (filename);
		if (xmlSAXUserParseFile (&parser, context, context->base_file) < 0) {
			g_error ("error");
		};
	}
}

int
main (int argc, char *argv[])
{
	gchar *section = NULL;
	gchar *ptr;
	gchar *ptr2;

	if (argc != 2) {
		/* It is '?SECTIONID' not '#SECTIONID' */
		g_print ("Usage:  gnome-db2html2 FILE[?SECTIONID]\n\n");
		return 0;
	}

	if (!strncmp (argv[1], "file://", strlen ("file://"))) {
		ptr = argv[1] + strlen ("file://");
	} else
		ptr = argv[1];

	for (ptr2 = ptr; *ptr2; ptr2++){
		if (*ptr2 == '?') {
			*ptr2 = '\000';
			if (*(ptr2 + 1))
				section = ptr2 + 1;
			break;
		}
	}
	parse_file (ptr, section);

	return 0;
}
