
#include "sect-elements.h"
#include "gnome.h"


static void sect_preparse_sect_start_element (Context *context, const gchar *name, const xmlChar **atrs);
static void sect_preparse_title_characters (Context *context, const gchar *chars, gint len);

ElementInfo sect_preparse[] = {
	{ ARTICLE, "article", NULL, NULL, NULL},
	{ BOOK, "book", NULL, NULL, NULL},
	{ SECTION, "section", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT1, "sect1", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT2, "sect2", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT3, "sect3", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT4, "sect4", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ SECT5, "sect5", (startElementSAXFunc) sect_preparse_sect_start_element, NULL, NULL},
	{ PARA, "para", NULL, NULL, NULL},
	{ FORMALPARA, "formalpara", NULL, NULL, NULL},
	{ ARTHEADER, "artheader", NULL, NULL, NULL},
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

	{ UNDEFINED, NULL, NULL, NULL, NULL}
};

static void
sect_preparse_sect_start_element (Context *context,
				  const gchar *name,
				  const xmlChar **atrs)
{
	gchar **atrs_ptr;

	g_return_if_fail (strlen (name) >= 5);
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
sect_preparse_title_characters (Context *context,
				const gchar *chars,
				gint len)
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

	stack_el = find_first_element (context, element_list);

	g_slist_free (element_list);
	if (stack_el == NULL)
		return;

	atrs_ptr = stack_el->atrs;

	while (atrs_ptr && *atrs_ptr) {
		if (!strcasecmp (*atrs_ptr, "id")) {
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
