#ifndef __SECT_ELEMENTS_H__
#define __SECT_ELEMENTS_H__

#include "gdb3html.h"

extern ElementInfo sect_elements[];

gpointer sect_init_data (void);

typedef enum SectContextState {
	LOOKING_FOR_SECT,
	LOOKING_FOR_SECT_TITLE,
	IN_SECT,
	LOOKING_FOR_POST_SECT,
	DONE_WITH_SECT,
} SectContextState;

typedef struct _SectFuncProtoInfo SectFuncProtoInfo;
struct _SectFuncProtoInfo {
	gchar *retval;
	GString *func;
	GSList *params;
};

typedef struct _SectContext SectContext;
struct _SectContext {
	HeaderInfo *header;
	FigureInfo *figure;
	gint figure_count;
	gint table_count;
	gchar *prev;
	gchar *previd;
	SectContextState state;
	GHashTable *title_hash;
	GString *legalpara;
	/* A list full of protos. */
	GSList *func_synopsis;
};

void sect_print (Context *context, gchar *format, ...);
void sect_write_characters (Context *context, const gchar *chars, int len);
void sect_article_end_element (Context *context, const gchar *name);
void sect_sect_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_sect_end_element (Context *context, const gchar *name);
void sect_para_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_para_end_element (Context *context, const gchar *name);
void sect_formalpara_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_formalpara_end_element (Context *context, const gchar *name);
void sect_author_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_author_characters (Context *context, const gchar *chars, int len);
void sect_email_characters (Context *context, const gchar *chars, int len);
void sect_copyright_characters (Context *context, const gchar *chars, int len);
void sect_title_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_title_end_element (Context *context, const gchar *name);
void sect_title_characters (Context *context, const gchar *chars, int len);
void sect_ulink_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_ulink_end_element (Context *context, const gchar *name);
void sect_xref_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_footnote_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_figure_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_figure_end_element (Context *context, const gchar *name);
void sect_graphic_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_em_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_em_end_element (Context *context, const gchar *name);
void sect_tt_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_tt_end_element (Context *context, const gchar *name);
void sect_b_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_b_end_element (Context *context, const gchar *name);
void sect_tti_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_tti_end_element (Context *context, const gchar *name);
void sect_btt_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_btt_end_element (Context *context, const gchar *name);
void sect_i_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_i_end_element (Context *context, const gchar *name);
void sect_itemizedlist_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_itemizedlist_end_element (Context *context, const gchar *name);
void sect_orderedlist_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_orderedlist_end_element (Context *context, const gchar *name);
void sect_variablelist_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_variablelist_end_element (Context *context, const gchar *name);
void sect_listitem_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_listitem_end_element (Context *context, const gchar *name);
void sect_programlisting_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_programlisting_end_element (Context *context, const gchar *name);
void sect_infobox_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_infobox_end_element (Context *context, const gchar *name);
void sect_cdata_characters (Context *context, const gchar *chars, int len);
void sect_keysym_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_keysym_end_element (Context *context, const gchar *name);
void sect_funcsynopsis_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_funcsynopsis_end_element (Context *context, const gchar *name);
void sect_funcprototype_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_funcdef_characters (Context *context, const gchar *chars, int len);
void sect_funcparams_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_funcparams_end_element (Context *context, const gchar *name);
void sect_paramdef_start_element (Context *context, const gchar *chars, int len);
void sect_parameter_characters (Context *context, const gchar *chars, int len);
void sect_void_start_element (Context *context, const gchar *chars, int len);
void sect_link_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_link_end_element (Context *context, const char *name);
void sect_menu_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_menu_end_element (Context *context, const char *name);
void sect_informaltable_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_informaltable_end_element (Context *context, const char *name);
void sect_table_with_border_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_table_without_border_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_table_end_element (Context *context, const char *name);
void sect_row_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_row_end_element (Context *context, const char *name);
void sect_entry_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_entry_end_element (Context *context, const char *name);
void sect_thead_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_thead_end_element (Context *context, const char *name);
void sect_tbody_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_tbody_end_element (Context *context, const char *name);
void sect_country_characters (Context *context, const char *chars, int len);
void sect_member_start_element (Context *context, const char *chars, const xmlChar **atrs);
void sect_member_end_element (Context *context, const char *name);
void sect_quote_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_quote_end_element (Context *context, const char *name);
void sect_sup_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_sup_end_element (Context *context, const char *name);
void sect_blockquote_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_blockquote_end_element (Context *context, const char *name);
void sect_inlinegraphic_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_legalnotice_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_legalnotice_end_element (Context *context, const char *name);
void sect_legalnotice_characters (Context *context, const char *chars, int len);
void sect_question_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_answer_start_element (Context *context, const char *name, const xmlChar **atrs);
void sect_glosssee_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_glossseealso_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_glossee_end_element (Context *context, const gchar *name);
void sect_varlistentry_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_varlistentry_end_element (Context *context, const gchar *name);
void sect_address_characters (Context *context, const char *chars, int len);
void sect_address_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_address_end_element (Context *context, const gchar *name);
void sect_street_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_city_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_country_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_literallayout_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_literallayout_end_element (Context *context, const gchar *name);
void sect_bridgehead_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_bridgehead_end_element (Context *context, const gchar *name);
void sect_term_start_element (Context *context, const gchar *name, const xmlChar **atrs);
void sect_term_end_element (Context *context, const gchar *name);
#endif
