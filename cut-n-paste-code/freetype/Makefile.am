lib_LTLIBRARIES=libfreetype2.la

CFLAGS=-DFT_FLAT_COMPILE

libfreetype2_la_SOURCES = \
	ftsystem.c	\
	ftdebug.c	\
	ftinit.c	\
	ftbase.c	\
	ftglyph.c	\
	ftmm.c	\
	cff.c	\
	type1cid.c	\
	psmodule.c	\
	raster1.c	\
	sfnt.c	\
	smooth.c	\
	truetype.c	\
	type1.c	\
	type1z.c	\
	winfnt.c

# All other .c files in the directory that are not listed above are
# #included, so don't generate their own .o (.lo) files. I'm pretty
# sure they have to be listed _somewhere_ here, if for no other reason
# than to make it into a tarball, but it seems to compile anyway.

libfreetype2includedir = $(includedir)/freetype

libfreetype2include_HEADERS= \
	ftconfig.h	\
	freetype.h	\
	ftbbox.h	\
	fterrors.h	\
	ftglyph.h	\
	ftimage.h	\
	ftmm.h	\
	ftmodule.h	\
	ftoutln.h	\
	ftrender.h	\
	ftsystem.h	\
	fttypes.h	\
	t1tables.h	\
	ttnameid.h	\
	tttables.h	\
	tttags.h
