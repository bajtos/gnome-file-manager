#include "special_files.h"
#include "special_files_symlinks.h"
#include "interface.h"
#include <glib.h>
#include "gfm_vfs.h"
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#include "debug.h"
	
static struct {
	SymlinksData *symlinks_data;
} global_data [NUM_PANELS];


guint special_files_max_symlinks_level = 10;



/*************************************
 *
 * special_files_init
 *
 *************************************/

void
special_files_init			(gint panel)
{
	g_return_if_fail (IS_PANEL(panel));

	TRACE
	symlinks_init (&global_data[panel].symlinks_data, panel);
}

/*************************************
 *
 * special_files_add_file_to_list
 *
 *************************************/

void
special_files_add_file_to_list		(gint panel,
					GtkTreeIter *iter,
					GfmRowData *row_data)
{
	GnomeVFSURI *uri = NULL;
	GnomeVFSFileInfo *info;

	g_return_if_fail (IS_PANEL(panel));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (row_data != NULL);
	
	info = row_data->info;
	g_return_if_fail (info != NULL);

	TRACE
	uri = gnome_vfs_uri_append_path (panels_data[panel].uri_wdir, row_data->info->name);
	TRACEM("uri: %s\n", gnome_vfs_uri_to_string (uri, 2));
	symlinks_add_file (global_data[panel].symlinks_data, iter, info, uri);
	TRACE

	gnome_vfs_uri_unref (uri);
	TRACE
	gfm_row_data_free (row_data);
	gtk_tree_iter_free (iter);
}



/*************************************
 *
 * special_files_process_list
 * 
 *************************************/

void
special_files_process_list		(gint panel)
{
	TRACE
	symlinks_execute (global_data[panel].symlinks_data);
}


/*************************************
 *
 * special_files_abort
 *
 *************************************/
void
special_files_abort			(gint panel)
{
	TRACE
	if (global_data[panel].symlinks_data) {
		symlinks_abort (global_data[panel].symlinks_data);
//		global_data[panel].symlinks_data = NULL;
	}
}

/*************************************
 *
 * check_set_default_mime_type
 *
 *************************************/

void check_set_default_mime_type (GnomeVFSFileInfo *info)
{
	return;

	/* this is solved in Gnome2
	
	if (! (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)) {
		info->mime_type = g_strdup (gnome_vfs_mime_type_from_name_or_default (info->name,
			gfm_is_directory(info)?"x-directory/normal":"application/octet-stream"));
		info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
	}
	*/
}





