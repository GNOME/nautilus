#include "fm-icon-cache.h"
#include <gnome.h>

#define ICON_CACHE_MAX_ENTRIES 10
#define ICON_CACHE_SWEEP_TIMEOUT 10

/* This allows us to do smarter caching */
static guint use_counter = 0;

typedef struct {
  char *name;
  GdkPixbuf *plain, *symlink;
  guint last_use;
} IconSet;

typedef enum {
  ICONSET_DIRECTORY,
  ICONSET_DIRCLOSED,
  ICONSET_EXECUTABLE,
  ICONSET_REGULAR,
  ICONSET_CORE,
  ICONSET_SOCKET,
  ICONSET_FIFO,
  ICONSET_CHARDEVICE,
  ICONSET_BLOCKDEVICE,
  ICONSET_BROKENSYMLINK,
  ICONSET_SPECIAL_LAST
} SpecialIconSetType;

struct _FMIconCache {
  char *theme_name;
  GHashTable *name_to_image;

  IconSet special_isets[ICONSET_SPECIAL_LAST];

  GdkPixbuf *symlink_overlay;

  guint sweep_timer;
};

static IconSet *
icon_set_new (const gchar *name)
{
  IconSet *new;

  new = g_new (IconSet, 1);
  new->name = g_strdup (name);
  new->plain = NULL;
  new->symlink = NULL;

  return new;
}

static void
icon_set_destroy (IconSet *icon_set, gboolean free_name)
{
  if (icon_set != NULL)
    {
      if(free_name)
	g_free (icon_set->name);
      if(icon_set->plain)
	gdk_pixbuf_unref(icon_set->plain);
      if(icon_set->symlink)
	gdk_pixbuf_unref(icon_set->symlink);
    }
}

FMIconCache *
fm_icon_cache_new(const char *theme_name)
{
  FMIconCache *retval;

  retval = g_new0(FMIconCache, 1);
  retval->theme_name = g_strdup(theme_name);
  retval->name_to_image = g_hash_table_new(g_str_hash, g_str_equal);
  retval->special_isets[ICONSET_DIRECTORY].name = "i-directory.png"; 
  retval->special_isets[ICONSET_DIRCLOSED].name = "i-dirclosed.png"; 
  retval->special_isets[ICONSET_EXECUTABLE].name = "i-executable.png"; 
  retval->special_isets[ICONSET_REGULAR].name = "i-regular.png"; 
  retval->special_isets[ICONSET_CORE].name = "i-core.png"; 
  retval->special_isets[ICONSET_SOCKET].name = "i-sock.png"; 
  retval->special_isets[ICONSET_FIFO].name = "i-fifo.png"; 
  retval->special_isets[ICONSET_CHARDEVICE].name = "i-chardev.png"; 
  retval->special_isets[ICONSET_BLOCKDEVICE].name = "i-blockdev.png"; 
  retval->special_isets[ICONSET_BROKENSYMLINK].name = "i-brokenlink.png"; 

  return retval;
}

static gboolean
fm_icon_cache_destroy_iconsets(gpointer key, gpointer value, gpointer user_data)
{
  IconSet *is = value;

  icon_set_destroy(is, TRUE);

  return TRUE;
}

static void
fm_icon_cache_invalidate(FMIconCache *fmic)
{
  int i;
  g_hash_table_foreach_remove(fmic->name_to_image, fm_icon_cache_destroy_iconsets, fmic);
  for(i = 0; i < ICONSET_SPECIAL_LAST; i++)
    icon_set_destroy(&fmic->special_isets[i], FALSE);

  if(fmic->symlink_overlay)
    {
      gdk_pixbuf_unref(fmic->symlink_overlay);
      fmic->symlink_overlay = NULL;
    }
}

void
fm_icon_cache_destroy(FMIconCache *fmic)
{
  fm_icon_cache_invalidate(fmic);
  g_hash_table_destroy(fmic->name_to_image);

  g_free(fmic->theme_name);
  g_free(fmic);
}

static gboolean
icon_set_possibly_free(gpointer key, gpointer value, gpointer user_data)
{
  IconSet *is = value;

  if(is->last_use > (use_counter - ICON_CACHE_MAX_ENTRIES))
    return FALSE;

  if (is->plain && is->plain->ref_count <= 1)
    {
      gdk_pixbuf_unref(is->plain);
      is->plain = NULL;
    }

  if (is->symlink && is->symlink->ref_count <= 1)
    {
      gdk_pixbuf_unref(is->symlink);
      is->symlink = NULL;
    }

  if (!is->symlink && !is->plain)
    {
      g_free(is->name);
      return TRUE;
    }

  return FALSE;
}

static gboolean
fm_icon_cache_sweep(gpointer data)
{
  FMIconCache *fmic = data;

  g_hash_table_foreach_remove(fmic->name_to_image, icon_set_possibly_free, NULL);

  fmic->sweep_timer = 0;

  return FALSE;
}

static void
fm_icon_cache_setup_sweep(FMIconCache *fmic)
{
  if(fmic->sweep_timer)
    return;

  if(g_hash_table_size(fmic->name_to_image) < ICON_CACHE_MAX_ENTRIES)
    return;

  fmic->sweep_timer = g_timeout_add(ICON_CACHE_SWEEP_TIMEOUT * 1000, fm_icon_cache_sweep, fmic);
}

void
fm_icon_cache_set_theme(FMIconCache *fmic, const char *theme_name)
{
  fm_icon_cache_invalidate(fmic);
  g_free(fmic->theme_name);
  fmic->theme_name = g_strdup(theme_name);
}

static IconSet *
fm_icon_cache_get_icon_for_file(FMIconCache *fmic, const GnomeVFSFileInfo *info)
{
  IconSet *retval = NULL;
  const char *icon_name = NULL;

  if(info->mime_type)
    icon_name = gnome_mime_get_value(info->mime_type,
				     "icon-filename");
  if(icon_name)
    {
      retval = g_hash_table_lookup(fmic->name_to_image, icon_name);
      if (!retval)
	{
	  retval = icon_set_new(icon_name);
	  g_hash_table_insert(fmic->name_to_image, retval->name, retval);
	}
    }
  else
    {
      /* We can't get a name, so we have to do some faking to figure out what set to load */
      if (info->permissions & (GNOME_VFS_PERM_USER_EXEC
			       | GNOME_VFS_PERM_GROUP_EXEC
			       | GNOME_VFS_PERM_OTHER_EXEC))
	retval = &fmic->special_isets[ICONSET_EXECUTABLE];
      else
	retval = &fmic->special_isets[ICONSET_REGULAR];
    }

  return retval;
}

static GdkPixbuf *
fm_icon_cache_load_file(FMIconCache *fmic, const char *fn)
{
  char *file_name = NULL;
  char cbuf[128];
  GdkPixbuf *retval;

  if(*fn != '/')
    {
      if(fmic->theme_name)
	{
	  g_snprintf(cbuf, sizeof(cbuf), "nautilus/%s/%s", fmic->theme_name, fn);

	  file_name = gnome_pixmap_file(cbuf);
	}

      if(!file_name)
	{
	  g_snprintf(cbuf, sizeof(cbuf), "nautilus/%s", fn);
	  file_name = gnome_pixmap_file(cbuf);
	}
    }

  retval = gdk_pixbuf_new_from_file(file_name?file_name:fn);
  g_free(file_name);

  return retval;
}

/* Splats one on top of the other, putting the src pixbuf in the lower left corner of the dest pixbuf */
static void
gdk_pixbuf_composite(GdkPixbuf *dest, GdkPixbuf *src)
{
  guchar *dest_data;
  int dx, dy, dw, dh, drs;
  guchar *rgba;
  int rrs;
  int da;
  int r, g, b, a;
  int i,j;
  guchar *imgrow;
  guchar *dstrow;
  int dest_has_alpha, src_has_alpha;

  dw = MIN(dest->art_pixbuf->width, src->art_pixbuf->width);
  dh = MIN(dest->art_pixbuf->width, src->art_pixbuf->width);
  dx = dw - src->art_pixbuf->width;
  dy = dh - src->art_pixbuf->height;

  dest_has_alpha = dest->art_pixbuf->has_alpha;
  src_has_alpha = src->art_pixbuf->has_alpha;

  dest_data = dest->art_pixbuf->pixels;
  rgba = src->art_pixbuf->pixels;

  drs = dest->art_pixbuf->rowstride;
  rrs = src->art_pixbuf->rowstride;

  /* To do as few conditionals inside the loop as possible, we have four different variants of it */
  if (dest_has_alpha)
    {
      if(src_has_alpha)
	{
	  for (j = 0; j < dh; j++)
	    {
	      imgrow = rgba + j * rrs;
	      dstrow = dest_data + (dy+j) * drs + (dx*4);

	      for (i = 0; i < dw; i++)
		{
		  r = *(imgrow++);
		  g = *(imgrow++);
		  b = *(imgrow++);
		  a = *(imgrow++);

		  da = *(dstrow+3);
		  *dstrow = (r*a + *dstrow * (da-a))>>8;
		  dstrow++;
		  *dstrow = (g*a + *dstrow * (da-a))>>8;
		  dstrow++;
		  *dstrow = (b*a + *dstrow * (da-a))>>8;
		  dstrow++;

		  *dstrow = (da * a)>>8;
		  dstrow++;
		}
	    }
	}
      else
	{
	  for (j = 0; j < dh; j++)
	    {
	      imgrow = rgba + j * rrs;
	      dstrow = dest_data + (dy+j) * drs + (dx*4);

	      for (i = 0; i < dw; i++)
		{
		  memcpy(dstrow, imgrow, 3);
		  imgrow += 3;
		  dstrow += 3;

		  *dstrow = 255;
		  dstrow++;
		}
	    }
	}
    }
  else
    {
      if(src_has_alpha)
	{
	  for (j = 0; j < dh; j++)
	    {
	      imgrow = rgba + j * rrs;
	      dstrow = dest_data + (dy+j) * drs + (dx*3);

	      for (i = 0; i < dw; i++)
		{
		  r = *(imgrow++);
		  g = *(imgrow++);
		  b = *(imgrow++);
		  a = *(imgrow++);
		  *dstrow = (r*a + *dstrow * (256-a))>>8;
		  dstrow++;
		  *dstrow = (g*a + *dstrow * (256-a))>>8;
		  dstrow++;
		  *dstrow = (b*a + *dstrow * (256-a))>>8;
		  dstrow++;
		}
	    }
	}
      else
	{
	  for (j = 0; j < dh; j++)
	    {
	      imgrow = rgba + j * rrs;
	      dstrow = dest_data + (dy+j) * drs + (dx*3);

	      memcpy(dstrow, imgrow, dw);
	    }
	}
    }
}

static GdkPixbuf *
fm_icon_cache_load_icon(FMIconCache *fmic, IconSet *is, gboolean is_symlink)
{
  GdkPixbuf *retval;

  if(is_symlink)
    retval = is->symlink;
  else
    retval = is->plain;

  if (!retval)
    {
      /* need to load the file */
      retval = fm_icon_cache_load_file(fmic, is->name);

      if (is_symlink)
	{
	  if (!fmic->symlink_overlay)
	    fmic->symlink_overlay = fm_icon_cache_load_file(fmic, "i-symlink.png");

	  if(fmic->symlink_overlay)
	    gdk_pixbuf_composite(retval, fmic->symlink_overlay);
	  is->symlink = retval;
	}
      else
	is->plain = retval;
    }

  gdk_pixbuf_ref(retval); /* Returned value is owned by caller */

  return retval;
}

GdkPixbuf *
fm_icon_cache_get_icon(FMIconCache *fmic,
		       const GnomeVFSFileInfo *info)
{
  IconSet *is;
  GdkPixbuf *retval;

  g_return_val_if_fail(fmic, NULL);
  g_return_val_if_fail(info, NULL);

  switch (info->type)
    {
    case GNOME_VFS_FILE_TYPE_UNKNOWN:
    case GNOME_VFS_FILE_TYPE_REGULAR:
    default:
      is = fm_icon_cache_get_icon_for_file(fmic, info);
      break;
    case GNOME_VFS_FILE_TYPE_DIRECTORY:
      is = &fmic->special_isets[ICONSET_DIRECTORY];
      break;
    case GNOME_VFS_FILE_TYPE_FIFO:
      is = &fmic->special_isets[ICONSET_FIFO];
      break;
    case GNOME_VFS_FILE_TYPE_SOCKET:
      is = &fmic->special_isets[ICONSET_SOCKET];
      break;
    case GNOME_VFS_FILE_TYPE_CHARDEVICE:
      is = &fmic->special_isets[ICONSET_CHARDEVICE];
      break;
    case GNOME_VFS_FILE_TYPE_BLOCKDEVICE:
      is = &fmic->special_isets[ICONSET_BLOCKDEVICE];
      break;
    case GNOME_VFS_FILE_TYPE_BROKENSYMLINK:
      is = &fmic->special_isets[ICONSET_BROKENSYMLINK];
      break;
    }

  is->last_use = use_counter++;
  retval = fm_icon_cache_load_icon(fmic, is, GNOME_VFS_FILE_INFO_SYMLINK(info));

  if(retval)
    fm_icon_cache_setup_sweep(fmic);

  return retval;
}
