/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Code to generate human-readable strings from search uris.

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Mathieu Lacage <mathieu@eazel.com>
*/

#include <config.h>
#include "nautilus-search-uri.h"

/* Must be included before other libgnome headers. */
#include <libgnome/gnome-defs.h>

#include "nautilus-lib-self-check-functions.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

static const char *strip_uri_beginning      (const char *location_uri);
static GList *     tokenize_uri             (const char *string);
static char *      get_translated_criterion (GList      *criterion);
static char *      get_nth_criterion_prefix (GList      *criterion);
static char *      parse_uri                (const char *search_uri);
static void        free_tokenized_uri       (GList      *list);

/**
 * strip_uri_beginning:
 * @location_uri: search uri.
 *
 * strips the search:[file:///...] part of the input uri.
 *
 */
static const char *
strip_uri_beginning (const char *location_uri)
{
        char **first_token;
        char *ptr;
        const char *ret_val;

        first_token = g_strsplit (location_uri, " ", 1);
        if (first_token[0] == NULL) {
                g_strfreev (first_token);                
                return NULL;
        }

        /* parse the first token from the end to the beginning. 
           to extract the search:[] part.
        */
        for (ptr = first_token[0]+strlen(first_token[0]); 
             ptr != first_token[0] && *ptr != ']'; 
             ptr--) {}

        ret_val = location_uri + (ptr - first_token[0]) + 1;

        g_strfreev (first_token);

        return ret_val;
}

/**
 * free_tokenized_uri:
 * @list: tokenized uri to free
 *
 */
static void
free_tokenized_uri (GList *list)
{
        GList *node;

        for (node = list; node != NULL; node = node->next) {
                eel_g_list_free_deep (node->data);
        }
        g_list_free (list);
}


/**
 * tokenize_uri
 * @string: string to parse
 *
 * This function tokenizes a subset of the grand medusa uri specification.
 * If it cannot, it returns NULL. CHECK FOR NULL upon return.
 *
 * Return value: a Singly linked list of singly linked lists.
 *               each of the element of the root linked list is a complete criterion.
 *               each criterin sinlgly linked list is made of the different tokens
 *               of the criterion.
 */
static GList *
tokenize_uri (const char *string) 
{
        const char *temp_string;
        char **criteria;
        GList *criterion_list;
        int i, j;

        if (string == NULL) {
                return NULL;
        }

        string = strip_uri_beginning (string);
        if (string == NULL) {
                return NULL;
        }

        /* make sure we can handle this uri */
        if (strchr (string , '(') != NULL
            || strchr (string, ')') != NULL 
            || strchr (string, '|') != NULL) {
                return NULL;
        }
        
        criterion_list = NULL;

        /* split the uri in different criteria */
        criteria = g_strsplit (string, " & ", 0);
        for (i = 0, temp_string = criteria[0]; 
             temp_string != NULL; 
             i++, temp_string = criteria[i]) {
                char **tokens;
                char *token;
                GList *token_list;

                /* split a criterion in different tokens */
                token_list = NULL;
                tokens = g_strsplit (temp_string, " ", 2);
                for (j = 0, token = tokens[0]; token != NULL; j++, token = tokens[j]) {
                        /* g_strstrip does not return a newly allocated string. */
                        token_list = g_list_prepend (token_list, g_strdup (g_strstrip (token)));
                }
                criterion_list = g_list_prepend (criterion_list, g_list_reverse (token_list));
                g_strfreev (tokens);
        }
        g_strfreev (criteria);

        return g_list_reverse (criterion_list);
}

typedef struct _value_criterion_item value_criterion_item;
typedef value_criterion_item *value_criterion_table;

typedef struct _operand_criterion_item operand_criterion_item;
typedef operand_criterion_item *operand_criterion_table;

typedef struct _field_criterion_item field_criterion_item;
typedef field_criterion_item *field_criterion_table;

/* toplevel structure each entry points to a level 2 structure */
struct _field_criterion_item {
	char *id;
        /* FIXME: This field is necessary so that
           the size of this structure is the same
           as the size of the other structures.
           see the comment in the definition of "value_criterion_item"
           to see what I mean.  Yay, evil! 
           someone should make this go away. */
        char *unused_field_for_hack_compatibility;
	operand_criterion_table items;
};
/* second level structure. if items is NULL, the entry is a leaf
   of our hierarchy. If it is not, it points to a level 3 leaf 
*/
struct _operand_criterion_item {
	char *id;
	char *translation;
	value_criterion_table items;
};
/* third level structure. leaf */
struct _value_criterion_item {
	char *id;
	char *translation;
        /* this field is there only to make the 3 structures similar enough 
           so that you can safely cast between them it is a kind of evil hack 
           but i like it. It is guaranteed to be always NULL. */
	value_criterion_table items; 
};


/* -------------------------------------------------------
   -       file name                                     -
   -------------------------------------------------------
*/

static operand_criterion_item file_name2_table [] = {
        {"contains",  
         /* Human readable description for a criterion in a search for
            files. Bracketed items are context, and are message
            strings elsewhere.  You don't have to translate the whole
            string, and only the translation for "containing '%s' will
            be used.  If you do translate the whole string, leave the
            translations of the rest of the text in brackets, so it
            will not be used.  
            "%s" here is a pattern the file name
            matched, such as "nautilus" */
         N_("[Items ]containing \"%s\" in their names"),
         NULL},
        {"starts_with",
         /* "%s" here is a pattern the file name started with, such as
            "nautilus" */
         N_("[Items ]starting with \"%s\""),
         NULL},
        {"ends_with",
         /* "%s" here is a pattern the file name ended with, such as
            "mime" */
         N_("[Items ]ending with %s"),
         NULL},
        {"does_not_contain",
         /* "%s" here is a pattern the file name did not match, such
            as "nautilus" */
         N_("[Items ]not containing \"%s\" in their names"),
         NULL},
        {"regexp_matches",
         /* "%s" is a regular expression string, for example "[abc]" */
         N_("[Items ]matching the regular expression \"%s\""),
         NULL},
        {"matches",
         /* "%s" is a file glob, for example "*.txt" */
         N_("[Items ]matching the file pattern \"%s\""),
         NULL},
        {NULL, NULL, NULL}
        
};


/* -------------------------------------------------------
   -       file type                                     -
   -------------------------------------------------------
*/
static value_criterion_item file_type_options3_table [] = {
        {"file",
         N_("[Items that are ]regular files"),
         NULL},
        {"text_file",
         N_("[Items that are ]text files"),
         NULL},
        {"application",
         N_("[Items that are ]applications"),
         NULL},
        {"directory",
         N_("[Items that are ]folders"),
         NULL},
        {"music",
         N_("[Items that are ]music"),
         NULL},
        {NULL, NULL, NULL}
};
static operand_criterion_item file_type2_table [] = {
        {"is_not",  
         /* "%s" here is a word describing a file type, for example
            "folder" */
         N_("[Items ]that are not %s"),
         file_type_options3_table},
        {"is",
         /* "%s" here is a word describing a file type, for example
            "folder" */
         N_("[Items ]that are %s"),
         file_type_options3_table},
        {NULL, NULL, NULL}
};
	

/* -------------------------------------------------------
   -       owner                                         -
   -------------------------------------------------------
*/
static operand_criterion_item owner2_table [] = {
        {"is_not",
         /* "%s" here is the name of user on a Linux machine, such as
            "root" */
         N_("[Items ]not owned by \"%s\""),
         NULL},
        {"is",
         /* "%s" here is the name of user on a Linux machine, such as
            "root" */
         N_("[Items ]owned by \"%s\""),
         NULL},
        {"has_uid",
         N_("[Items ]with owner UID \"%s\""),
         NULL},
        {"does_not_have_uid",
         N_("[Items ]with owner UID other than \"%s\""),
         NULL},
        {NULL, NULL, NULL}
};

/* -------------------------------------------------------
   -       size                                          -
   -------------------------------------------------------
*/
static operand_criterion_item size2_table [] = {
        {"larger_than",
         N_("[Items ]larger than %s bytes"),
         NULL},
        {"smaller_than",
         N_("[Items ]smaller than %s bytes"),
         NULL},
        {"is",
         N_("[Items ]of %s bytes"),
         NULL},
        {NULL, NULL, NULL}
};
	
/* -------------------------------------------------------
   -       modified  time                                -
   -------------------------------------------------------
*/
static operand_criterion_item mod_time2_table [] = {
        {"is today", 
         N_("[Items ]modified today"), 
         NULL},
        {"is yesterday",
         N_("[Items ]modified yesterday"),
         NULL},
        {"is",
         N_("[Items ]modified on %s"), NULL},
        {"is_not", 
         N_("[Items ]not modified on %s"), 
         NULL},
        {"is_before",
         N_("[Items ]modified before %s"),
         NULL},
        {"is_after",
         N_("[Items ]modified after %s"),
         NULL},
        {"is_within_a_week_of",
         N_("[Items ]modified within a week of %s"),
         NULL},
        {"is_within_a_month_of",
         N_("[Items ]modified within a month of %s"),
         NULL},
        {NULL, NULL, NULL}
};

/* -------------------------------------------------------
   -     emblems                                         -
   -------------------------------------------------------
*/

static operand_criterion_item emblem2_table [] = {
        { "include",
          /* "%s" here is the name of an Emblem */
          N_("[Items ]marked with \"%s\""),
          NULL},
        { "do_not_include",
          /* "%s" here is the name of an Emblem */
          N_("[Items ]not marked with \"%s\""),
          NULL},
        {NULL, NULL, NULL}
};


/* -------------------------------------------------------
   -       contains                                      -
   -------------------------------------------------------
*/


static operand_criterion_item contains2_table [] = {
        {"includes_all_of",
         /* "%s" here is a word or words present in the file, for
            example "nautilus" or "apple orange" */
         N_("[Items ]with all the words \"%s\""),
         NULL},
        {"includes_any_of",
         /* "%s" here is a word or words present in the file, for
            example "nautilus" or "apple orange" */
         N_("[Items ]containing one of the words \"%s\""),
         NULL},
        {"does_not_include_all_of",
         /* "%s" here is a word or words present in the file, for
            example "nautilus" or "apple orange" */
         N_("[Items ]without all the words \"%s\""),
         NULL},
        {"does_not_include_any_of",
         /* "%s" here is a word or words present in the file, for
            example "nautilus" or "apple orange" */
         N_("[Items ]without any of the words \"%s\""),
         NULL},
        {NULL, NULL, NULL},
};



/* -------------------------------------------------------
   -       main table                                    -
   ------------------------------------------------------- */
static field_criterion_item main_table[] = {
        {"file_name",
         NULL,
         file_name2_table},
        {"file_type",
         NULL,
         file_type2_table},
        {"owner",
         NULL,
         owner2_table},
        {"size",
         NULL,
         size2_table},
        {"content",
         NULL,
         contains2_table},
        {"modified",
         NULL,
         mod_time2_table},
        {"keywords",
         NULL,
         emblem2_table},
        {NULL, NULL}
};



/**
 * get_item_number:
 * @current_table: the table to parse.
 * @item: the string to search into the table.
 *
 * Small helper function which allows whoich serches for @item
 * into the @current_table.
 * it returns -1 if it could not find it.
 * Yes, I know it is wrong to use the normal function return value
 * to pass error status.  */
static int 
get_item_number (field_criterion_item *current_table, char *item) 
{
        int i;

        i = 0;
        while (strcmp (current_table[i].id, 
                       item) != 0) {
                i++;
                if (current_table[i].id == NULL) {
                        return -1;
                }
        }
        
        return i;
}

/**
 * get_translated_criterion:
 * @criterion: criterion uri to parse
 *
 * Returns a translated string for a given criterion uri.
 */
static char *
get_translated_criterion (GList *criterion)
{
        
        int item_number, value_item_number;
        operand_criterion_item *operand_table;
        value_criterion_item *value_table;
        char *ret_val;
        char *context_stripped_operand, *context_stripped_value;

        /* make sure we got a valid criterion */
        if (g_list_length (criterion) != 3) {
                return NULL;
        }


        /* get through begening of criterion structure */
        item_number = get_item_number (main_table, (char *)criterion->data);
        if (item_number == -1) {
                return NULL;
        }
        operand_table = main_table[item_number].items;
        criterion = criterion->next;

        /* get through operanddle criterion structure */
        item_number = get_item_number ((field_criterion_item *)operand_table, 
                                       (char *)criterion->data);
        if (item_number == -1) {
                return NULL;
        }
        value_table = operand_table[item_number].items;
        criterion = criterion->next;
        
        /* get through value criterion structure.
           The fun begins NOW. */
        
        if (value_table == NULL && operand_table[item_number].translation != NULL) {
                /* simplest case: if the pointer to the value criterion 
                   structure is NULL and we have a translation,
                   we output a concat of the translation and the 
                   last part of the uri.
                */
                context_stripped_operand = eel_str_remove_bracketed_text (_(operand_table[item_number].translation));
                ret_val = g_strdup_printf (context_stripped_operand,
					   (char *) criterion->data);
                g_free (context_stripped_operand);
                return ret_val;
        } else if (value_table != NULL) {
                /* get through level 3 structure */
                
                value_item_number = get_item_number ((field_criterion_item *) value_table, 
                                                     (char *) criterion->data);
                if (value_item_number == -1) {
                        return NULL;
                }

                if (operand_table[item_number].translation == NULL) {
                        /* if we had no translation in operand criterion table */
                        ret_val = eel_str_remove_bracketed_text (_(value_table[value_item_number].translation));
                } else {
                        /* if we have both some translation in level 2 and level 3 */
                        context_stripped_operand = eel_str_remove_bracketed_text (_(operand_table[item_number].translation));
                        context_stripped_value = eel_str_remove_bracketed_text (_(value_table[value_item_number].translation));
                        ret_val = g_strdup_printf (context_stripped_operand, context_stripped_value);
                        g_free (context_stripped_operand);
                        g_free (context_stripped_value);
                }
                return ret_val;
        }
        
        return g_strdup ("");
}


/**
 * get_nth_criterion_prefix:
 * @criterion: The GList whose data field points to the criterion GList.
 *
 * calculates the "," or "and" prefix for any criterion.
 *
 * return value: the translated prefix.
 */
static char *
get_nth_criterion_prefix (GList *criterion)
{
        /* if we are the last criterion, put it here. */

         /* Human readable description for a criterion in a search for
            files. Bracketed items are context, and are message
            strings elsewhere.  Translate only the words "and" here. */
        if (criterion->next == NULL) {
                
                return eel_str_remove_bracketed_text (_("[Items larger than 400K] and [without all the words \"apple orange\"]"));
        }
        /* Human readable description for a criterion in a search for
           files. Bracketed items are context, and are message
           strings elsewhere.  Translate only the words "and" here. */
        return eel_str_remove_bracketed_text (_("[Items larger than 400K], [owned by root and without all the words \"apple orange\"]"));
}

/**
 * parse_uri:
 * @search_uri: uri to translate.
 *
 * returns the translated version of the uri.
 */
static char *
parse_uri (const char *search_uri)
{
        GList *criteria, *criterion;
        char *translated_criterion, *translated_prefix;
        char *ret_val, *temp;
        
        criteria = tokenize_uri (search_uri);
        if (criteria == NULL) {
                return NULL;
        }

        /* processes the first criterion and add the necessary "whose" prefix */
        translated_criterion = get_translated_criterion ((GList *)criteria->data);
        if (translated_criterion == NULL) {
                free_tokenized_uri (criteria);
                return NULL;
        }
        /* The beginning of the description of a search that has just been
           performed.  The "%s" here is a description of a single criterion,
           which in english might be "that contain the word 'foo'" */
        ret_val = g_strdup_printf (_("Items %s"),
                                   translated_criterion);
        g_free (translated_criterion);
        
        /* processes the other criteria and add the necessary "and" prefixes */
        for (criterion = criteria->next; criterion != NULL; criterion = criterion->next) {
                translated_criterion = get_translated_criterion (criterion->data);
                if (translated_criterion == NULL) {
                        g_free (ret_val);
                        free_tokenized_uri (criteria);
                        return NULL;
                }
                translated_prefix = get_nth_criterion_prefix (criterion);
                temp = g_strconcat (ret_val, translated_prefix, 
                                    translated_criterion, NULL);
                g_free (ret_val);
                ret_val = temp;
                g_free (translated_criterion);
                g_free (translated_prefix);
        }
        
        free_tokenized_uri (criteria);        

        return ret_val;
}


/**
 * nautilus_search_uri_to_human:
 * @search_uri: search uri to translate to human langage.
 *
 * The returned string is already localized.
 */
char *
nautilus_search_uri_to_human (const char *search_uri)
{
        char *uri, *human;

        g_return_val_if_fail (search_uri != NULL, NULL);

        uri = gnome_vfs_unescape_string_for_display (search_uri);
        human = parse_uri (uri);
        if (human == NULL) {
                return uri;
        }

        g_free (uri);

        return human;
}

char *
nautilus_get_target_uri_from_search_result_name (const char *search_result_name)
{
	return gnome_vfs_unescape_string (search_result_name, NULL);
}

gboolean
nautilus_is_search_uri (const char *uri)
{
        g_return_val_if_fail (uri != NULL, FALSE);

        return eel_istr_has_prefix (uri, "search:")
                || eel_istr_has_prefix (uri, "gnome-search:");
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_search_uri (void)
{
	/* search_uri_to_human */

        /* make sure that it does not accept non-supported uris.*/
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human (""), ""); 
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("s"), "s");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human (" "), " ");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("  "), "  ");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human (" s"), " s");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human (" s "), " s ");
	EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("xxx:yyy"), "xxx:yyy");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]"), "search:[][]");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]fi"), "search:[][]fi");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name"), 
                                      "search:[][]file_name");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name cont"), 
                                      "search:[][]file_name cont");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains"), 
                                      "search:[][]file_name contains");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name c stuff"), 
                                      "search:[][]file_name c stuff");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]&"), 
                                      "search:[][]&");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]f & s"), 
                                      "search:[][]f & s");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & f"), 
                                      "search:[][]file_name contains stuff & f");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type i"), 
                                      "search:[][]file_name contains stuff & file_type i");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type is f"), 
                                      "search:[][]file_name contains stuff & file_type is f");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stu)ff & file_type is file"), 
                                      "search:[][]file_name contains stu)ff & file_type is file");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stu(ff & file_type is file"), 
                                      "search:[][]file_name contains stu(ff & file_type is file");
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stu|ff & file_type is file"), 
                                      "search:[][]file_name contains stu|ff & file_type is file");

        /* make sure all the code paths work */
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff"), 
                                      _("Items containing \"stuff\" in their names"));
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_type is file"), 
                                      _("Items that are regular files"));
        /* FIXME bugzilla.gnome.org 45088: This may be what the function calls "human", but it's bad grammar. */
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type is file"), 
                                      _("Items containing \"stuff\" in their names and that are regular files"));
        /* FIXME bugzilla.gnome.org 45088: This may be what the function calls "human", but it's bad grammar. */
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type is file"
                                                                    " & size smaller_than 2000"), 
                                      _("Items containing \"stuff\" in their names, that are regular files and "
                                        "smaller than 2000 bytes"));
        /* FIXME bugzilla.gnome.org 45088: This may be what the function calls "human", but it's bad grammar. */
        EEL_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains medusa & file_type is directory"), 
                                      _("Items containing \"medusa\" in their names and that are "
                                        "folders"));
        
        /* is_search_uri */
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri (""), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("search:"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("gnome-search:"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("xxx-search:"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("search:xxx"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("gnome-search:xxx"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("xxx-search:xxx"), FALSE);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */

