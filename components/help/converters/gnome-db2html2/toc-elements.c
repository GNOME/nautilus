
#include "toc-elements.h"


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


ElementInfo toc_elements[] = {
	{ ARTICLE, "article", (startElementSAXFunc) article_start_element, NULL, NULL},
	{ BOOK, "book", NULL, NULL, NULL},
	{ SECTION, "section", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT1, "sect1", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT2, "sect2", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT3, "sect3", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT4, "sect4", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ SECT5, "sect5", (startElementSAXFunc) toc_sect_start_element, (endElementSAXFunc) toc_sect_end_element, NULL},
	{ PARA, "para", NULL, NULL, NULL},
	{ FORMALPARA, "formalpara", NULL, NULL, NULL},
	{ ARTHEADER, "artheader", (startElementSAXFunc) artheader_start_element, (endElementSAXFunc) toc_artheader_end_element, NULL},
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
	{ ULINK, "ulink", NULL, NULL, NULL}, //(startElementSAXFunc) toc_ulink_start_element, (endElementSAXFunc) toc_ulink_end_element, (charactersSAXFunc) write_characters},
	{ XREF, "xref", NULL, NULL, NULL},
	{ FOOTNOTE, "footnote", NULL, NULL, NULL},
	{ FIGURE, "figure", NULL, NULL, NULL},
	{ GRAPHIC, "graphic", NULL, NULL, NULL},
	{ UNDEFINED, NULL, NULL, NULL, NULL}
};



gpointer
toc_init_data (void)
{
	TocContext *retval = g_new0 (TocContext, 1);
	retval->header = g_new0 (HeaderInfo, 1);

	return (gpointer) retval;
}


static void
toc_sect_start_element (Context *context,
			const gchar *name,
			const xmlChar **atrs)
{
	gchar **atrs_ptr;

	g_return_if_fail (strlen (name) >= 5);

	switch (name[4]) {
	case '1':
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
		if (!strcasecmp (*atrs_ptr, "id")) {
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
}

static void
toc_artheader_end_element (Context *context, const gchar *name)
{
	GSList *ptr;
	AuthorInfo *author;
	HeaderInfo *header = ((TocContext *) context->data)->header;

	if (header->title)
		g_print ("<TITLE>%s</TITLE>\n</HEAD>\n", header->title);
	else
		g_print ("<TITLE>GNOME Documentation</TITLE>\n</HEAD>\n");

	g_print ("<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#0000FF\" VLINK=\"#840084\" ALINK=\"#0000FF\">\n");
	g_print ("<TABLE><TR><TD VALIGN=\"TOP\">\n");
	g_print ("<IMG SRC=\"/usr/share/pixmaps/gnome-logo-icon.png\" BORDER=\"0\" ALT=\"GNOME\">\n");
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
	if ((header->copyright_holder) && (header->copyright_year))
		g_print ("Copyright &copy; %s by %s", header->copyright_year, header->copyright_holder);
	g_print ("<HR>\n<H1>Table of Contents</H1>\n\n");
	g_print ("<P>\n");
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
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	switch (index) {
	case ARTHEADER:
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
	index = find_first_parent (context, element_list);
	g_slist_free (element_list);

	switch (index) {
	case ARTHEADER:
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

	switch (index) {
	case COPYRIGHT:
		if (((StackElement *) context->stack->data)->info->index == YEAR)
			((TocContext *)context->data)->header->copyright_year = temp;
		else if (((StackElement *) context->stack->data)->info->index == HOLDER)
			((TocContext *)context->data)->header->copyright_holder = temp;
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
			g_print ("<H3>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
		if (context->sect1 > 0) g_print ("%d", context->sect1);
		if (context->sect2 > 0) g_print (".%d", context->sect2);
		if (context->sect3 > 0) g_print (".%d", context->sect3);
		if (context->sect4 > 0) g_print (".%d", context->sect4);
		if (context->sect5 > 0) g_print (".%d", context->sect5);
		g_print (".&nbsp;&nbsp;");
		g_print ("<A href=\"%s?", context->base_file);

		atrs_ptr = (stack_el->atrs);
		while (atrs_ptr && *atrs_ptr) {
			if (!strcasecmp (*atrs_ptr, "id")) {
				atrs_ptr++;
				g_print ("%s", *atrs_ptr);
				break;
			}
			atrs_ptr += 2;
		}
		g_print ("\">");
		break;
	case FORMALPARA:
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

	temp = g_strndup (chars, len);

	switch (index) {
	case SECT1:
	case SECT2:
	case SECT3:
	case SECT4:
	case SECT5:
	case SECTION:
		g_print (temp);
		g_free (temp);
		break;
	case ARTHEADER:
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
