
#include "sect-elements.h"

#define IS_IN_SECT(context) (((SectContext *)context->data)->state == IN_SECT)
typedef enum SectContextState {
	LOOKING_FOR_SECT,
	LOOKING_FOR_SECT_TITLE,
	IN_SECT,
	LOOKING_FOR_POST_SECT,
	DONE_WITH_SECT
} SectContextState;

typedef struct _SectContext SectContext;
struct _SectContext {
	HeaderInfo *header;
	gchar *prev;
	gchar *previd;
	SectContextState state;
	/* A list full of GStrings. */
	GList *footnotes;
};

static void sect_write_characters (Context *context, const gchar *chars, int len);
static void sect_sect_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_sect_end_element (Context *context, const gchar *name);
static void sect_para_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_para_end_element (Context *context, const gchar *name);
static void sect_para_characters (Context *context, const gchar *name, gint len);
static void sect_formalpara_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_formalpara_end_element (Context *context, const gchar *name);
static void sect_author_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_author_characters (Context *context, const gchar *chars, int len);
static void sect_copyright_characters (Context *context, const gchar *chars, int len);
static void sect_title_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_title_end_element (Context *context, const gchar *name);
static void sect_title_characters (Context *context, const gchar *chars, int len);
static void sect_ulink_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_ulink_end_element (Context *context, const gchar *name);
static void sect_footnote_start_element (Context *context, const gchar *name, const xmlChar **atrs);

ElementInfo sect_elements[] = {
	{ ARTICLE, "article", (startElementSAXFunc) article_start_element, (endElementSAXFunc) sect_article_end_element, NULL},
	{ BOOK, "book", NULL, NULL, NULL},
	{ SECTION, "section", (startElementSAXFunc) sect_sect_start_element, (endElementSAXFunc) sect_sect_end_element, NULL},
	{ SECT1, "sect1", (startElementSAXFunc) sect_sect_start_element, (endElementSAXFunc) sect_sect_end_element, NULL},
	{ SECT2, "sect2", (startElementSAXFunc) sect_sect_start_element, (endElementSAXFunc) sect_sect_end_element, NULL},
	{ SECT3, "sect3", (startElementSAXFunc) sect_sect_start_element, (endElementSAXFunc) sect_sect_end_element, NULL},
	{ SECT4, "sect4", (startElementSAXFunc) sect_sect_start_element, (endElementSAXFunc) sect_sect_end_element, NULL},
	{ SECT5, "sect5", (startElementSAXFunc) sect_sect_start_element, (endElementSAXFunc) sect_sect_end_element, NULL},
	{ PARA, "para", (startElementSAXFunc) sect_para_start_element, (endElementSAXFunc) sect_para_end_element, (charactersSAXFunc) sect_para_characters},
	{ FORMALPARA, "formalpara", (startElementSAXFunc) sect_formalpara_start_element, (endElementSAXFunc) sect_formalpara_end_element, NULL },
	{ ARTHEADER, "artheader", NULL, NULL, NULL}, //(startElementSAXFunc) artheader_start_element, (endElementSAXFunc) sect_artheader_end_element, NULL},
	{ AUTHORGROUP, "authorgroup", NULL, NULL, NULL},
	{ AUTHOR, "author", (startElementSAXFunc) sect_author_start_element, NULL, NULL},
	{ FIRSTNAME, "firstname", NULL, NULL, (charactersSAXFunc) sect_author_characters },
	{ OTHERNAME, "othername", NULL, NULL, (charactersSAXFunc) sect_author_characters },
	{ SURNAME, "surname", NULL, NULL, (charactersSAXFunc) sect_author_characters },
	{ AFFILIATION, "affiliation", NULL, NULL, NULL},
	{ EMAIL, "email", NULL, NULL, (charactersSAXFunc) sect_author_characters },
	{ ORGNAME, "orgname", NULL, NULL, (charactersSAXFunc) sect_author_characters },
	{ ADDRESS, "address", NULL, NULL, NULL},
	{ COPYRIGHT, "copyright", NULL, NULL, NULL},
	{ YEAR, "year", NULL, NULL, (charactersSAXFunc) sect_copyright_characters},
	{ HOLDER, "holder", NULL, NULL, (charactersSAXFunc) sect_copyright_characters},
	{ TITLE, "title", (startElementSAXFunc) sect_title_start_element, (endElementSAXFunc) sect_title_end_element, (charactersSAXFunc) sect_title_characters },
	{ SUBTITLE, "subtitle", (startElementSAXFunc) sect_title_start_element, (endElementSAXFunc) sect_title_end_element, (charactersSAXFunc) sect_title_characters },
	{ ULINK, "ulink", (startElementSAXFunc) sect_ulink_start_element, (endElementSAXFunc) sect_ulink_end_element, (charactersSAXFunc) sect_write_characters},
	{ XREF, "xref", NULL, NULL, NULL},
	{ FOOTNOTE, "footnote", (startElementSAXFunc) sect_footnote_start_element, NULL, NULL},
	{ FIGURE, "figure", NULL, NULL, NULL},
	{ GRAPHIC, "graphic", NULL, NULL, NULL},
	{ UNDEFINED, NULL, NULL, NULL, NULL}
};

gpointer
sect_init_data (void)
{
	SectContext *retval = g_new0 (SectContext, 1);
	retval->header = g_new0 (HeaderInfo, 1);
	retval->state = LOOKING_FOR_SECT;
	return (gpointer) retval;
}

static void
sect_write_characters (Context *context,
		      const gchar *chars,
		      int len)
{
	if (!IS_IN_SECT (context))
		return;

	write_characters (context, chars, len);
}

void
sect_article_end_element (Context *context, const gchar *name)
{
	if (context->footnotes) {
		GSList *list;
		gint i = 1;

		g_print ("<HR>");
		g_print ("<H4>Notes:</H4>");
		g_print ("<TABLE BORDER=\"0\" WIDTH=\"100%%\">\n\n");
		for (list = context->footnotes; list; list = list->next) {
			g_print ("<TR><TD ALIGN=\"LEFT\" VALIGN=\"TOP\" WIDTH=\"5%%\">\n");
			g_print ("<A HREF=\"#HEADNOTE%d\" NAME=\"FOOTNOTE%d\">[%d]</A></TD>\n", i, i, i);
			g_print ("<TD ALIGN=\"LEFT\" VALIGN=\"TOP\" WIDTH=\"95%%\">\n%s\n</TD></TR>\n", ((GString *)list->data)->str);
			i++;
		}
		g_print ("</TABLE>");
	}
	g_print ("</BODY>\n</HEAD>\n");
}

static void
sect_para_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	GSList *element_list = NULL;
	StackElement *stack_el;

	if (!IS_IN_SECT (context))
		return;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FOOTNOTE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);
	if (stack_el == NULL)
		return;

	switch (stack_el->info->index) {
	case SECT1:
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
	case SECTION:
		g_print ("<P>\n");
		break;
	case FORMALPARA:
	default:
		break;
	};
}

static void
sect_para_end_element (Context *context, const gchar *name)
{
	GSList *element_list = NULL;
	StackElement *stack_el;

	if (!IS_IN_SECT (context))
		return;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FOOTNOTE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);
	if (stack_el == NULL)
		return;

	switch (stack_el->info->index) {
	case SECT1:
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
	case SECTION:
		g_print ("</P>\n");
		break;
	case FORMALPARA:
	default:
		break;
	};
}


static void
sect_para_characters (Context *context,
		      const gchar *chars,
		      int len)
{
	if (!IS_IN_SECT (context))
		return;

	write_characters (context, chars, len);
}


static void
sect_formalpara_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	if (!IS_IN_SECT (context))
		return;

	g_print ("<P>");
}

static void
sect_formalpara_end_element (Context *context, const gchar *name)
{
	if (!IS_IN_SECT (context))
		return;

	g_print ("</P>");
}


static void
sect_sect_start_element (Context *context,
			const gchar *name,
			const xmlChar **atrs)
{
	gchar **atrs_ptr;
	SectContext *sect_context = (SectContext *)context->data;

	g_return_if_fail (strlen (name) >= 5);
	atrs_ptr = (gchar **) atrs;
	while (atrs_ptr && *atrs_ptr) {
		if (!strcasecmp (*atrs_ptr, "id")) {
			atrs_ptr++;
			((StackElement *)context->stack->data)->atrs = g_new0 (gchar *, 3);
			((StackElement *)context->stack->data)->atrs[0] = g_strdup ("id");
			((StackElement *)context->stack->data)->atrs[1] = g_strdup (*atrs_ptr);
			if (!strcmp (*atrs_ptr, context->target_section))
				sect_context->state = LOOKING_FOR_SECT_TITLE;
			break;
		}
		atrs_ptr += 2;
	}

	switch (name[4]) {
	case '1':
		if (sect_context->state != LOOKING_FOR_SECT_TITLE) {
			g_free (sect_context->prev);
			g_free (sect_context->previd);
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
		break;
		context->sect5 = 0;
	case '5':
		context->sect4++;
		break;
	default:
		break;
	}
}

static void
sect_sect_end_element (Context *context,
		      const gchar *name)
{
	gchar **atrs_ptr;

	g_return_if_fail (strlen (name) >= 5);

	switch (name[4]) {
	case '1':
		context->sect2 = 0;
	case '2':
		context->sect3 = 0;
	case '3':
		context->sect4 = 0;
	case '4':
		context->sect5 = 0;
	default:
		break;
	}
	atrs_ptr = ((StackElement *) context->stack->data)->atrs;

	while (atrs_ptr && *atrs_ptr) {
		if (!strcasecmp (*atrs_ptr, "id")) {
			atrs_ptr++;
			if (!strcmp (*atrs_ptr, context->target_section)) {
				((SectContext *)context->data)->state = LOOKING_FOR_POST_SECT;
			}
			break;
		}
		atrs_ptr += 2;
	}
}


static void
sect_author_start_element (Context *context,
			  const gchar *name,
			  const xmlChar **atrs)
{
	GSList *element_list = NULL;
	AuthorInfo *author;
	ElementIndex index;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (ARTHEADER));
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	switch (index) {
	case ARTHEADER:
		break;
	default:
		return;
	};
	author = g_new0 (AuthorInfo, 1);
	((SectContext *) context->data)->header->authors = g_slist_prepend (((SectContext *) context->data)->header->authors, author);
}

static void
sect_author_characters (Context *context, const gchar *chars, int len)
{
	GSList *element_list = NULL;
	AuthorInfo *author;
	ElementIndex index;
	char *temp;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (ARTHEADER));
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	switch (index) {
	case ARTHEADER:
		break;
	default:
		return;
	};

	author = (AuthorInfo *) ((SectContext *) context->data)->header->authors->data;
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
sect_copyright_characters (Context *context,
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

	switch (index) {
	case COPYRIGHT:
		if (((StackElement *) context->stack->data)->info->index == YEAR)
			((SectContext *)context->data)->header->copyright_year = temp;
		else if (((StackElement *) context->stack->data)->info->index == HOLDER)
			((SectContext *)context->data)->header->copyright_holder = temp;
		break;
	default:
		g_free (temp);
		break;
	};
}

static void
sect_title_start_element (Context *context,
			  const gchar *name,
			  const xmlChar **atrs)
{
	GSList *element_list = NULL;
	StackElement *stack_el;
	gchar **atrs_ptr;

	if (!IS_IN_SECT (context))
		return;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));
	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);
	if (stack_el == NULL)
		return;


	switch (stack_el->info->index) {
	case SECT1:
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
	case SECTION:
		if (context->sect2 == 0)
			g_print ("<H2>");
		else
			g_print ("<H3>");
		g_print ("<A name=\"");

		atrs_ptr = (stack_el->atrs);
		while (atrs_ptr && *atrs_ptr) {
			if (!strcasecmp (*atrs_ptr, "id")) {
				atrs_ptr++;
				g_print ("%s", *atrs_ptr);
				break;
			}
			atrs_ptr += 2;
		}
		g_print ("\">Section ");
		if (context->sect1 > 0) g_print ("%d", context->sect1);
		if (context->sect2 > 0) g_print (".%d", context->sect2);
		if (context->sect3 > 0) g_print (".%d", context->sect3);
		if (context->sect4 > 0) g_print (".%d", context->sect4);
		if (context->sect5 > 0) g_print (".%d", context->sect5);
		g_print (".&nbsp;&nbsp;</A><BR>");
		break;
	case FORMALPARA:
		g_print ("<B>");
		break;
	default:
		break;
	};

}

static void
sect_title_end_element (Context *context,
			const gchar *name)
{
	GSList *element_list = NULL;
	ElementIndex index;

	if (!IS_IN_SECT (context))
		return;

	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));

	index = find_first_parent (context, element_list);

	switch (index) {
	case SECT1:
	case SECTION:
		g_print ("</A></H2>\n");
		break;
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
		g_print ("</A></H3>\n");
		break;
	case FORMALPARA:
		g_print (".</B>");
		break;
	default:
		break;
	};

	g_slist_free (element_list);

}

static void
sect_title_characters (Context *context,
		       const gchar *chars,
		       int len)
{
	GSList *element_list = NULL;
	ElementIndex index;
	char *temp;


	if (((SectContext *)context->data)->state == LOOKING_FOR_SECT_TITLE) {
		StackElement *stack_el;

		element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
		element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
		element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
		element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
		element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
		element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));

		stack_el = find_first_element (context, element_list);

		temp = g_strndup (chars, len);

		g_print ("<TITLE>%s</TITLE>\n</HEAD>\n", temp);
		g_print ("<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#0000FF\" VLINK=\"#840084\" ALINK=\"#0000FF\">\n");
		if (stack_el == NULL) 
			g_print ("<A href=\"%s\"><font size=3>Up to Table of Contents</font></A><BR>\n",
				 context->base_file);
#if 0
		else
			g_print ("<A href=\"%s#%s\"><font size=3>Up to %s</font></A><BR>\n",
				 context->base_file,
				 sect_context->topid,
				 sect_context->top);
#endif
		g_print ("<H1>%s</H1>\n", temp);
		((SectContext *)context->data)->state = IN_SECT;
		g_free (temp);
		return;
	}

	if (!IS_IN_SECT (context))
		return;

	temp = g_strndup (chars, len);


	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT1));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT2));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT3));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT4));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECT5));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (SECTION));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (ARTHEADER));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FIGURE));
	element_list = g_slist_prepend (element_list, GINT_TO_POINTER (FORMALPARA));

	index = find_first_parent (context, element_list);

	switch (index) {
	case SECT1:
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
	case SECTION:
	case FORMALPARA:
		g_print (temp);
		g_free (temp);
		break;
	case ARTHEADER:
		if (((StackElement *)context->stack->data)->info->index == TITLE)
			((SectContext *) context->data)->header->title = temp;
		else if (((StackElement *)context->stack->data)->info->index == SUBTITLE)
			((SectContext *) context->data)->header->subtitle = temp;
		break;
	default:
		g_free (temp);
		break;
	};

	g_slist_free (element_list);
}

void
sect_ulink_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	if (!IS_IN_SECT (context))
		return;

	ulink_start_element (context, name, atrs);
}


void
sect_ulink_end_element (Context *context, const gchar *name)
{
	if (!IS_IN_SECT (context))
		return;

	ulink_end_element (context, name);
}

static void
sect_footnote_start_element (Context *context, const gchar *name, const xmlChar **atrs)
{
	GString *footnote;
	gint i;

	if (!IS_IN_SECT (context))
		return;

	footnote = g_string_new (NULL);
	context->footnotes = g_slist_append (context->footnotes, footnote);
	i = g_slist_length (context->footnotes);
	g_print ("<A NAME=\"HEADNOTE%d\" HREF=\"#FOOTNOTE%d\">[%d]</A>", i, i, i);
}

