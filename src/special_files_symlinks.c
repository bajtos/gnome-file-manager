#include "special_files_symlinks.h"
#include "gfm_vfs.h"
#include <glib.h>
#include "interface.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#include "debug.h"
	
#define ASYNC_JOB_PRIORITY 5

struct _SymlinksData {	
	gint panel;
	GList *uris;
	GnomeVFSAsyncHandle *handle;

	gboolean aborted;

	/* callback data */
	GList *iters;
	gint levels_remaining; /* how many times we can recursive resolve this link */
};


static GnomeVFSURI *
extract_symlink_target		(const GnomeVFSURI *uri, 
				const GnomeVFSFileInfo *info);

static void
get_file_info_callback		(GnomeVFSAsyncHandle *handle,
				GList *result,
				SymlinksData *sl_data);

/* is info symlinks? should we follow? */
static gboolean
follow_symlink			(const GnomeVFSFileInfo *info);

/*************************************
 * 
 * symlinks_* stuff 
 * (from special_files_symlinks.h)
 *
 *************************************/

void
symlinks_init		(SymlinksData **sl_data, gint panel)
{
#define data (*sl_data)
	g_return_if_fail (IS_PANEL(panel));

	TRACE 

	if (data) 
		symlinks_abort (data);
	else {
		data = g_new (SymlinksData, 1);
	}

	data->panel = panel;
	data->uris = NULL;
	data->handle = NULL;
	data->aborted = FALSE;
	
	data->iters = NULL;
	data->levels_remaining = special_files_max_symlinks_level;
#undef data
}

void
symlinks_add_file	(SymlinksData *sl_data, 
			GtkTreeIter *iter, 
			const GnomeVFSFileInfo *info,
			const GnomeVFSURI *uri)
{
	g_return_if_fail (sl_data != NULL);
	g_return_if_fail (iter != NULL);
	g_return_if_fail (info != NULL);
	g_return_if_fail (uri != NULL);
	if (sl_data->aborted) {
		TRACEM("Aborted\n");
		return;
	}

	TRACE
	if (follow_symlink (info)) { 
		GnomeVFSURI *target_uri = NULL;
		TRACEM ("%s\n", info->name)
			
		target_uri = extract_symlink_target (uri, info);
		if (target_uri) {
			sl_data->uris = g_list_append (sl_data->uris, target_uri);
			sl_data->iters = g_list_append (sl_data->iters, gtk_tree_iter_copy (iter));
		}
	}
}

void
symlinks_execute	(SymlinksData *sl_data)
{
	GList *uris;
	
	g_return_if_fail (sl_data != NULL);
	g_return_if_fail (sl_data->handle == NULL);
	if (sl_data->aborted) {
		TRACEM("Aborted\n");
		return;
	}
	if (sl_data->uris == NULL) {
		TRACEM("List empty\n");
		return;
	}
	if (!--sl_data->levels_remaining) {
		TRACEM("Recursion limit exceeded\n");
		return;
	}
	
	TRACE 
		
	uris = sl_data->uris;
	sl_data->uris = NULL;

	gnome_vfs_async_get_file_info (
		&sl_data->handle,
		uris,
		GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
		ASYNC_JOB_PRIORITY, 
		(GnomeVFSAsyncGetFileInfoCallback) get_file_info_callback,		
		sl_data
	);

	gnome_vfs_uri_list_free (uris);
}

void
symlinks_abort		(SymlinksData *sl_data)
{
	g_return_if_fail (sl_data != NULL);

	TRACE

	sl_data->aborted = TRUE;

	if (sl_data->handle) {
		gnome_vfs_async_cancel (sl_data->handle);
		sl_data->handle = NULL;
	}

	gnome_vfs_uri_list_free (sl_data->uris);
	sl_data->uris = NULL;

	g_list_foreach (sl_data->iters, (GFunc) gtk_tree_iter_free, NULL);
	g_list_free (sl_data->iters);
	sl_data->iters = NULL;
}

/*************************************
 *
 * get_file_info_callback
 *
 *************************************/ 
static void
get_file_info_callback	(GnomeVFSAsyncHandle *handle,
			GList *result,
			SymlinksData *sl_data)
{
	GnomeVFSFileInfo *info;
	GnomeVFSURI *uri;
	GnomeVFSResult res;
	
	GtkListStore *model;
	GfmRowData *row_data;

	GList *r, *it, *it2;

	TRACE
	g_assert (sl_data->uris == NULL);
	it=sl_data->iters;
	for (r=result; r; r=r->next) {
		g_assert (it);
		
		info = ((GnomeVFSGetFileInfoResult *)r->data)->file_info;
		uri = ((GnomeVFSGetFileInfoResult *)r->data)->uri;
		res = ((GnomeVFSGetFileInfoResult *)r->data)->result;

		TRACEM("%s\n", info->name);

		if (res != GNOME_VFS_OK) {
			TRACEM("<Error\n");
			goto remove;
		}

		if (follow_symlink (info)) {
			TRACEM("<Follow\n");
			uri = extract_symlink_target (uri, info);
			sl_data->uris = g_list_append (sl_data->uris, uri);
			it = it->next;
#if DEBUG 
			{ gchar *u;
			  u = gnome_vfs_uri_to_string (uri, 0);	
			printf("Following %s to %s\n", info->name, u);
			  free (u);
			}
#endif
			continue;
		}
		TRACEM("<Done\n");
		
		model = panels_data[sl_data->panel].files_data;
		gtk_tree_model_get (GTK_TREE_MODEL(model), it->data, GFM_COLUMN_DATA, &row_data, -1);
		if (row_data->real_info)
			gnome_vfs_file_info_unref (row_data->real_info);

		gnome_vfs_file_info_ref (info);
		row_data->real_info = info;
		info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_FLAGS;
		info->flags |= GNOME_VFS_FILE_FLAGS_SYMLINK;
		gtk_list_store_set (model, it->data,
			GFM_COLUMN_DATA, row_data,
			GFM_COLUMN_ICON, NULL,
			-1);

		remove:
			TRACEM("%s\n", info->name);
			it2 = it->next;
			gtk_tree_iter_free (it->data);
			sl_data->iters = g_list_delete_link (sl_data->iters, it);
			it = it2;
			
	}	

	sl_data->handle = NULL;
	symlinks_execute (sl_data);
}

/*************************************
 * 
 * extract_symlink_target
 *
 *************************************/

static GnomeVFSURI *
extract_symlink_target (const GnomeVFSURI *uri, const GnomeVFSFileInfo *info)
{
	const gchar *target;
	GnomeVFSURI *parent=NULL;
	GnomeVFSURI *ret=NULL;
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME, NULL);


	target = info->symlink_name;
	TRACEM("%s\n", target);
	g_return_val_if_fail (target != NULL, NULL);

	if (target[0] == '/') /* absolute path */ {
TRACE
		ret = gnome_vfs_uri_new (target);
		TRACEM("%s\n",gnome_vfs_uri_to_string (ret,0));
	} else {
TRACEM("uri: %s\n", gnome_vfs_uri_to_string(uri, 0));
		if (gnome_vfs_uri_has_parent (uri)) {
TRACE
			parent = gnome_vfs_uri_get_parent (uri);
			ret = gnome_vfs_uri_append_path (parent, target);
			TRACEM("%s\n",gnome_vfs_uri_to_string (ret,0))
			gnome_vfs_uri_unref (parent);	
		}
TRACE
	}

	if (!ret)
		TRACEM("warning: returning NULL");
	return ret;
}

static gboolean
follow_symlink			(const GnomeVFSFileInfo *info)
{
	return (
	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE
	  && info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK)
		||
	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_FLAGS
	 && info->flags & GNOME_VFS_FILE_FLAGS_SYMLINK))
	
	&& (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME);
}
