#include <libgnomevfs/gnome-vfs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#define STRING_OR_NULL(x) (NULL == x ? "NULL" : x)

guint gl_tests_failed = 0;

static void
test_failed (const char *uri, const char *expected, const char *got)
{
	printf ("Test failed: '%s' expected '%s' got '%s'\n",
		STRING_OR_NULL(uri), STRING_OR_NULL(expected), STRING_OR_NULL(got));

	gl_tests_failed++;
}

static int
strcmp_tolerate_nulls (const char *s1, const char *s2)
{
	if (s1 == NULL && s2 == NULL) {
		return 0;
	} else if (s1 == NULL) {
		return -1;
	} else if (s2 == NULL) {
		return 1;
	} else {
		return strcmp (s1, s2);
	}
}

static void
test_uri_transform (const char *orig, const char *expected)
{
	GnomeVFSURI *uri;
	char *result;

	uri = gnome_vfs_uri_new (orig);

	if (uri) {
		result = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
		if (0 != strcmp_tolerate_nulls (expected, result)) {
			test_failed (orig, expected, result);		
		}
	} else if (expected != NULL) {
		test_failed (orig, expected, NULL);
	}
}

int
main (int argc, char **argv)
{

	gnome_vfs_init ();

	/* These test cases are bad as they assume an eazel-hacking
	 * install with the docs in /gnome.
	 * 
	 * Note that I had HTML control-center documentation
	 * and SGML nautilus documentation when running the tests below
	 */
	test_uri_transform (
		"gnome-help:control-center", 
		"file:///gnome/share/gnome/help/control-center/C/index.html");

	test_uri_transform (
		"gnome-help:/gnome/share/gnome/help/control-center/C/index.html", 
		"file:///gnome/share/gnome/help/control-center/C/index.html");

	test_uri_transform (
		"gnome-help:/gnome/share/gnome/help/control-center/C/index", 
		NULL);

	test_uri_transform (
		"ghelp:control-center", 
		"file:///gnome/share/gnome/help/control-center/C/index.html");

	test_uri_transform (
		"help:control-center", 
		"file:///gnome/share/gnome/help/control-center/C/index.html");

	test_uri_transform (
		"gnome-help:control-center#foo", 
		"file:///gnome/share/gnome/help/control-center/C/index.html#foo");

	test_uri_transform (
		"ghelp:nautilus", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml'%3Bmime-type%3Dtext%2Fhtml");

	test_uri_transform (
		"ghelp:nautilus#fragment", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml%3Ffragment'%3Bmime-type%3Dtext%2Fhtml");

	test_uri_transform (
		"ghelp:///gnome/share/gnome/help/nautilus/C/nautilus.sgml", 
		"pipe:gnome-db2html2%20'%2F%2F%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml'%3Bmime-type%3Dtext%2Fhtml");

	test_uri_transform (
		"ghelp:/gnome/share/gnome/help/nautilus/C/nautilus.sgml", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml'%3Bmime-type%3Dtext%2Fhtml");
		
	test_uri_transform (
		"ghelp:/gnome/share/gnome/help/nautilus/C/krak/index.html", 
		NULL);

	test_uri_transform (
		"ghelp:/gnome/share/gnome/help/nautilus/C/index.html", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml'%3Bmime-type%3Dtext%2Fhtml");

	test_uri_transform (
		"ghelp:/gnome/share/gnome/help/nautilus/C/index.html#fragment", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml%3Ffragment'%3Bmime-type%3Dtext%2Fhtml");

	test_uri_transform (
		"ghelp:/gnome/share/gnome/help/nautilus/C/stuff.html", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml%3Fstuff'%3Bmime-type%3Dtext%2Fhtml");

	test_uri_transform (
		"ghelp:/gnome/share/gnome/help/nautilus/C/stuff.html#fragment", 
		"pipe:gnome-db2html2%20'%2Fgnome%2Fshare%2Fgnome%2Fhelp%2Fnautilus%2FC%2Fnautilus.sgml%3Ffragment'%3Bmime-type%3Dtext%2Fhtml");

	if (gl_tests_failed == 0) {
		exit (0);
	} else {
		exit (-1);
	}

}

