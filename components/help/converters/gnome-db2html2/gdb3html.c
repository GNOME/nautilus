#include <config.h>

#include "gdb3html.h"
#include "toc-elements.h"
#include "sect-elements.h"
#include "sect-preparse.h"
#include "gnome.h"

#if 0
#define ERROR_OUTPUT
#endif

/* Generic Function. Used by toc_elements and sect_preparse */

void
sect1_start_element (Context *context,
		     const char *name,
		     const xmlChar **atrs)
{
	char **atrs_ptr;
	
	atrs_ptr = (char **) atrs;
	while (atrs_ptr &&  *atrs_ptr) {
		if (g_strcasecmp (*atrs_ptr, "id") == 0) {
			atrs_ptr++;
			context->sect1id_stack =
				g_list_prepend (context->sect1id_stack, g_strdup (*atrs_ptr));
			break;
		}
		atrs_ptr += 2;
	}
}


/* Generic functions.  Used by both elements and toc_elements */
void
article_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	g_print ("<HTML>\n");
}

void
article_end_element (Context *context, const gchar *name)
{
	/* This should now be covered by the print_footer function */
	/* g_print ("</BODY>\n</HTML>\n"); */
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
get_entity (Context *context, const char *name)
{
	xmlEntityPtr ret;
#ifdef ERROR_OUTPUT
	g_print ("in getEntity:%s\n", name);
#endif
	ret = getEntity (context->ParserCtxt, name);

/*	return xmlGetPredefinedEntity (name); */
	return (ret);
}

static void
start_document (Context *context)
{
	startDocument (context->ParserCtxt);
}

static void
end_document (Context *context)
{
	endDocument (context->ParserCtxt);
}

static void
start_element(Context *context,
	      const gchar *name,
	      const xmlChar **attrs)
{
	ElementInfo *element;
	StackElement *stack_el = g_new0 (StackElement, 1);
	
	startElement (context->ParserCtxt, name, attrs);

	element = find_element_info (context->elements, name);

	stack_el->info = element;
	context->stack = g_list_prepend (context->stack, stack_el);
	context->empty_element = TRUE;

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

	endElement (context->ParserCtxt, name);
	
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
gdb3html_characters (Context *context,
		     const gchar *chars,
	    	     int len)
{
	ElementInfo *element;
	
	characters (context->ParserCtxt, chars, len);

	if (context->stack == NULL)
		return;
	element = ((StackElement *)context->stack->data)->info;
	context->empty_element = FALSE;
	
	if (element && element->characters_func)
		(* element->characters_func) (context, chars, len);
}

static void
gdb3html_comment (Context *context, const char *msg)
{
#ifdef ERROR_OUTPUT
	g_log("XML", G_LOG_LEVEL_MESSAGE, "%s", msg);
#endif
}

static void
gdb3html_warning (Context *context, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
#ifdef ERROR_OUTPUT
	g_logv("XML", G_LOG_LEVEL_WARNING, msg, args);
#endif
	va_end(args);
}

static void
gdb3html_error (Context *context, const char *msg, ...)
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

	cdataBlock (context->ParserCtxt, value, len);

	element = find_element_info (context->elements, "cdata");
	stack_el->info = element;

	context->stack = g_list_prepend (context->stack, stack_el);

	if (element && element->characters_func)
		(* element->characters_func) (context, value, len);

	context->stack = g_list_remove_link (context->stack, context->stack);
}

static int
gdb3html_isStandalone (Context *context)
{
	int ret;
	
	ret = isStandalone (context->ParserCtxt);
	return (ret);
}

static int
gdb3html_hasInternalSubset (Context *context)
{
	int ret;
	
	ret = hasInternalSubset (context->ParserCtxt);
	return (ret);
}

static int
gdb3html_hasExternalSubset (Context *context)
{
	int ret;
	
	ret = hasExternalSubset (context->ParserCtxt);
	return (ret);
}
static void
gdb3html_internalSubset (Context *context, const xmlChar *name,
			 const xmlChar *ExternalID, const xmlChar *SystemID)
{
	/* This function is copied from SAX.c in libxml so we can 'silence'
	 * the warning messages */
	xmlParserCtxtPtr ctxt;
       
	ctxt = context->ParserCtxt;
	
	xmlCreateIntSubset (ctxt->myDoc, name, ExternalID, SystemID);
	if (((ExternalID != NULL) || (SystemID != NULL)) &&
	    (ctxt->validate && ctxt->wellFormed && ctxt->myDoc)) {
		xmlDtdPtr ret = NULL;
		xmlParserCtxtPtr dtdCtxt;
		xmlParserInputPtr input = NULL;
		xmlCharEncoding enc;

		dtdCtxt = xmlNewParserCtxt();
		if (dtdCtxt == NULL) {
			return;
		}

		/* Ask entity resolve to load it */
		if ((ctxt->directory != NULL) && (dtdCtxt->directory == NULL)) {
			dtdCtxt->directory = (char *) xmlStrdup (BAD_CAST ctxt->directory);
		}
		if ((dtdCtxt->sax != NULL) && (dtdCtxt->sax->resolveEntity != NULL)) {
			dtdCtxt->sax->warning = (warningSAXFunc) gdb3html_warning;
			input = dtdCtxt->sax->resolveEntity (dtdCtxt->userData, ExternalID, SystemID);
		}
		if (input == NULL) {
			xmlFreeParserCtxt (dtdCtxt);
			return;
		}

		/* Plug some encoding conversion routines */
		xmlPushInput (dtdCtxt, input);
		enc = xmlDetectCharEncoding (dtdCtxt->input->cur);
		xmlSwitchEncoding (dtdCtxt, enc);

		if (input->filename == NULL) {
			input->filename = (char *) xmlStrdup (SystemID);
		}
		input->line = 1;
		input->col = 1;
		input->base = dtdCtxt->input->cur;
		input->cur = dtdCtxt->input->cur;
		input->free = NULL;

		/* lets parse the entity knowing it's an external subset */
		xmlParseExternalSubset (dtdCtxt, ExternalID, SystemID);

		if (dtdCtxt->myDoc != NULL) {
			if (dtdCtxt->wellFormed) {
				ret = dtdCtxt->myDoc->intSubset;
				dtdCtxt->myDoc->intSubset = NULL;
			} else {
				ret = NULL;
			}
			xmlFreeDoc (dtdCtxt->myDoc);
			dtdCtxt->myDoc = NULL;
		}
		xmlFreeParserCtxt (dtdCtxt);

		ctxt->myDoc->extSubset = ret;
	}
}
		
static xmlParserInputPtr
gdb3html_resolveEntity (Context *context, const xmlChar *publicId, const xmlChar *systemId)
{
	xmlParserInputPtr ret;

	ret = resolveEntity (context->ParserCtxt, publicId, systemId);
	return ret;
}

static void 
gdb3html_entityDecl (Context *context, const xmlChar *name, int type,
	 	     const xmlChar *publicId, const xmlChar *systemId, xmlChar *content)
{
	entityDecl (context->ParserCtxt, name, type, publicId, systemId, content);

}

static void
gdb3html_attributeDecl (Context *context, const xmlChar *elem, const xmlChar *name,
              	        int type, int def, const xmlChar *defaultValue,
	      	        xmlEnumerationPtr tree)
{
    attributeDecl(context->ParserCtxt, elem, name, type, def, defaultValue, tree);
}

static void
gdb3html_elementDecl (Context *context, const xmlChar *name, int type,
	    	      xmlElementContentPtr content)
{
    elementDecl(context->ParserCtxt, name, type, content);
}

static void
gdb3html_notationDecl (Context *context, const xmlChar *name,
	     	       const xmlChar *publicId, const xmlChar *systemId)
{
    notationDecl(context->ParserCtxt, name, publicId, systemId);
}

static void
gdb3html_unparsedEntityDecl (Context *context, const xmlChar *name,
		    	     const xmlChar *publicId, const xmlChar *systemId,
		    	     const xmlChar *notationName)
{
	unparsedEntityDecl (context->ParserCtxt, name, publicId, systemId, notationName);
}

static void
gdb3html_reference (Context *context, const xmlChar *name)
{
	reference (context->ParserCtxt, name);
}

static void
gdb3html_processingInstruction (Context *context, const xmlChar *target,
				const xmlChar *data)
{
	processingInstruction (context->ParserCtxt, target, data);
}

static xmlEntityPtr
gdb3html_getParameterEntity (Context *context, const xmlChar *name)
{
	xmlEntityPtr ret;

	ret = getParameterEntity (context->ParserCtxt, name);
	return ret;
}

static xmlSAXHandler parser = {
	(internalSubsetSAXFunc) gdb3html_internalSubset,  /* internalSubset */
	(isStandaloneSAXFunc) gdb3html_isStandalone, /* isStandalone */
	(hasInternalSubsetSAXFunc) gdb3html_hasInternalSubset, /* hasInternalSubset */
	(hasExternalSubsetSAXFunc) gdb3html_hasExternalSubset, /* hasExternalSubset */
	(resolveEntitySAXFunc) gdb3html_resolveEntity, /* resolveEntity */
	(getEntitySAXFunc) get_entity, /* getEntity */
	(entityDeclSAXFunc) gdb3html_entityDecl, /* entityDecl */
	(notationDeclSAXFunc) gdb3html_notationDecl, /* notationDecl */
	(attributeDeclSAXFunc) gdb3html_attributeDecl, /* attributeDecl */
	(elementDeclSAXFunc) gdb3html_elementDecl, /* elementDecl */
	(unparsedEntityDeclSAXFunc) gdb3html_unparsedEntityDecl, /* unparsedEntityDecl */
	NULL, /* setDocumentLocator */
	(startDocumentSAXFunc) start_document, /* startDocument */
	(endDocumentSAXFunc) end_document, /* endDocument */
	(startElementSAXFunc) start_element, /* startElement */
	(endElementSAXFunc) end_element, /* endElement */
	(referenceSAXFunc) gdb3html_reference, /* reference */
	(charactersSAXFunc) gdb3html_characters, /* characters */
	NULL, /* ignorableWhitespace */
	(processingInstructionSAXFunc) gdb3html_processingInstruction, /* processingInstruction */
	(commentSAXFunc) gdb3html_comment, /* comment */
	(warningSAXFunc) gdb3html_warning, /* warning */
	(errorSAXFunc) gdb3html_error, /* error */
	(fatalErrorSAXFunc) fatal_error, /* fatalError */
	(getParameterEntitySAXFunc) gdb3html_getParameterEntity, /*parameterEntity */
	(cdataBlockSAXFunc) cdata_block
};

static xmlDocPtr
xml_parse_document (gchar *filename)
{
	/* This function is ripped from parser.c in libxml but slightly
	 * modified so as not to spew debug warnings all around */
	xmlDocPtr ret;
	xmlParserCtxtPtr ctxt;
	char *directory;

	ctxt = xmlCreateFileParserCtxt(filename);
	if (ctxt == NULL) {
		return (NULL);
	}
	ctxt->sax = NULL; /* This line specifically stops the warnings */

	if ((ctxt->directory == NULL) && (directory == NULL))
		directory = xmlParserGetDirectory (filename);
	if ((ctxt->directory == NULL) && (directory != NULL))
		ctxt->directory = (char *) xmlStrdup ((xmlChar *) directory);

	xmlParseDocument (ctxt);

	if (ctxt->wellFormed) {
		ret = ctxt->myDoc;
	} else {
		ret = NULL;
		xmlFreeDoc (ctxt->myDoc);
		ctxt->myDoc = NULL;
	}
	xmlFreeParserCtxt (ctxt);
	
	return (ret);
}

static void
print_footer (Context *context, const char *prev, const char *next, gboolean with_home)
{
	g_print ("\n<HR ALIGN=\"LEFT\" WIDTH=\"100%%\">\n");
	g_print ("<TABLE WIDTH=\"100%%\" BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\">\n");

	g_print ("<TR>\n<TD WIDTH=\"33%%\" ALIGN=\"LEFT\" VALIGN=\"TOP\">");
	if (prev == NULL) {
		g_print ("&nbsp;");
	} else {
		g_print ("<A HREF=\"help:%s?%s\">&#60;&#60;&#60; Previous</A>", context->base_file, prev);
	}

	g_print ("</TD>\n<TD WIDTH=\"34%%\" ALIGN=\"CENTER\" VALIGN=\"TOP\">");
	if (with_home == FALSE) {
		g_print ("&nbsp;");
	} else {
		g_print ("<A HREF=\"help:%s\">Home</A>", context->base_file);
	}

	g_print ("</TD>\n<TD WIDTH=\"33%%\" ALIGN=\"RIGHT\" VALIGN=\"TOP\">");
	if (next == NULL) {
		g_print ("&nbsp;");
	} else {
		g_print ("<A HREF=\"help:%s?%s\">Next &#62;&#62;&#62;</A>", context->base_file, next);
	}

	g_print ("</TD>\n</TR></TABLE>\n");
	g_print ("</BODY>\n</HTML>\n");
}

static void
sect_footer (Context *context, const char *section)
{
	GList *tmp;
	GList *tmp_next;
	GList *tmp_prev;
	char *tmp_next_data;
	char *tmp_prev_data;

	if (context->sect1id_stack == NULL) {
		return;
	}

	tmp = context->sect1id_stack;
	while (tmp != NULL) {
		if (g_strcasecmp ((char *)tmp->data, section) == 0) {
			/* Yes this below is correct because we are using a 'stack' */
			tmp_next = g_list_previous (tmp);
			tmp_prev = g_list_next (tmp);
			
			if (tmp_next == NULL) {
				tmp_next_data = NULL;
			} else {
				tmp_next_data = tmp_next->data;
			}

			if (tmp_prev == NULL) {
				tmp_prev_data = NULL;
			} else {
				tmp_prev_data = tmp_prev->data;
			}
			
			print_footer (context ,(char *) tmp_prev_data, (char *) tmp_next_data, TRUE);
			break;
		}
		tmp = g_list_next (tmp);
	}	
}

static void
parse_file (gchar *filename, gchar *section)
{
	GList *tmp;
	Context *context = g_new0 (Context, 1);
	
	context->ParserCtxt = xmlNewParserCtxt ();
	xmlInitParserCtxt (context->ParserCtxt);
	context->ParserCtxt->sax = &parser;
	context->ParserCtxt->validate = 1;
	/* FIXME bugzilla.eazel.com 2399: 
	 * Is the below correct? version needs to be set so as not to
	 * segfault in starDocument (in SAX.h) */
	context->ParserCtxt->version = xmlStrdup ("1.0"); 
	context->ParserCtxt->myDoc = xml_parse_document (filename);
	xmlSubstituteEntitiesDefault (1);
	//context->sect1id_stack = NULL;

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
		sect_footer (context, section);
	} else {
		context->elements = toc_elements;
		context->data = toc_init_data ();
		context->base_file = g_strdup (filename);
		if (xmlSAXUserParseFile (&parser, context, context->base_file) < 0) {
			g_error ("error");
		};
		tmp = g_list_last (context->sect1id_stack);
		if (tmp != NULL) {
			print_footer (context, NULL, tmp->data,FALSE);
		}
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
