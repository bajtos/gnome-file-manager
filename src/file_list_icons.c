#ifndef DEBUG
#define DEBUG 0
#endif

#include "debug.h"

#include "file_list_icons.h"
#include <gnome.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "gfm_vfs.h"
#include "string.h"

#define DEFAULT_REGULAR_ICON "i-regular.png"
#define DEFAULT_EXECUTABLE_ICON "i-executable.png"
#define DEFAULT_DIRECTORY_ICON "i-directory.png"
#define SYMLINK_MASK_ICON "emblem-symbolic-link.png"

static char *
get_icon_path (const char *path_or_name);

static void
file_list_icons_cleanup (void);

	
static
GHashTable *icons = NULL;
static
GdkPixbuf *symlink_mask = NULL;


void
gdk_pixbuf_unref (GdkPixbuf *pixbuf);

void
file_list_icons_init (void)
{
	GdkPixbuf *pixbuf;
	gchar *ipath;
	GError *error = NULL;


#if DEBUG 
puts("icons_init()");
#endif 
	file_list_icons_cleanup();

	/** hash for icons **/
#if DEBUG 
	puts("	creating hash");
#endif 
	icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gdk_pixbuf_unref);
	g_return_if_fail (icons);
	
	/** symlink mask **/
#if DEBUG 
	puts("	loading symlink mask");
#endif 
	ipath = get_icon_path (SYMLINK_MASK_ICON);
#if DEBUG 
	printf("		path: %s\n",ipath);
#endif 

	g_return_if_fail (ipath != NULL);
	TRACE
	pixbuf = gdk_pixbuf_new_from_file (ipath, &error);
#if DEBUG 
	puts("		scaling");
#endif 

	free (ipath);
	if (pixbuf) {
		symlink_mask = gdk_pixbuf_scale_simple (pixbuf, 6,6, GDK_INTERP_BILINEAR);
		gdk_pixbuf_unref (pixbuf);
	} else {
		g_print ("Could not load symlinks icon: %s\n", error->message);
		g_error_free (error);
	}

#if DEBUG 
	puts(" registering cleanup");
#endif 
	atexit (file_list_icons_cleanup);

#if DEBUG 
	puts("done");
#endif 
	
}	
		

static void
file_list_icons_cleanup (void)
{
	if (symlink_mask) {
		gdk_pixbuf_unref (symlink_mask);
		symlink_mask = NULL;
	}
	
	if (icons) {
		g_hash_table_destroy (icons);
		icons = NULL;
	}
}


GdkPixbuf *
file_list_icons_get_icon (const GnomeVFSFileInfo *info)
{
	GdkPixbuf *pixbuf, *scaled;
	gchar *ipath; 
	GError *error = NULL;

	g_return_val_if_fail (info->mime_type != NULL, NULL);

TRACEM("%s\n", info->mime_type)
	if (!g_hash_table_lookup_extended (icons, info->mime_type, NULL, (gpointer) &pixbuf))
	{ /** not in the hash, we must seek and load **/ 
		ipath = get_icon_path(gnome_vfs_mime_get_icon (info->mime_type));
		if (!ipath) {
			gchar *tmp, *tmp1, *real_icon_name;
			TRACEM("not found, trying default\n");

			tmp = g_strdup (info->mime_type);
			tmp1 = strchr (tmp, '/');
			if (tmp1 != NULL) *tmp1 = '-';
			real_icon_name = g_strconcat ("document-icons/gnome-", tmp, ".png", NULL);
			g_free (tmp);

			ipath = get_icon_path (real_icon_name);
			g_free (real_icon_name);

			if (!ipath) {
				TRACEM("failed too, falling back to defaults");
				ipath = get_icon_path (
						gfm_is_directory(info)?
							DEFAULT_DIRECTORY_ICON:
						(GFM_IS_EXECUTABLE (info)?
							DEFAULT_EXECUTABLE_ICON:
							DEFAULT_REGULAR_ICON)
					);
			}
		}
	
		g_return_val_if_fail (ipath != NULL, NULL);
		TRACE
		pixbuf = gdk_pixbuf_new_from_file (ipath, &error);
		if (! pixbuf) {
			g_warning (_("Could not load icon %s: %s\n"), ipath, error->message);
			g_error_free (error);
			goto insert;
		}
		free (ipath);
			
		/* TODO: scaling according to the size of list */
		scaled = gdk_pixbuf_scale_simple (pixbuf,
				12,12, GDK_INTERP_BILINEAR);
		
		gdk_pixbuf_unref(pixbuf);
		pixbuf = gdk_pixbuf_add_alpha (scaled, FALSE, 0,0,0);
		gdk_pixbuf_unref(scaled);
		
insert:	
		g_hash_table_insert (icons, g_strdup(info->mime_type), pixbuf);
	}
	if (!pixbuf) /* TODO: create some image */
		return NULL;

	/* symlinks */
	/* this is unresolved symlink */
	if (	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) &&
		(info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) && 
		 symlink_mask && pixbuf) {
		
		int 	sw = gdk_pixbuf_get_width (symlink_mask),
			sh = gdk_pixbuf_get_height (symlink_mask),
			iw = gdk_pixbuf_get_width (pixbuf),
			ih = gdk_pixbuf_get_height (pixbuf);
		gchar *real_key = g_strconcat (info->mime_type, "|unres-symlink", NULL);
		GdkPixbuf *real_pixbuf = (GdkPixbuf *)g_hash_table_lookup (icons, real_key);
		
		TRACEM("unres-symlink\n");
		if (real_pixbuf)
			return real_pixbuf;

		TRACE
		real_pixbuf = gdk_pixbuf_copy (pixbuf);
		gdk_pixbuf_copy_area (symlink_mask,
			0, 0, 
			sw, sh,
			real_pixbuf,
			iw-sw,ih-sh);

		TRACE
		g_hash_table_insert (icons, real_key, real_pixbuf);
TRACE
		return real_pixbuf;
	}
		
	/* and this is resolved symlink */
	if (	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_FLAGS) &&
		(info->flags & GNOME_VFS_FILE_FLAGS_SYMLINK) &&
		 symlink_mask && pixbuf) {
		
		int 	sw = gdk_pixbuf_get_width (symlink_mask),
			sh = gdk_pixbuf_get_height (symlink_mask),
			iw = gdk_pixbuf_get_width (pixbuf),
			ih = gdk_pixbuf_get_height (pixbuf);
		gchar *real_key = g_strconcat (info->mime_type, "|res-symlink", NULL);
		GdkPixbuf *real_pixbuf = (GdkPixbuf *)g_hash_table_lookup (icons, real_key);

		TRACEM("res-symlink\n");
		if (real_pixbuf)
			return real_pixbuf;

		TRACE
		real_pixbuf = gdk_pixbuf_copy (pixbuf);
		gdk_pixbuf_copy_area (symlink_mask,
			0, 0, 
			sw, sh,
			real_pixbuf,
			iw-sw,ih-sh);

		TRACE
		g_hash_table_insert (icons, real_key, real_pixbuf);
TRACE
		return real_pixbuf;
	}

TRACE
	return pixbuf;

}

static char *
get_icon_path (const char *path_or_name)
{
	gchar *result;
	gchar *real_name;

	if (! path_or_name) {
		return NULL;
	}
	TRACEM("%s\n",path_or_name);

	result = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_PIXMAP,
			path_or_name,
			TRUE, /* only if exists */
			NULL);
	if (result && g_file_test (result, G_FILE_TEST_IS_REGULAR)) {
		TRACEM("%s\n", result);
		return result;
	}

	if (strstr(path_or_name, ".png") == NULL) {
		real_name = g_strconcat (path_or_name, ".png", NULL);
#if DEBUG 
		printf ("Trying %s\n", real_name);
#endif 
		result = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_PIXMAP,
				real_name,
				TRUE, /* only if exists */
				NULL);
		g_free (real_name);
		if (result && g_file_test (result, G_FILE_TEST_IS_REGULAR)) {
			TRACEM("%s\n",result);
			return result;
		}
	}

	real_name = g_strconcat ("document-icons/", path_or_name, NULL);
#if DEBUG 
	printf ("Trying %s\n", real_name);
#endif 
	result = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_PIXMAP,
			real_name,
			TRUE, /* only if exists */
			NULL);
	g_free (real_name);
	if (result && g_file_test (result, G_FILE_TEST_IS_REGULAR)) {
		TRACEM("%s\n",result);
		return result;
	}

	
	if (strstr(path_or_name, ".png") == NULL) {
		real_name = g_strconcat ("document-icons/", path_or_name, ".png", NULL);
#if DEBUG 
		printf ("Trying %s\n", real_name);
#endif 
		result = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_PIXMAP,
				real_name,
				TRUE, /* only if exists */
				NULL);
		g_free (real_name);
		if (result) {
			TRACEM("%s\n",result);
			return result;
		}
	}


	/* our icons */
	result = g_strconcat (GFM_PIXMAPDIR, "/", path_or_name, NULL);
	if (result && g_file_test (result, G_FILE_TEST_IS_REGULAR)) {
		TRACEM("%s\n",result);
		return result;
	}
	else {
#if DEBUG
		TRACEM("our: %s failed", result);
#endif
		if (result) g_free(result);
	}
	
#if DEBUG 
	puts("failed");
#endif
	return NULL;

}

