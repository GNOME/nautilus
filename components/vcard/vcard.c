/* ad hoc vcard parsing routines for prototyping - replace with something better soon
 *
 * Copyright (C) 1999 Andy Hertzfeld
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#include <gnome.h>
#include <ctype.h>

#include "vcard.h"

char * vcard_full_name (gchar* vcard)
	{
	gchar full_name[2048];
	gchar *temp_str = strstr(vcard, "FN:");
	if (temp_str)
		{
		gint str_len;
		gchar *end_pos = strchr(temp_str, '\n');
		if (!end_pos)
			end_pos = temp_str + strlen(temp_str);
			
		str_len = end_pos - temp_str - 3;
		memcpy(full_name, temp_str + 3, str_len);
		full_name[str_len] = '\0';
		return g_strdup (full_name);
		}
	else
		return NULL;
	}
	
char * vcard_title (gchar* vcard)
	{
	gchar title[2048];
	gchar *temp_str = strstr(vcard, "TITLE:");
	if (temp_str)
		{
		gint str_len;
		gchar *end_pos = strchr(temp_str, '\n');
		if (!end_pos)
			end_pos = temp_str + strlen(temp_str);
			
		str_len = end_pos - temp_str - 6;
		memcpy(title, temp_str + 6, str_len);
		title[str_len] = '\0';
		return g_strdup (title);
		}
	else
		return NULL;
	}

char * vcard_logo (gchar* vcard)
	{
	gchar logo[2048];
	gchar *temp_str = strstr(vcard, "LOGO;VALUE=uri:");
	if (temp_str)
		{
		gint str_len;
		gchar *end_pos = strchr(temp_str, '\n');
		if (!end_pos)
			end_pos = temp_str + strlen(temp_str);
			
		str_len = end_pos - temp_str - 15;
		memcpy(logo, temp_str + 15, str_len);
		logo[str_len] = '\0';
		return g_strdup (logo);
		}
	else
		return NULL;
	}

char * vcard_postal (gchar* vcard)
	{
	gchar postal[2048];
	gchar *temp_str = strstr(vcard, "ADR;");
	if (temp_str)
		{
		gchar *skip_pos = strchr(temp_str, ':');
		if (skip_pos)
			{
			gint str_len;
			gchar *end_pos = strchr(skip_pos, '\n');
			if (!end_pos)
				end_pos = skip_pos + strlen(skip_pos);
					
			str_len = end_pos - skip_pos - 1;
			memcpy(postal, skip_pos + 1, str_len);
			postal[str_len] = '\0';
			return g_strdup (postal);
			}
		else
			return NULL;
		}
	else
		return NULL;
	}

char * vcard_streetaddress(gchar* vcard)
	{
	gchar streetaddress[2048];
	gchar *temp_str = strstr(vcard, "ADR;");
	if (temp_str)
		{
		gchar *skip_pos = strchr(temp_str, ':');
		if (skip_pos)
			{
			gint str_len;
			gchar *end_pos;
			gint semi_count = 0;
			
			/* to extract the street address, count up two semi-colons, then scan until the next one */
		
			while (*skip_pos && (semi_count < 2))
				{
				if (*skip_pos++ == ';')
					semi_count += 1;
				}
			
			end_pos = strchr(skip_pos, ';');
			if (!end_pos)
				end_pos = skip_pos + strlen(skip_pos);
			
			str_len = end_pos - skip_pos;
			
			memcpy(streetaddress, skip_pos, str_len);
			streetaddress[str_len] = '\0';
			return g_strdup (streetaddress);
			}
		else
			return NULL;
		}
	else
		return NULL;
	}

char * vcard_city_state_zip(gchar* vcard)
	{
	gchar city[2048];
	gchar *temp_str = strstr(vcard, "ADR;");
	if (temp_str)
		{
		gchar *skip_pos = strchr(temp_str, ':');
		if (skip_pos)
			{
			gint str_len;
			gchar *end_pos;
			gint semi_count = 0;
			
			/* to extract the city, state and zip, count up 3 semi-colons */
		
			while (*skip_pos && (semi_count < 3))
				{
				if (*skip_pos++ == ';')
					semi_count += 1;
				}
			
				end_pos = skip_pos + strlen(skip_pos);
			
				str_len = end_pos - skip_pos;
				memcpy(city, skip_pos, str_len);
				city[str_len] = '\0';
				return g_strdup (city);
			}
		else
			return NULL;
		}
	else
		return NULL;
	}

char * vcard_company(gchar* vcard)
	{
	gchar company[2048];
	gchar *temp_str = strstr(vcard, "ORG:");
	if (temp_str)
		{
			gint str_len;
			gchar *end_pos = strchr(temp_str, '\n');
			if (!end_pos)
				end_pos = temp_str + strlen(temp_str);
			
			str_len = end_pos - temp_str - 4;
			memcpy(company, temp_str + 4, str_len);
			company[str_len] = '\0';
			return g_strdup (company);
		}
	else
		return NULL;
	}

char * vcard_email(gchar* vcard)
	{
		return NULL;
	}

char * vcard_fax(gchar* vcard)
	{
		return NULL;
	}

char * vcard_web(gchar* vcard)
	{
		return NULL;
	}

/* 	this routine returns a cr-delimited list of all the phone, email and web addresses it can find */

char * vcard_address_list(gchar* vcard)
	{
	gchar *next_line, *scan_ptr;
	gchar temp_card[16384];
	
	strcpy(temp_card, vcard);
	
	/* iterate through the vcard text, extracting the fields that we're interested in and building the returned list */
	
	next_line = strtok(temp_card, "\n");   
	while (next_line != NULL)
		{
		/* skip over leading blanks, and find the tag delimiter */
		
		next_line = g_strchug(next_line);
		scan_ptr = next_line;
		
		while (isalpha (*scan_ptr))
			scan_ptr++;
		
		if ((*scan_ptr == ';') || (*scan_ptr == ':'))
			{
			gchar delimiter = *scan_ptr;
			*scan_ptr = '\0';
			
		/* see if it's one of the tags that we collect */
		 
			if (!strcmp(next_line, "TEL") || !strcmp(next_line, "EMAIL") || !strcmp(next_line, "URL"))
					{
					gchar temp_address[2048];
					
					/* it matched, s prepare the output string and append it to the result */
					
					if (delimiter == ':')
						sprintf(temp_address, "%s||%s\n", next_line, scan_ptr + 1);
					else
						{
						gchar *sub_type_ptr;
						
						scan_ptr += 1;
						sub_type_ptr = scan_ptr;
						while ((*scan_ptr != ':') && (*scan_ptr != '\0'))
								scan_ptr += 1;
						*scan_ptr = '\0';							
						sprintf(temp_address, "%s|%s|%s\n", next_line, sub_type_ptr, scan_ptr + 1);
						}
				    /*
				    strcat(address_list, temp_address);				
					*/
					}
			}					
		next_line = strtok(NULL, "\n");
		}
	
	return NULL;
	}
