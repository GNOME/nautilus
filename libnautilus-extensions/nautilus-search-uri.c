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
#include "nautilus-string.h"
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

static const char *       strip_uri_beginning         (const char *location_uri);
static GSList *           tokenize_uri                (const char *string);
static char *             get_translated_criterion    (const GSList *criterion);
static char *             get_nth_criterion_prefix    (GSList *criterion);
static char *             parse_uri                   (const char *search_uri);
static void               free_tokenized_uri          (GSList *list);

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
free_tokenized_uri (GSList *list)
{
        GSList *temp_list;

        for (temp_list = list; temp_list != NULL; temp_list = temp_list->next) {
                GSList *inner_list, *temp_inner_list;
                
                inner_list = (GSList *)temp_list->data;
                for (temp_inner_list = inner_list; temp_inner_list != NULL; 
                     temp_inner_list = temp_inner_list->next) {
                        g_free ((char *)temp_inner_list->data);
                }
                
                g_slist_free (inner_list);
        }
        g_slist_free (list);
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
static GSList *
tokenize_uri (const char *string) 
{
        const char *temp_string;
        char **criteria;
        GSList *criterion_list;
        int i, j;

        if (string == NULL) {
                return NULL;
        }

        criterion_list = NULL;

        string = strip_uri_beginning (string);
        if (string == NULL) {
                return NULL;
        }

        /* make sure we can handle this uri */
        if ( strchr (string , '(') != NULL
             || strchr (string, ')') != NULL 
             || strchr (string, '|') != NULL) {
                return NULL;
        }
        
        /* split the uri in different criteria */
        criteria = g_strsplit (string, " & ", 0);
        for (i = 0, temp_string = criteria[0]; 
             temp_string != NULL; 
             i++, temp_string = criteria[i]) {
                char **tokens;
                char *token;
                GSList *token_list;

                /* split a criterion in different tokens */
                token_list = NULL;
                tokens = g_strsplit (temp_string, " ", 2);
                for (j = 0, token = tokens[0]; token != NULL; j++, token = tokens[j]) {
                        /* g_strstrip does not return a newly allocated string. */
                        token_list = g_slist_append (token_list, g_strdup(g_strstrip (token)));
                }
                criterion_list = g_slist_append (criterion_list, token_list);
                g_strfreev (tokens);
        }
        g_strfreev (criteria);

        return criterion_list;
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
         /* Part of a window title for search results: a
            description of the set of files that contains a string */
         N_("containing \"%s\" in their names"),
         NULL},
        {"starts_with",
         /* Part of a window title for search results: a
            description of the set of files that start with a string */
         N_("starting with \"%s\""),
         NULL},
        {"ends_with",
         /* Part of a window title for search results: a
            description of the set of files that end with a string */
         N_("ending with %s"),
         NULL},
        {"does_not_contain",
         /* Part of a window title for search results: a
            description of the set of files that end with a string */
         N_("containing \"%s\" in their names"),
         NULL},
        {"regexp_matches",
         /* Part of a window title for search results: a
            description of the set of files that match a regular
            expression */
         N_("matching the regular expression \"%s\""),
         NULL},
        {"matches",
         /* Part of a window title for search results: a
            description of the set of files that match a glob pattern */
         N_("matching the file pattern \"%s\""),
         NULL},
        {NULL, NULL, NULL}
        
};


/* -------------------------------------------------------
   -       file type                                     -
   -------------------------------------------------------
*/
static value_criterion_item file_type_options3_table [] = {
        {"file",
         /* Part of a window title for search results: a
            description of the set of files that are not directories,
            sockets, etc. */
         N_("regular files"),
         NULL},
        {"text_file",
         /* Part of a window title for search results: a
            description of the set of files that are stored as plain
            text */
         N_("text files"),
         NULL},
        {"application",
         /* Part of a window title for search results: a
            description of the set of files that are binary files or
            applications you can run */
         N_("applications"),
         NULL},
        {"directory",
         /* Part of a window title for search results: a
            description of the set of items that are folders
            (directories) */
         N_("folders"),
         NULL},
        {"music",
         /* Part of a window title for search results: a
            description of the set of items that are music files
            (mp3s, wavs) */
         N_("music"),
         NULL},
        {NULL, NULL, NULL}
};
static operand_criterion_item file_type2_table [] = {
        {"is_not",  
         /* Part of a window title for search results: way of
            describing the fact that files are not of a certain type
            (folder, music, etc).  Context is "files that are [folder,
            music]" */
         N_("that are %s"),
         file_type_options3_table},
        {"is", 
         /* Part of a window title for search results: way of
            describing the fact that files are of a certain type
            (folder, music, etc).  Context is "files that are not
            [folder, music]" */
         N_("that are %s"),
         file_type_options3_table},
};
	

/* -------------------------------------------------------
   -       owner                                         -
   -------------------------------------------------------
*/
static operand_criterion_item owner2_table [] = {
        {"is_not",
         /* Part of a window title for search results: way of
            describing the fact that files are not owned by someone,
            Context is files not ownd by xxx */
         N_("not owned by \"%s\""),
         NULL},
        {"is",
         /* Part of a window title for search results: way of
            describing the fact that files are owned by someone.
            Context is files owned by root */
         N_("owned by \"%s\""),
         NULL},
        {"has_uid",
         /* Part of a window title for search results: way of
            describing the fact that files have a specific owner uid.
            Context is "files with owner UID xx" */
         N_("with owner UID \"%s\""),
         NULL},
        {"does_not_have_uid",
         /* Part of a window title for search results: way of
            describing the fact that files have a specific owner uid
            other than this one.  Context is "files with owner uid
            other than xx" */
         N_("with owner UID other than \"%s\""),
         NULL},
        {NULL, NULL, NULL}
};

/* -------------------------------------------------------
   -       size                                          -
   -------------------------------------------------------
*/
static operand_criterion_item size2_table [] = {
        {"larger_than",
         /* Part of a window title for search results: way of
            describing that the files are larger than a certain size.
            Context is "files larger than %s bytes" */
         N_("larger than %s bytes"),
         NULL},
        {"smaller_than",
         /* Part of a window title for search results: way of
            describing that the files are larger than a certain size.
            Context is "files smaller than %s bytes" */
         N_("smaller than %s bytes"),
         NULL},
        {"is",
         /* Part of a window title for search results: way of
            describing that the files are larger than a certain size.
            Context is "files of %s bytes" */
         N_("of %s bytes"),
         NULL},
        {NULL, NULL, NULL}
};
	
/* -------------------------------------------------------
   -       modified  time                                -
   -------------------------------------------------------
*/
static operand_criterion_item mod_time2_table [] = {
        {"is today", 
         /* Part of a window title for search results: way of
            describing that the files were modified since midnight of
            the current day.  Context is "files modified today" */
         N_("modified today"), 
         NULL},
        {"is yesterday",
         /* Part of a window title for search results: way of
            describing that the files were modified since midnight of
            the previous day.  Context is "files modified yesterday" */
         N_("modified yesterday"),
         NULL},
        {"is",
         /* Part of a window title for search results: way of
            describing that the files were modified since midnight of
            the date that will replace the %s.  Context is "files
            modified on 11/20/00" */
         N_("modified on %s"),
         NULL},
        {"is_not", 
         /* Part of a window title for search results: way of
            describing that the files were not modified since midnight
            of the date that will replace the %s.  Context is "files
            not modified on 11/20/00" */
         N_("not modified on %s"), 
         NULL},
        {"is_before",
         /* Part of a window title for search results: way of
            describing that the files were not modified since midnight
            of the date that will replace the %s. Context is "files
            modified before 11/20/00" */
         N_("modified before %s"),
         NULL},
        {"is_after",
         /* Part of a window title for search results: way of
            describing that the files were modified after midnight of
            the date field that will replace the %s.  Context is
            "files modified after 11/20/00" */
         N_("modified after %s"),
         NULL},
        {"is_within_a_week_of",
         /* Part of a window title for search results: way of
            describing that the files were modified within a week of 
            the date field that will replace the %s.  Context is
            "files within a week of 11/20/00" */
         N_("modified within a week of %s"),
         NULL},
        {"is_within_a_month_of",
         /* Part of a window title for search results: way of
            describing that the files were modified within a week of 
            the date field that will replace the %s.  Context is
            "files within a month of 11/20/00" */
         N_("modified within a month of %s"),
         NULL},
        {NULL, NULL, NULL}
};

/* -------------------------------------------------------
   -     emblems                                         -
   -------------------------------------------------------
*/

static operand_criterion_item emblem2_table [] = {
        { "include",
         /* Part of a window title for search results: way of
            describing that the files have emblems attached that include
            the one that will replace the %s.  Context is "files
            marked with Important" */
          N_("marked with \"%s\""),
          NULL},
        { "do_not_include",
         /* Part of a window title for search results: way of
            describing that the files have emblems attached that don't
            include the one that will replace the %s.  Context is
            "files not marked with Important" */
          N_("not marked with \"%s\""),
          NULL},
        {NULL, NULL, NULL}
};


/* -------------------------------------------------------
   -       contains                                      -
   -------------------------------------------------------
*/


static operand_criterion_item contains2_table [] = {
        {"includes_all_of",
         /* Part of a window title for search results: way of
            describing that the (text) files searched for contain the
            word or words that will replace the %s.  Context is "files
            with all the word "bob frank" */
         N_("with all the words \"%s\""),
         NULL},
        {"includes_any_of",
         /* Part of a window title for search results: way of
            describing that the (text) files searched for contain the
            word or one or more of the words that will replace the %s.
            Context is "files containing one of the words "bob frank" */
         N_("containing one of the words \"%s\""),
         NULL},
        {"does_not_include_all_of",
         /* Part of a window title for search results: way of
            describing that the (text) files searched for don't
            contain the word or all of the words that will replace the
            %s.  Context is "files without all the word "bob frank" */
         N_("without all the words \"%s\""),
         NULL},
        {"does_not_include_any_of",
         /* Part of a window title for search results: way of
            describing that the (text) files searched for contain none of 
            the word or words that will replace the %s.  Context is "files
            with none of the words "bob frank" */
         N_("without any of the words \"%s\""),
         NULL},
        {NULL, NULL, NULL},
};



/* -------------------------------------------------------
   -       main table                                    -
   ------------------------------------------------------- */

static field_criterion_item main_table[] = {
        {"file_name",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the file
            name attribute matched.  Context is after "files" and
            before the translation for "containing xx in the name" */
         file_name2_table},
        {"file_type",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the file
            type attribute matched.  Context is after "files" and
            before the translation for "that are music" */
         file_type2_table},
        {"owner",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the
            properties of the file's owner that the search matched.
            Context is after "files" and before the translation for
            "owned by xx" */
         owner2_table},
        {"size",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the
            properties of the file's size that the search matched.
            Context is after "files" and before the translation for
            "larger than 500 bytes" */
         size2_table},
        {"content",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the
            properties of the file's content that the search matched.
            Context is after "files" and before the translation for
            "containing all the words" */
         contains2_table},
        {"modified",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the
            properties of the file's modification date that the search
            matched.  Context is after "files" and before the
            translation for "modified today" */
         mod_time2_table},
        {"keywords",
         /* Part of a window title for search results: Optional
            preposition that precedes the clause describing the
            properties of the file's attached emblems that the search
            matched.  Context is after "files" and before the
            translation for "marked with Important" */
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
get_translated_criterion (const GSList *criterion)
{

        int item_number, value_item_number;
        operand_criterion_item *operand_table;
        value_criterion_item *value_table;
        char *ret_val;

        /* make sure we got a valid criterion */
        if (g_slist_length ((GSList *) criterion) != 3) {
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
                ret_val = g_strdup_printf (_(operand_table[item_number].translation), 
					   (char *) criterion->data);
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
                        ret_val = g_strdup (_(value_table[value_item_number].translation));
                } else {
                        /* if we have both some translation in level 2 and level 3 */
                        ret_val = g_strdup_printf (_(operand_table[item_number].translation), 
                                                   _(value_table[value_item_number].translation));
                }
                return ret_val;
        }

        return g_strdup (_("are folders"));
}


/**
 * get_nth_criterion_prefix:
 * @criterion: The GSList whose data field points to the criterion GSList.
 *
 * calculates the "," or "and" prefix for any criterion.
 *
 * return value: the translated prefix.
 */
static char *
get_nth_criterion_prefix (GSList *criterion)
{
        /* if we are the last criterion, put it here. */

        if (criterion->next == NULL) {
                return g_strdup (_(" and "));
        }
        return g_strdup (", ");
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
        GSList *criteria, *criterion;
        char *translated_criterion, *translated_prefix;
        char *ret_val, *temp;
        
        criteria = tokenize_uri (search_uri);
        if (criteria == NULL) {
                return NULL;
        }

        /* processes the first criterion and add the necessary "whose" prefix */
        translated_criterion = get_translated_criterion ((GSList *)criteria->data);
        if (translated_criterion == NULL) {
                free_tokenized_uri (criteria);
                return NULL;
        }
        ret_val = g_strdup_printf (_("Items %s"),
                                   translated_criterion);
        g_free (translated_criterion);
        
        /* processes the other criteria and add the necessary "and" prefixes */
        for (criterion = criteria->next; criterion != NULL; criterion = criterion->next) {
                translated_criterion = get_translated_criterion ((const GSList *)criterion->data);
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

        return nautilus_istr_has_prefix (uri, "search:")
                || nautilus_istr_has_prefix (uri, "gnome-search:");
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_search_uri (void)
{
	/* search_uri_to_human */

        /* make sure that it does not accept non-supported uris.*/
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human (""), ""); 
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("s"), "s");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human (" "), " ");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("  "), "  ");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human (" s"), " s");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human (" s "), " s ");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("xxx:yyy"), "xxx:yyy");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]"), "search:[][]");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]fi"), "search:[][]fi");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name"), 
                                      "search:[][]file_name");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name cont"), 
                                      "search:[][]file_name cont");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains"), 
                                      "search:[][]file_name contains");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name c stuff"), 
                                      "search:[][]file_name c stuff");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]&"), 
                                      "search:[][]&");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]f & s"), 
                                      "search:[][]f & s");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & f"), 
                                      "search:[][]file_name contains stuff & f");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type i"), 
                                      "search:[][]file_name contains stuff & file_type i");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type is f"), 
                                      "search:[][]file_name contains stuff & file_type is f");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stu)ff & file_type is file"), 
                                      "search:[][]file_name contains stu)ff & file_type is file");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stu(ff & file_type is file"), 
                                      "search:[][]file_name contains stu(ff & file_type is file");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stu|ff & file_type is file"), 
                                      "search:[][]file_name contains stu|ff & file_type is file");

        /* make sure all the code paths work */
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff"), 
                                      _("Items containing \"stuff\" in their names"));
        /* FIXME bugzilla.eazel.com 5088: This may be what the function calls "human", but it's bad grammar. */
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type is file"), 
                                      _("Items containing \"stuff\" in their names and that are regular files"));
        /* FIXME bugzilla.eazel.com 5088: This may be what the function calls "human", but it's bad grammar. */
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains stuff & file_type is file"
                                                                    " & size smaller_than 2000"), 
                                      _("Items containing \"stuff\" in their names, that are regular files and "
                                        "smaller than 2000 bytes"));
        /* FIXME bugzilla.eazel.com 5088: This may be what the function calls "human", but it's bad grammar. */
        NAUTILUS_CHECK_STRING_RESULT (nautilus_search_uri_to_human ("search:[][]file_name contains medusa & file_type is directory"), 
                                      _("Items containing \"medusa\" in their names and that are "
                                        "folders"));
        
        /* is_search_uri */
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri (""), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("search:"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("gnome-search:"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("xxx-search:"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("search:xxx"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("gnome-search:xxx"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_is_search_uri ("xxx-search:xxx"), FALSE);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
