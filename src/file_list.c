#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

#include "file_list.h"
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib.h>
#include "gfm_vfs.h"
#include "file_list_icons.h"
#include "copy_move_delete.h"
#include "configuration.h"
#include "interface.h"
#include "gfm_list_store.h"
#include "mime_actions.h"
#include "special_files.h"
#include "selections.h"
#include "lister.h"
#include "file_list_tips.h"
#include "wd_entry.h"

/*
 *
 *       E V E N T   H A N D L I N G
 *
 */

void
on_file_list_destroy                   (GtkObject       *object,
                                        gpointer         user_data);
					
/*
 * set sorting of list
 */
gboolean
on_file_list_button_press_event	       (GtkTreeView		*flist,
					GdkEventButton		*event,
					gpointer		user_data);

gboolean
on_file_list_key_press_event           (GtkTreeView		*flist,
                                        GdkEventKey		*event,
                                        gpointer		user_data);


/****************************
 *
 * Supporting functions
 *
 */

#if SHOW_MOUNT_POINT
static gchar *
find_mount_point (const gchar *path);
#endif

/*
 * updates GtkLabel displaying formatted string with info 
 * about free capacity on device where uri is located
 * and path where is this device mounted (if SHOW_MOUNT_POINT is set) 
 */

static void
dfree_set_text (gint panel);


/*
 * main function of this module, parses each info from list and feed this
 * informations into GtkClist pointed by callback_data 
 * TODO: add some other info to callback_data
 */
static void
load_dir_callback      (GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			GList *list,
			guint entries_read,
			gpointer callback_data);

/* row_data will be released during shutdown */ 
static void
append_file_to_flist	(gint panel,
			GfmRowData *row_data);

static gboolean
show_hidden_files	(gint panel);

static void
gfm_mkdir		(gint panel);

static struct {
	guint dir_perm; /* permissions of working directory -- we need this to mkdir, etc. */
} private_data[NUM_PANELS];

/**************************
 *
 * 	CODE
 *
 */

/*
 * 	file_list_abort()
 */
void
file_list_abort (gint panel) {
	GnomeVFSAsyncHandle **handle = panels_data[panel].load_dir_handle;
	GnomeVFSURI **uri;

	g_assert (handle != NULL);
	g_return_if_fail (panel == PANEL_LEFT || panel == PANEL_RIGHT);
	g_return_if_fail (panels_data[panel].files_view != NULL);

	special_files_abort (panel);

TRACE

	if (*handle == NULL) return;

	gnome_vfs_async_cancel (*handle);

	uri = &panels_data[panel].uri_ndir;
	if (*uri) {
		gnome_vfs_uri_unref (*uri);
		*uri = NULL;
	}

	gnome_appbar_set_status (app_bar, _("Interrupted"));
TRACEM("done\n")
}



/*
 * 	file_list_chdir()
 */
void
file_list_chdir (gint panel, GnomeVFSURI *uri)
{
	
	g_return_if_fail (panel == PANEL_LEFT || panel == PANEL_RIGHT);
	g_return_if_fail (uri != NULL);

TRACEM("panel=%d\n",panel)

	file_list_abort (panel);
	
	g_return_if_fail (panels_data[panel].uri_ndir == NULL);

	panels_data[panel].uri_ndir = uri;
	gnome_vfs_uri_ref (uri);

	file_list_update (panel);
	gnome_vfs_uri_unref (uri);
TRACEM("done\n")
}


/*
 * 	file_list_update()
 */
void
file_list_update (gint panel) {

	GnomeVFSAsyncHandle **handle;
	PanelData *pd;
	
	g_return_if_fail (IS_PANEL(panel));
	pd = &panels_data[panel];

TRACEM("panel=%d\n",panel)

	if (!pd->uri_ndir) {
		file_list_abort (panel);
		g_return_if_fail (pd->uri_ndir == NULL);

		pd->uri_ndir = pd->uri_wdir;
	}
		
	g_return_if_fail (pd->uri_ndir != NULL);
	
	handle = pd->load_dir_handle;
	g_return_if_fail (handle != NULL);
	

	gnome_appbar_set_status (app_bar, _("Loading..."));

	gnome_vfs_uri_ref (pd->uri_ndir);
	
TRACE
	gnome_vfs_async_load_directory_uri ( handle, pd->uri_ndir, 
		GNOME_VFS_FILE_INFO_DEFAULT
			|| GNOME_VFS_FILE_INFO_GET_MIME_TYPE
			|| GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE
			|| GNOME_VFS_FILE_INFO_FOLLOW_LINKS
		,
		
		20, 	/* items per notification */
		GFM_VFS_PRIORITY_LOADDIR, 	/* priority */
		load_dir_callback,
		GINT_TO_POINTER(panel));
TRACE
}



/*
 * 	file_list_toggle_hidden_files()
 */
void
file_list_toggle_hidden_files (gint panel)
{
	
	g_return_if_fail (IS_PANEL(panel));
	if (!panels_data[panel].has_hidden_files)
		return;

	file_list_update (panel);
#if 0 
	if (show_hidden_files (panel))
		file_list_update (panel);
	else {
		/* doesn't work */
		GtkTreeModel *model = GTK_TREE_MODEL(panels_data[panel].files_data);
		GtkTreeIter iter;
		GfmRowData *row_data;
		GnomeVFSFileInfo *info;

		if (!gtk_tree_model_get_iter_first (model, &iter))
			return;
		
		while (iter.stamp == GTK_LIST_STORE(model)->stamp) { /* [ugly hack:] check if iter is still valid */
			gtk_tree_model_get (model, &iter, GFM_COLUMN_DATA, &row_data, -1);
			g_return_if_fail (row_data != NULL);
			info = row_data->info;
			g_return_if_fail (info != NULL);

			if (info->name[0] == '.' && info->name[2] != '\0') {/* hidden file, but not '..' */
				TRACEM("%s\n", info->name);
				gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
			}
			gfm_row_data_free (row_data);
		}
	}
#endif 
}




/******************************************8
 *
 * EVENT HANDLING
 *
 */

void
on_file_list_destroy                   (GtkObject       *object,
                                        gpointer         user_data)
{
	gint panel = get_panel_id (object);

	file_list_abort (panel);

	if (panels_data[panel].load_dir_handle)
		free (panels_data[panel].load_dir_handle);
	if (panels_data[panel].uri_wdir)
		gnome_vfs_uri_unref (panels_data[panel].uri_wdir);
	if (panels_data[panel].uri_ndir)
		gnome_vfs_uri_unref (panels_data[panel].uri_ndir);

}

gboolean
on_file_list_button_press_event (GtkTreeView *flist, GdkEventButton *event, gpointer data) 
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (flist), FALSE);
	

	if (event -> type != GDK_2BUTTON_PRESS) {
		return FALSE;
	}
	

	if (event -> window != gtk_tree_view_get_bin_window (flist)) 
		return FALSE;
	
	if (!gtk_tree_view_get_path_at_pos (flist, event->x, event->y, &path, &column, NULL,NULL))
		return FALSE;
	
	execute_default_action (flist, path);
	gtk_tree_path_free (path);

	return TRUE;
	
}


/** handling keystrokes **/

static void
focus_wd_entry (gint panel)
{
	GtkWidget *widget;

	g_return_if_fail (IS_PANEL(panel));
	widget = gnome_entry_gtk_entry(GNOME_ENTRY(panels_data[panel].wdir_entry));
	g_return_if_fail (widget != NULL);

	gtk_widget_grab_focus(widget);
	gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);	

}

gboolean
on_file_list_key_press_event           (GtkTreeView     *flist,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeModel *model;
	GnomeVFSURI *uri;
	gint panel; 

	g_return_val_if_fail (GTK_IS_TREE_VIEW (flist), FALSE);
	model = gtk_tree_view_get_model (flist);
	panel = get_panel_id (flist);

	switch (event->keyval) {
		case GDK_Return: 
			gtk_tree_view_get_cursor (flist, &path, NULL);
			execute_default_action (flist, path);
			break;

		case GDK_BackSpace:
			if (!gnome_vfs_uri_has_parent (panels_data[panel].uri_wdir))
				return FALSE;
			uri = gnome_vfs_uri_get_parent (panels_data[panel].uri_wdir);
			file_list_chdir (panel, uri);
			gnome_vfs_uri_unref (uri);
			break;


		case GDK_F3:
			switch (event->state) {
			  case 0:
				  lister_view_from_tree_view (flist);
				  break;
			  default:
				  return FALSE;
			}

			break;
		case GDK_F4:
			switch (event->state) {
			  case 0:
				gtk_tree_view_get_cursor (flist, &path, NULL);
				edit_with_default_editor (flist, path);
				gtk_tree_path_free (path);
				break;
			  default:
				  return FALSE;
			}

			break;
		case GDK_F5:
			switch (event->state) {
			  case 0:
				copy_or_move (panel, FALSE);
				break;
			  case GDK_CONTROL_MASK:
				create_symlink (panel);
				break;
			  default:
				return FALSE;
			}
			break;
		case GDK_F6: 
			copy_or_move (panel, TRUE);
			break;
		case GDK_F7:
			gfm_mkdir (panel);
			break;
		case GDK_F8:
			delete_files (panel);
			break;
		case GDK_R:
		case GDK_r:
			if (event->state != GDK_CONTROL_MASK) return FALSE;
			file_list_update (panel);
			break;
		case GDK_C:
		case GDK_c:
			switch (event->state) { 
				case GDK_CONTROL_MASK: 
					file_list_abort (panel); /* here will be 'copy_selection' */
					break;
				case GDK_MOD1_MASK:
					focus_wd_entry (panel);
					break;

				default:
					return FALSE;
			}
			break;
		case GDK_plus:
			selections_select_all (flist);
			break;
		case GDK_minus:
			selections_unselect_all (flist);
			break;
		case GDK_asterisk:
			selections_invert_selection (flist);
			break;

		case GDK_Tab:
			if (event->state != 0 && event->state != GDK_SHIFT_MASK) return FALSE;
			gtk_widget_grab_focus (GTK_WIDGET(panels_data[OTHER_PANEL(panel)].files_view));
			break;

		case GDK_F1:
			if (event->state != GDK_CONTROL_MASK) return FALSE;
			focus_wd_entry (PANEL_LEFT);
			break;
		case GDK_F2:
			if (event->state != GDK_CONTROL_MASK) return FALSE;
			focus_wd_entry (PANEL_RIGHT);
			break;
		case GDK_Escape:
			file_list_abort (panel);
			break;
		default:
			return FALSE;
			
	}
			
	if (path)
		gtk_tree_path_free (path);
	return TRUE;
}


/************************************
 * 
 * 	load_dir_callback()
 *
 ************************************/

/* 
 * grabs focus to our file-list, but only from first callback
 * (user can change focus during loading of large directory over slow line
 */

static void
flist_grab_focus (gboolean *grab_focus, PanelData *pd);

static void
load_dir_callback      (GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			GList *list,
			guint entries_read,
			gpointer callback_data)
{

	PanelData *pd;
	gint panel = GPOINTER_TO_INT(callback_data);
	GList *l;
	GnomeVFSFileInfo *info;
	GfmRowData *row_data;
	gboolean grab_focus = FALSE;
	gboolean _show_hidden_files;

	g_return_if_fail (IS_PANEL(panel));
	pd = &panels_data[panel];

#if DEBUG
puts("Entering load_dir_callback");
#endif 

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		if (pd->uri_ndir) {
			/** oops, we can't chdir **/
			gnome_vfs_uri_unref (pd->uri_ndir);
			wd_entry_set_text (panel);
			pd->uri_ndir = NULL;
#if DEBUG  
puts("	Ooops, can't chdir");
#endif 
		}

		/** handle errors **/
		report_gnome_vfs_error (result, TRUE);
#if DEBUG 
puts(gnome_vfs_result_to_string(result));
#endif
		return;
	} 

	if (pd->uri_ndir) {	/* we've just started */

		pd->has_hidden_files = FALSE;
		private_data [panel].dir_perm = 0777;
		special_files_init (panel);

#if DEBUG 
puts("	clearing file_list");
#endif 
		file_list_tips_disable ();
		gtk_list_store_clear (pd->files_data);

	
		if (pd->uri_wdir) {
#if DEBUG  
puts("	unref uri_wdir");
#endif 
			gnome_vfs_uri_unref (pd->uri_wdir);
		}

		pd->uri_wdir = pd->uri_ndir;
		pd->uri_ndir = NULL;
		

		if (gnome_vfs_uri_has_parent (pd->uri_wdir) && (info = gnome_vfs_file_info_new())) {
#if DEBUG 
puts("	adding '..'");
#endif 
			info->name="..";
			info->valid_fields = GNOME_VFS_FILE_INFO_FIELDS_TYPE | GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
			info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
			info->mime_type = "x-directory/normal";
			info->uid = 0; /* TODO: hmm, it seems like we have to call get_file_info_uri() */
			info->gid = 0; 

			row_data = gfm_row_data_new ();
			row_data->info = info;
			gnome_vfs_file_info_ref (info);
			row_data->real_info = NULL;

			append_file_to_flist (panel, row_data);
		}
		
		
#if DEBUG  
puts("	updating visuals");
#endif
		wd_entry_set_text (panel);
		dfree_set_text (panel);
		grab_focus = TRUE;
	}

	g_return_if_fail (GTK_IS_LIST_STORE(pd->files_data));
	
#if DEBUG  
puts("	populating list store:");
#endif 
	/** fill list_store **/
	_show_hidden_files = show_hidden_files (panel);
	g_object_freeze_notify (G_OBJECT(pd->files_view));
	for (l = list; l; l = l->next) {
		const gchar *name;
		
		info = GNOME_VFS_FILE_INFO (l->data);
		name = (info->name);

		if (!strcmp (name, ".")) {
			if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)
				private_data[panel].dir_perm = info->permissions;
			continue;
		}

		if (!strcmp (name, "..")) {

#if 0 
			GtkTreeIter iter;
			
			check_set_default_mime_type (info);

			/* this doesn't work, since '..' isn't always first */
			gtk_tree_model_get_iter_first (GTK_TREE_MODEL(pd->files_data), &iter);
			gtk_tree_model_get (GTK_TREE_MODEL (pd->files_data), &iter, GFM_COLUMN_DATA, &row_data, -1);
			
			TRACE
			row_data = gfm_row_data_new ();
			row_data->info = info;
			gnome_vfs_file_info_ref (info);
			row_data->real_info = NULL;
			
			TRACE
			gtk_list_store_set (pd->files_data, &iter, GFM_COLUMN_DATA, row_data, -1);
			
			TRACE
			gfm_row_data_free (row_data);
#endif 			
			continue;
		}


		if (name[0]=='.') {
			pd->has_hidden_files = TRUE;
			if (!_show_hidden_files)
				continue;
		}

		check_set_default_mime_type (info);

		/*
		if (GFM_IS_EXECUTABLE(info)) {	
			info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
			if (info->mime_type) 
				free (info->mime_type);
			info->mime_type = strdup("application/x-executable");
		}	
		*/

		
		row_data = gfm_row_data_new ();
		row_data->info = info;
		gnome_vfs_file_info_ref (info);
		row_data->real_info = NULL;

#if DEBUG  
printf("		%s\n", info->name);
#endif 
		append_file_to_flist (panel, row_data);

		flist_grab_focus (&grab_focus, pd);

	}
	g_object_thaw_notify (G_OBJECT(pd->files_view));


	/** finished **/
	flist_grab_focus (&grab_focus, pd);
	if (result == GNOME_VFS_ERROR_EOF) {
#if DEBUG 
printf("finishing %s %s\n", pd->uri_wdir?"(wdir not null)":"", pd->uri_ndir?"(ndir not null)":"");
#endif 
		gnome_appbar_set_status (app_bar, "Done.");

		special_files_process_list (panel);

		return;
	}
#if DEBUG 
	puts("To be continued ...");
#endif 

}

static void
flist_grab_focus (gboolean *grab_focus, PanelData *pd) 
{
	GtkTreePath *path;

	if (!*grab_focus) return;
TRACE
	path = gtk_tree_path_new_first();
	gtk_widget_grab_focus (GTK_WIDGET(pd->files_view));
	gtk_tree_view_set_cursor (pd->files_view, path, NULL, FALSE);
	file_list_tips_enable ();
	*grab_focus = FALSE;

}


/*
 * append_file_to_flist ()
 */
static void
append_file_to_flist	(gint panel,
			GfmRowData *row_data)
{
	GtkTreeIter *iter = g_new (GtkTreeIter, 1);

TRACE
	gtk_list_store_append (panels_data[panel].files_data, iter);
	gtk_list_store_set (panels_data[panel].files_data, iter, GFM_COLUMN_DATA, row_data, -1);

	special_files_add_file_to_list (panel, iter, row_data);
}

/*
 * (old) stuff around showing mount point
 */
#if SHOW_MOUNT_POINT

#include <sys/stat.h>
#include <sys/statfs.h>


static gchar *
find_mount_point (const gchar *path) {
/* inspired by GNU df (disk free ?) */
/* TODO: use info->device or info->node somehow and skip this time-consuming routine */
	struct stat def, cur;
	gchar *p, *wpath;

	wpath = malloc (strlen(path)+2);
	strcpy (wpath, path);

	p = wpath + strlen(path) -1;
	if (*p != '/') {
		*(++p)='/';
		*(++p)=0;
	}
	
	g_return_val_if_fail (wpath != NULL, "");

	if (stat (path, &def)) { p=NULL; goto done; }


	do {
		while (p>=wpath && *p != '/') p--;
		p[0] = 0;

		if (p==wpath) { p[1]=0; break; }
		if (stat (wpath, &cur)) { p=NULL; goto done; }
	} while (cur.st_dev == def.st_dev);
	*p='/';
	
done:
	return (wpath);
}
#endif



/*
 * 	dfree_set_text()
 */
static void
dfree_set_text (gint panel) 
{
	gchar *text, *sz;
	GnomeVFSFileSize df;
	GnomeVFSResult result;
	
	g_return_if_fail (IS_PANEL(panel));
	g_return_if_fail (GTK_IS_LABEL (panels_data[panel].dfree));

#if DEBUG
puts("	calculating free space");
#endif
	result = gnome_vfs_get_volume_free_space (panels_data[panel].uri_wdir, &df);
	if (result != GNOME_VFS_OK) {
		gtk_label_set_text (panels_data[panel].dfree, "");
		TRACEM("error: %s\n", gnome_vfs_result_to_string (result));
		return;
	}

#if DEBUG
puts("		printing");
#endif
	sz = human_readable_file_size (df);
	text = g_strdup_printf(_("%s free"), sz);
	g_free(sz);

#if DEBUG
puts("		updating label");
#endif
	gtk_label_set_text (panels_data[panel].dfree, text);
	g_free (text);

#if DEBUG 
	puts("		updating tooltips");
#endif
	text = g_strdup_printf("%'" GNOME_VFS_SIZE_FORMAT_STR " %s", df, _(" bytes free"));
	gtk_tooltips_set_tip (tooltips, GTK_WIDGET (panels_data[panel].dfree),text, "");
	g_free (text);

#if DEBUG
puts("	done");
#endif
}

static gboolean
show_hidden_files	(gint panel)
{
	GtkCheckMenuItem *checkbox;

	checkbox = GTK_CHECK_MENU_ITEM (lookup_widget (main_window, "show-hidden-files"));
	g_return_val_if_fail (checkbox != NULL, TRUE);
	
	return gtk_check_menu_item_get_active (checkbox);
}

static void
gfm_mkdir		(gint panel)
{
	static GtkWidget *dialog;
	static GtkWidget *entry;
	GtkWidget *widget;
	GnomeVFSResult result = GNOME_VFS_OK;
	GnomeVFSURI *uri;
	gint response;
	
	if (!dialog) {
		dialog = gtk_dialog_new_with_buttons (_("Create directory"),
				GTK_WINDOW(main_window), /* parent */
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				_("Create"), GTK_RESPONSE_ACCEPT,
				NULL);
		if (!dialog) {
			result = GNOME_VFS_ERROR_NO_MEMORY;
			goto done;
		}

		gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
/*
		widget = gtk_button_new_with_label (_("Create"));
		if (!widget) {
			gtk_widget_destroy (dialog);
			dialog = NULL;
			result = GNOME_VFS_ERROR_NO_MEMORY;
			goto done;
		}
		
		gtk_dialog_add_action_widget (GTK_DIALOG(dialog), widget, GTK_RESPONSE_ACCEPT);
		gtk_widget_show(widget);
		GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
		gtk_widget_grab_default (widget);
*/		
		entry = gtk_entry_new ();
		if (!entry) {
			gtk_widget_destroy (dialog);
			dialog = NULL;
			result = GNOME_VFS_ERROR_NO_MEMORY;
			goto done;
		}
		gtk_box_pack_end (GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, TRUE, TRUE, 1);
		gtk_widget_show (entry);
		gtk_entry_set_activates_default (GTK_ENTRY(entry), TRUE);

		widget = gtk_label_new (_("Directory name:"));
		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 5);
		gtk_widget_show (widget);
	}

	gtk_entry_set_text (GTK_ENTRY(entry), "");
	gtk_widget_grab_focus (entry);
	
	response = gtk_dialog_run (GTK_DIALOG(dialog));
	gtk_widget_hide (dialog);

	if (response != GTK_RESPONSE_ACCEPT)
		goto done;

	gnome_appbar_set_status (app_bar, _("Creating directory"));
	uri = gnome_vfs_uri_append_file_name (
			panels_data[panel].uri_wdir, 
			gtk_entry_get_text (GTK_ENTRY(entry)));

	result = gnome_vfs_make_directory_for_uri (uri, private_data[panel].dir_perm);
	gnome_vfs_uri_unref (uri);

	if (result == GNOME_VFS_OK) {
		file_list_update (panel);
		if (gnome_vfs_uri_equal (panels_data[panel].uri_wdir, panels_data[OTHER_PANEL(panel)].uri_wdir))
			file_list_update (OTHER_PANEL(panel));
	}

done:
	if (result != GNOME_VFS_OK)
		report_gnome_vfs_error (result, TRUE);
	else 
		gnome_appbar_set_status (app_bar, _("Done."));
}
