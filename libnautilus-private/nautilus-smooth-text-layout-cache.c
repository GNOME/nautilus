/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-smooth-text-layout-cache.c - A GtkObject subclass for efficiently rendering smooth text.

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Harper <jsh@eazel.com>
*/

#include <config.h>
#include "nautilus-smooth-text-layout-cache.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-smooth-text-layout.h"
#include <gtk/gtkmain.h>
#include <string.h>
#include <memory.h>

typedef struct CacheIndex CacheIndex;

/* Define this to get some cache statistics printed */
#undef TEST

/* Detail member struct */
struct NautilusSmoothTextLayoutCacheDetails
{
	/* Hash table mapping from CacheIndex* -> NautilusSmoothTextLayout* */
	GHashTable *table;

	/* Linked list containing all cached items, in MRU order */
	CacheIndex *head, *tail;

	/* Current total size of the cache. */
	int total_size;

	/* Maximum size of the cache */
	int maximum_size;

#ifdef TEST
	int misses, hits;
#endif
};

/* Structure used to index cached layouts */
struct CacheIndex
{
	/* These two fields are used to chain the linked list whose
	 * header is `head' and `tail' in the details structure.
	 */
	CacheIndex *next, *previous;

	/* These are the keyed values */
	NautilusScalableFont *font;
	int font_size;
	char *text;
	int text_length;
	gboolean wrap;
	int line_spacing;
	int max_text_width;
};

/* FIXME: Make this return the actual memory size, then change maximum_size */
#define LAYOUT_SIZE(l) 1

#define DEFAULT_MAXIMUM_SIZE 200

/* GtkObjectClass methods */
static void   nautilus_smooth_text_layout_cache_initialize_class (NautilusSmoothTextLayoutCacheClass  *smooth_text_layout_cache_class);
static void   nautilus_smooth_text_layout_cache_initialize       (NautilusSmoothTextLayoutCache       *smooth_text_layout_cache);
static void   nautilus_smooth_text_layout_cache_destroy          (GtkObject                           *object);


/* Cache index handling */

/* Allocate a new index structure. TEXT is copied, FONT is ref'd. */
static CacheIndex *
cache_index_new (NautilusScalableFont *font, int font_size,
		 const char *text, int text_length,
		 gboolean wrap, int line_spacing, int max_text_width)
{
	CacheIndex *index;

	index = g_new (CacheIndex, 1);
	gtk_object_ref (GTK_OBJECT (font));
	index->font = font;
	index->font_size = font_size;
	index->text = g_memdup (text, text_length);
	index->text_length = text_length;
	index->wrap = wrap;
	index->line_spacing = line_spacing;
	index->max_text_width = max_text_width;

	return index;
}

static void
cache_index_free (CacheIndex *index)
{
	gtk_object_unref (GTK_OBJECT (index->font));
	g_free (index->text);
	g_free (index);
}

/* Compare two indices */
static gint
cache_index_equal (gconstpointer x, gconstpointer y)
{
	const CacheIndex *cx, *cy;

	cx = x;
	cy = y;

	return (cx->font == cy->font
		&& cx->font_size == cy->font_size
		&& cx->wrap == cy->wrap
		&& cx->line_spacing == cy->line_spacing
		&& cx->max_text_width == cy->max_text_width
		&& cx->text_length == cy->text_length
		&& memcmp (cx->text, cy->text, cx->text_length) == 0);
}

static inline guint
hash_string (const char *text, int length)
{
    guint value;
    int index;

    /* Use the classic K=33 hash function */
    value = 0;
    for (index = 0; index < length; index++) {
	    value = (value * 33) + *text++;
    }

    return value;
}

/* Returns a hash value for the given index */
static guint
cache_index_hash (gconstpointer x)
{
	const CacheIndex *cx;

	cx = x;

	/* Is this a good hash function? */
	return cx->font_size + hash_string (cx->text, cx->text_length);
}


/* Cache management */

/* Insert INDEX as the MRU item in CACHE */
static void
cache_insert (NautilusSmoothTextLayoutCache *cache, CacheIndex *index)
{
	index->next = cache->details->head;
	index->previous = NULL;
	if (cache->details->head != NULL) {
		cache->details->head->previous = index;
	}
	cache->details->head = index;
	if (cache->details->tail == NULL) {
		cache->details->tail = index;
	}
}

/* Unlink INDEX from the list of items in CACHE */
static void
cache_remove (NautilusSmoothTextLayoutCache *cache, CacheIndex *index)
{
	if (index->next != NULL) {
		index->next->previous = index->previous;
	}
	if (index->previous != NULL) {
		index->previous->next = index->next;
	}

	if (cache->details->head == index) {
		cache->details->head = index->next;
	}
	if (cache->details->tail == index) {
		cache->details->tail = index->previous;
	}

	index->next = index->previous = NULL;
}

/* Add the mapping INDEX->LAYOUT into CACHE. Assumes ownership of LAYOUT. */
static void
cache_enter (NautilusSmoothTextLayoutCache *cache, CacheIndex *index,
	     NautilusSmoothTextLayout *layout)
{
	g_hash_table_insert (cache->details->table, index, layout);
	cache_insert (cache, index);
	cache->details->total_size += LAYOUT_SIZE (layout);
}

/* Remove the mapping from INDEX from CACHE */
static void
cache_evict (NautilusSmoothTextLayoutCache *cache, CacheIndex *index)
{
	CacheIndex *key;
	NautilusSmoothTextLayout *layout;

	if (g_hash_table_lookup_extended (cache->details->table, index, (gpointer) &key, (gpointer) &layout)) {
		g_hash_table_remove (cache->details->table, key);
		cache_remove (cache, index);
		cache_index_free (index);
		cache->details->total_size -= LAYOUT_SIZE (layout);
		gtk_object_unref (GTK_OBJECT (layout));
	} else {
		g_warning ("index not in layout cache!");
	}
}

/* Return the layout associated with the given parameters, or null */
static NautilusSmoothTextLayout *
cache_lookup (NautilusSmoothTextLayoutCache *cache,
	      NautilusScalableFont *font, int font_size,
	      const char *text, int text_length,
	      gboolean wrap, int line_spacing, int max_text_width,
	      CacheIndex **index_ptr)
{
	CacheIndex index, *key;
	NautilusSmoothTextLayout *layout;

	index.font = font;
	index.font_size = font_size;
	index.text = (char *) text;
	index.text_length = text_length;
	index.wrap = wrap;
	index.line_spacing = line_spacing;
	index.max_text_width = max_text_width;

	if (g_hash_table_lookup_extended (cache->details->table, &index, (gpointer) &key, (gpointer) &layout)) {
		if (index_ptr != NULL) {
			*index_ptr = key;
		}
		return layout;
	} else {
		return NULL;
	}
}

/* Remove the oldest items from CACHE until it's less than its maximum size */
static inline void
cache_trim (NautilusSmoothTextLayoutCache *cache)
{
#ifdef TEST
	int old_size;
	old_size = cache->details->total_size;
#endif

	while (cache->details->total_size > cache->details->maximum_size) {
		g_assert (cache->details->tail != NULL);
		cache_evict (cache, cache->details->tail);
	}

#ifdef TEST
	if (old_size != cache->details->total_size) {
		g_message ("cache %p: evicted %d entries (%d left)", cache,
			   old_size - cache->details->total_size,
			   cache->details->total_size);
	}
#endif
}


/* Public entrypoints */

/**
 * nautilus_smooth_text_layout_cache_render:
 * @cache: The layout cache being queried
 * @text: The string to be rendered
 * @text_length: The number of bytes to draw
 * @font: The desired smooth font.
 * @font_size: The pixel size of the font
 * @wrap:
 * @line_spacing:
 * @max_text_width: Requested parameters of the Layout
 *
 * Returns a NautilusSmoothTextLayout. It should be unref'd when
 * finished with; none of the fields may be modified by the caller.
 *
 */
NautilusSmoothTextLayout *
nautilus_smooth_text_layout_cache_render (NautilusSmoothTextLayoutCache *cache,
					  const char *text, int text_length,
					  NautilusScalableFont *font, int font_size,
					  gboolean wrap, int line_spacing, int max_text_width)
{
	NautilusSmoothTextLayout *layout;
	CacheIndex *index;

	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT_CACHE (cache), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (font != NULL, NULL);

	layout = cache_lookup (cache,
			       font, font_size,
			       text, text_length,
			       wrap, line_spacing, max_text_width,
			       &index);

	if (layout != NULL) {
		if (cache->details->head != index) {
			cache_remove (cache, index);
			cache_insert (cache, index);
		}
#ifdef TEST
		cache->details->hits++;
#endif
	} else {
		layout = nautilus_smooth_text_layout_new (text, text_length,
							  font, font_size,
							  wrap);
		if (layout == NULL) {
			return NULL;
		}

		nautilus_smooth_text_layout_set_line_spacing (layout, line_spacing);
		nautilus_smooth_text_layout_set_line_wrap_width (layout, max_text_width);
		cache_trim (cache);
		
		index = cache_index_new (font, font_size,
					 text, text_length,
					 wrap, line_spacing, max_text_width);
		cache_enter (cache, index, layout);

#ifdef TEST
		cache->details->misses++;
#endif
	}

	gtk_object_ref (GTK_OBJECT (layout));
	return layout;
}


/* GtkObject stuff */

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSmoothTextLayoutCache, nautilus_smooth_text_layout_cache, GTK_TYPE_OBJECT)

NautilusSmoothTextLayoutCache *
nautilus_smooth_text_layout_cache_new (void)
{
	NautilusSmoothTextLayoutCache *cache;

	cache = NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE (gtk_object_new (nautilus_smooth_text_layout_cache_get_type (), NULL));
	gtk_object_ref (GTK_OBJECT (cache));
	gtk_object_sink (GTK_OBJECT (cache));

	cache->details->maximum_size = DEFAULT_MAXIMUM_SIZE;

	return cache;
}

static void
nautilus_smooth_text_layout_cache_initialize_class (NautilusSmoothTextLayoutCacheClass *smooth_text_layout_cache_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (smooth_text_layout_cache_class);
	
	/* GtkObjectClass */
	object_class->destroy = nautilus_smooth_text_layout_cache_destroy;
}

static void
nautilus_smooth_text_layout_cache_initialize (NautilusSmoothTextLayoutCache *smooth_text_layout_cache)
{
	smooth_text_layout_cache->details = g_new0 (NautilusSmoothTextLayoutCacheDetails, 1);
	smooth_text_layout_cache->details->table = g_hash_table_new (cache_index_hash, cache_index_equal);
}

static void
free_one_cache_entry (gpointer key, gpointer value, gpointer data)
{
	CacheIndex *index;
	NautilusSmoothTextLayout *layout;

	index = key;
	layout = value;

	cache_index_free (key);
	gtk_object_unref (GTK_OBJECT (layout));
}

/* GtkObjectClass methods */
static void
nautilus_smooth_text_layout_cache_destroy (GtkObject *object)
{
 	NautilusSmoothTextLayoutCache *cache;

	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT_CACHE (object));

	cache = NAUTILUS_SMOOTH_TEXT_LAYOUT_CACHE (object);

	g_hash_table_foreach (cache->details->table, free_one_cache_entry, NULL);
	g_hash_table_destroy (cache->details->table);

	g_free (cache->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


/* Test harness
 *
 * The test works as follows:
 *
 * 0. Seed the random number generator with a constant value
 *
 * 1. Construct TOTAL_CASES/2 random test cases. Each has a random
 *    number of lines and a random number of words in each line. All
 *    the other parameters are also generated randomly.
 *
 * 2. In the second half of the test case array, randomly pick an entry
 *    from the first half and randomly change some parameters
 *
 * 3. Iterate TOTAL_TESTS times picking a random string, fetching its
 *    SmoothTextLayout object using both uncaching and caching
 *    functions, comparing the results (compares all fields in the
 *    structures, and their image buffers)
 */

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

#include "nautilus-debug.h"
#include "nautilus-self-checks.h"
#include <stdlib.h>
#include <stdio.h>

static NautilusSmoothTextLayoutCache *test_cache;
static NautilusScalableFont *test_font;

/* This is every 100th word from my /usr/share/dict/words file */
static char *test_words[] = {
	"abler", "absolve", "acceptances", "acculturating",
	"acquisitions", "adaptors", "adjure", "adrenaline", "Aeolus",
	"aforesaid", "agitations", "airplane", "Alexandre",
	"Allentown", "alphabetize", "Amazon", "amortize", "anaphora",
	"Angles", "annual", "anticipated", "apex", "appertain",
	"approbation", "archaicness", "aristocracy", "arrests",
	"ascensions", "assaults", "assurances", "Atlantis", "attire",
	"aura", "automation", "avouch", "babyhood", "Baffin", "balled",
	"bankrupting", "Barney", "basket", "Baylor", "beaver",
	"beforehand", "belies", "beneficiaries", "berths",
	"bewilderingly", "bilingual", "birdbath", "Blackfoot",
	"bleacher", "Blomquist", "blushes", "boilerplate",
	"bookkeeper", "bossed", "bowing", "branching", "breaks",
	"Bridgewater", "broadcastings", "browsing", "Buenos", "bumps",
	"Burmese", "butterball", "cabs", "callable", "cancel",
	"capability", "carbonizes", "carriages", "casts", "cautioner",
	"centaur", "chalices", "chargeable", "checkbook", "Chicanos",
	"choir", "chuckles", "circuses", "classes", "cliches",
	"closing", "coating", "cogitated", "colleges", "comedic",
	"commonalities", "compensatory", "comprehensibility",
	"conceptualized", "confectionery", "confusion", "Conrail",
	"constant", "contemplative", "contribute", "convinces",
	"coquette", "correspond", "councilwoman", "courtrooms",
	"cramps", "creditor", "croaks", "crumpled", "cultivable",
	"curtly", "cytoplasm", "dangle", "Davy", "debtor", "declarer",
	"deep", "defy", "delirious", "Denebola", "deploy",
	"descriptively", "destructiveness", "deviation", "diarrhea",
	"diggings", "Diogenes", "disapprove", "discovery", "disk",
	"displeasing", "distastes", "diverting", "doghouse", "dopers",
	"downstairs", "dreamers", "drudgery", "dungeons", "dynamism",
	"eaves", "educating", "ejecting", "elevation", "emancipate",
	"emphasizing", "encounter", "Engels", "enriches", "enumerated",
	"equates", "errs", "Ethernets", "Everglades", "exceeded",
	"executional", "expect", "explosive", "extract", "facsimile",
	"Falmouth", "Farrell", "Fayette", "feminism", "fibrously",
	"filthy", "firming", "flamer", "flew", "floweriness", "folksy",
	"forefathers", "formatively", "founds", "fray", "Freudianism",
	"Fruehauf", "furlong", "galaxy", "garlanded", "gearing",
	"geological", "giggle", "glazed", "Glynn", "Goodyear",
	"gradual", "grassiest", "greeter", "grooved", "gubernatorial",
	"guts", "hallmark", "handy", "harmfulness", "Hattie", "Healy",
	"heights", "heralds", "Heuser", "hindered", "hobby", "homing",
	"Horace", "housetop", "humorers", "Hyde", "identify",
	"imagine", "impenetrable", "impotence", "inane",
	"incompletely", "indescribable", "inductances", "inferior",
	"ingeniousness", "injured", "insertion", "institutes",
	"intensification", "interleaved", "interstate", "intubation",
	"invoked", "irreversibility", "itemizations", "Janus",
	"Jesuitizing", "Jordan", "Julie", "Kant", "keypad",
	"Kinnickinnic", "knots", "labyrinths", "landings", "later",
	"layers", "lectures", "leniency", "Lexington", "lighthearted",
	"lingerie", "littering", "Lockwood", "looms", "loves",
	"luring", "Mackey", "maids", "Malone", "Mann", "maritime",
	"maskable", "matrix", "McDonnell", "medal", "melodies",
	"mercenariness", "meteoritic", "microprocessing", "milk",
	"minima", "miserably", "MITREs", "module", "monkeyed", "moped",
	"motherland", "muck", "mumbles", "mutability", "names",
	"naughtiness", "negatives", "Neva", "nihilism",
	"nondeterminism", "north", "novices", "oases", "obstruction",
	"odious", "oleomargarine", "operate", "orbital", "Orleans",
	"outburst", "overboard", "oversized", "packaged", "palliative",
	"parades", "pares", "partitioned", "patients", "payoffs",
	"Peking", "perceived", "permeating", "perspiration", "phaser",
	"phyla", "piggybacked", "pipelining", "Plainfield", "playing",
	"plundered", "police", "pop", "position", "pottery",
	"preallocated", "predictive", "preposterously", "pretexts",
	"prior", "procurer", "projections", "properly", "protecting",
	"proximal", "puffed", "purges", "Pythagoreanizes",
	"quartering", "quince", "radiography", "rams", "rasping",
	"reacted", "reasonings", "receives", "recommend", "recursing",
	"reelects", "reformulated", "regrettable", "rejoiced",
	"reloader", "rendezvous", "repetitious", "reproducibilities",
	"resentment", "responded", "retailing", "retype", "revoked",
	"Richmond", "Riordan", "Roberta", "roofing", "rounding",
	"ruggedness", "ruthlessness", "sails", "sanatorium", "satires",
	"Scala", "schemers", "scorner", "scripts", "seat", "seeming",
	"semipermanently", "sequentially", "severance", "Shanghaied",
	"Shelley", "shiver", "showed", "sicken", "silent", "sinews",
	"sixties", "sky", "sleighs", "sluggishness", "smoothing",
	"snorkel", "sofas", "Somerset", "soured", "sparsely",
	"speculates", "spirituals", "sportswriter", "squadrons",
	"stagers", "star", "steadier", "sterilizer", "stirrer",
	"stouter", "streetcar", "strontium", "Styx", "subprogram",
	"subtleness", "suffocated", "Sumter", "suppers", "surround",
	"swat", "swivel", "synonymously", "Taipei", "tapestry",
	"teaches", "televise", "tenure", "Thailand", "Thessaly",
	"threatens", "tickles", "tingling", "tolerable", "torrent",
	"tracked", "transferal", "trapezoidal", "tribunals", "Trotsky",
	"tuft", "twenty", "umbrage", "unconditional", "undertakings",
	"ungratefully", "unkindness", "unraveling", "unused", "urge",
	"vacuumed", "variably", "Venetian", "vested", "vignettes",
	"visited", "vouching", "Walgreen", "warning", "wavelength",
	"Weidman", "wharves", "whitens", "wile", "Winsett",
	"womanhood", "worldliness", "writ", "yelped", "Zionism",
};

struct test_case {
	char *string;
	int font_size;
	gboolean wrap;
	int line_spacing;
	int max_text_width;
};

#define N_TEST_WORDS (sizeof (test_words) / sizeof (char *))

#define MIN_WORDS_PER_LINE 1
#define MAX_WORDS_PER_LINE 8

#define MIN_LINES 1
#define MAX_LINES 10

#define MIN_FONT_SIZE 8
#define MAX_FONT_SIZE 48

#define MIN_LINE_SPACING 0
#define MAX_LINE_SPACING 24

#define MIN_MAX_TEXT_WIDTH 50
#define MAX_MAX_TEXT_WIDTH 512

/* I chose 1 1/2 here so that we get both cache reuse and ejection */
#define TOTAL_CASES ((DEFAULT_MAXIMUM_SIZE * 3) / 2)

/* I'd prefer to do more than 10, here. But it takes too long */
#define TOTAL_TESTS (TOTAL_CASES * 10)

static int
random_integer (int min, int max)
{
	int divisor, limit, value;
	
	limit = max - min;
	divisor = RAND_MAX / limit;

	do {
		value = random () / divisor;
	} while (value >= limit);

	return min + value;
}

static gboolean
random_boolean (void)
{
	return random () > (RAND_MAX / 2);
}

static inline const char *
random_word (void)
{
	return test_words [random_integer (0, N_TEST_WORDS)];
}

static char *
local_stpcpy (char *dst, const char *src)
{
	while ((*dst++ = *src++) != 0) {
	}
	return dst - 1;
}

static char *
make_random_string (void)
{
	char *string, *ptr;
	int x, y;
	int width, height;
	const char *words[MAX_LINES][MAX_WORDS_PER_LINE+1];
        int string_length;

	height = random_integer (MIN_LINES, MAX_LINES);
	string_length = height;	/* newlines */
	for (y = 0; y < height; y++) {
		width = random_integer (MIN_WORDS_PER_LINE,
					MAX_WORDS_PER_LINE);
		string_length += width - 1;	/* spaces */
		for (x = 0; x < width; x++) {
			words[y][x] = random_word ();
			string_length += strlen (words[y][x]);
		}
		words[y][x] = NULL;
	}

	string = g_new (gchar, string_length + 1);
	ptr = string;
	for (y = 0; y < height; y++) {
		for (x = 0; words[y][x] != NULL; x++) {
			if (x != 0) {
				*ptr++ = ' ';
			}
			ptr = local_stpcpy (ptr, words[y][x]);
		}
		*ptr++ = '\n';
	}
	*ptr = '\0';

	return string;
}

static void
make_random_test_case (struct test_case *test)
{
	test->string = make_random_string ();
	test->font_size = random_integer (MIN_FONT_SIZE, MAX_FONT_SIZE);
	test->wrap = random_boolean ();
	test->line_spacing = random_integer (MIN_LINE_SPACING,
					     MAX_LINE_SPACING);
	test->max_text_width = random_integer (MIN_MAX_TEXT_WIDTH,
					       MAX_MAX_TEXT_WIDTH);
}

static void
randomize_test_case (struct test_case *dest, struct test_case *src)
{
	dest->string =  random_boolean () ? g_strdup (src->string) : make_random_string ();
	dest->font_size = (random_boolean () ? src->font_size
			   : random_integer (MIN_FONT_SIZE, MAX_FONT_SIZE));
	dest->wrap = random_boolean ();
	dest->line_spacing = (random_boolean () ? src->line_spacing
			      : random_integer (MIN_LINE_SPACING, MAX_LINE_SPACING));
	dest->max_text_width = (random_boolean () ? src->max_text_width
				: random_integer (MIN_MAX_TEXT_WIDTH, MAX_MAX_TEXT_WIDTH));
}

static void
free_test_case (struct test_case *test)
{
	g_free (test->string);
}

static gboolean
check_one (struct test_case *test)
{
	NautilusSmoothTextLayout *a, *b;
	gboolean result;

	g_assert (test_cache != NULL);
	g_assert (test->string != NULL);

	a = nautilus_smooth_text_layout_cache_render (test_cache,
						      test->string,
						      strlen (test->string),
						      test_font,
						      test->font_size,
						      test->wrap,
						      test->line_spacing,
						      test->max_text_width);
	g_assert (a != NULL);

	b = nautilus_smooth_text_layout_new (test->string,
					     strlen (test->string),
					     test_font,
					     test->font_size,
					     test->wrap);
	g_assert (b != NULL);
	nautilus_smooth_text_layout_set_line_spacing (b, test->line_spacing);
	nautilus_smooth_text_layout_set_line_wrap_width (b, test->max_text_width);

	result = nautilus_smooth_text_layout_compare (a, b);

	gtk_object_unref (GTK_OBJECT (a));
	gtk_object_unref (GTK_OBJECT (b));

	return result;
}

void
nautilus_self_check_smooth_text_layout_cache (void)
{
	struct test_case cases[TOTAL_CASES];
	int i, index;

	test_cache = nautilus_smooth_text_layout_cache_new ();
	test_font = nautilus_scalable_font_get_default_font ();

	/* initialize the random number generator to a known state */
	srandom (1);

	for (i = 0; i < TOTAL_CASES/2; i++) {
		make_random_test_case (cases + i);
	}
	for (; i < TOTAL_CASES; i++) {
		randomize_test_case (cases + i, cases + (i - TOTAL_CASES / 2));
	}

	for (i = 0; i < TOTAL_TESTS; i++) {
		index = random_integer (0, TOTAL_CASES);
		if (!check_one (cases + index)) {
			/* hmm.. */
			fprintf (stderr, "\nNautilusSmoothTextLayoutCache test %d failed\n", i);
			NAUTILUS_CHECK_BOOLEAN_RESULT (FALSE, TRUE);
		}
		if (i % 100 == 0) {
			fprintf (stderr, "[%d] ", i);
			fflush (stderr);
		}
	}
	fputc ('\n', stderr);

	for (i = 0; i < TOTAL_CASES; i++) {
		free_test_case (cases + i);
	}

	gtk_object_unref (GTK_OBJECT (test_font));
	gtk_object_unref (GTK_OBJECT (test_cache));
}

#endif /* NAUTILUS_OMIT_SELF_CHECK */
