/*
 * gnome-db2html3 - by John Fleck - based on Daniel Veillard's
 * xsltproc: user program for the XSL Transformation engine
 * 
 * Copyright (C) John Fleck, 2001
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
 * Foundation, Inc.,  59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 *
 */

#ifdef USE_GNOME_DB2HTML3

#include <config.h>
#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/DOCBparser.h>
#include <libxml/catalog.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

extern int xmlLoadExtDtdDefaultValue;

int
main(int argc, char **argv) {

	xsltStylesheetPtr gdb_xslreturn;
	xmlDocPtr gdb_doc, gdb_results;
	const char *params[16 + 1];
	char *gdb_docname;         /* name of doc to be parsed */
	char *gdb_pathname;        /* path to the file to be parsed */
	char *gdb_rootid;          /* id of sect, chapt, etc to be parsed */
	char *gdb_stylesheet;      /* stylesheet to be used */
	char **gdb_split_docname;  /* placeholder for file type determination */
	char *ptr;
	const char *gdb_catalogs;  /* catalog(s) name */
	gboolean has_rootid;
 
	has_rootid = FALSE; 
	gdb_doc = NULL;
	gdb_rootid = NULL;

	/* stylesheet location based on Linux Standard Base      *
	 * http://www.linuxbase.org/spec/gLSB/gLSB/sgmlr002.html */
	gdb_stylesheet = g_strconcat (PREFIXDIR, "/share/sgml/docbook/gnome-customization-0.1/gnome-customization.xsl", NULL);
	
	if (argc <= 1) {
		g_print("Usage: %s file?rootid, where file\n", argv[0]);
		g_print("is the name of the file to be parsed and rootid\n");
		g_print("is the id of the section to be displayed\n");
		return(0);
	}

	/* use libxml2 catalog routine to retrieve catalog *
	 * info to speed dtd resolution                    */
	gdb_catalogs = getenv("SGML_CATALOG_FILES");
	if (gdb_catalogs == NULL) {
		fprintf(stderr, "Variable $SGML_CATALOG_FILES not set\n");
	} else {
		xmlLoadCatalogs(gdb_catalogs);
	}

	gdb_docname = argv[1];

	/* check to see if gdb_docname has a ?sectid included */
	for (ptr = gdb_docname; *ptr; ptr++){
		if (*ptr == '?') {
			*ptr = '\000';
			if (*(ptr + 1)) {
				gdb_rootid = ptr;
				has_rootid = TRUE;
			}
		}
	}

	/* libxml housekeeping */
	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	/* parse the stylesheet */
	gdb_xslreturn  = xsltParseStylesheetFile((const xmlChar *)gdb_stylesheet);
	if (gdb_xslreturn != NULL) { 
		g_printerr ("Stylesheet %s successfully parsed\n", gdb_stylesheet);
	}

	/* check the file type by looking at name
	 * FIXME - we need to be more sophisticated about this
	 * then parse as either xml or sgml */
	gdb_split_docname = g_strsplit(gdb_docname, ".", 2);
	if (!strcmp(gdb_split_docname[1], "sgml")) {
			gdb_doc = docbParseFile(gdb_docname, "UTF-8"); 
	} else {
		(gdb_doc = xmlParseFile(gdb_docname));
	}

	if (gdb_doc == NULL) {
		g_printerr ("Document not parsed successfully. \n");
		return (0);
	}
	else {
		g_printerr ("Document %s successfully parsed\n", gdb_docname);
	}
	
	/* retrieve path component of filename passed in at
	   command line */
	gdb_pathname = g_dirname (gdb_doc->URL);

	/* set params to be passed to stylesheet */
	params[0] = "gdb_docname";
	params[1] = g_strconcat("\"", gdb_doc->URL, "\"", NULL) ;
	params[2] = "gdb_pathname";
	params[3] = g_strconcat("\"", gdb_pathname, "\"", NULL) ;
	params[4] = NULL;

	if (has_rootid) {
		params[4] = "rootid";
		params[5] = g_strconcat("\"", gdb_rootid + 1, "\"", NULL) ;
		params[6] = NULL;
	}

	gdb_results = xsltApplyStylesheet(gdb_xslreturn, gdb_doc, params);

	if (gdb_results != NULL) { 
		g_printerr ("Stylesheet successfully applied\n");
	}

	/* print out the results */
	xsltSaveResultToFile(stdout, gdb_results, gdb_xslreturn);
	
	return (1);
}

#else

int
main(int argc, char **argv) {
	return 1;
}

#endif /* USE_GNOME_DB2HTML3 */
