#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

#include "copy_move_delete.h"
#include <stdlib.h>
#include "interface.h"
#include "gfm_vfs.h"
#include "gfm_list_store.h"
#include <glib.h>

#define ASYNC_JOB_PRIORITY 5

static GList *async_jobs = NULL;	/* list of XferCallbackData of currently running jobs*/

typedef struct {
	gint source_panel;
	GList *async_job;

	enum {
		XFER_COPY,
		XFER_MOVE,
		XFER_DELETE
	} action;
	enum {
		UPDATE_NONE,
		UPDATE_SOURCE = 1 << 0,
		UPDATE_TARGET = 1 << 1,
	} updates;
	
	GtkWidget
		*status_window,
		*error_dialog, *error_label,
		*replace_dialog, *replace_label;

	GnomeVFSAsyncHandle *async_handle;

	/** private stuff **/
	guint percents_done;
	GnomeVFSXferPhase last_phase;
	gulong last_file_index;
} XferCallbackData;

static const char *actions_desc[] = {"Copying","Moving", "Deleting"};

/*
 * 	SUPPORT
 */


// called periodically during transfer
// this callback updates progress dialog, choose which action shall be taken in case of overwrites, etc. 
static gint
xfer_progress_callback	       (GnomeVFSAsyncHandle *handle,
				GnomeVFSXferProgressInfo *info,
				XferCallbackData *xfer_data);

// returns full URL(s) of selected file(s) in active panel as source and URL translated to other panel as targets
// E.g. source: ["/home/user/a", "/home/user/b"] target: ["ftp://remote/a", "ftp://remote/b"]
// **list could be NULL (will be ignored)
static void
get_uri_lists_from_flist       (gint panel, 
				GList **source_list, 
				GList **target_list);

/*
 * prepares XferCallbackData -- creates progress dialog, initializes progress bars, etc.
 */
static void
xfer_init		       (XferCallbackData *cdata);

/*
 * cleans up XferCallbackData, destroys dialogs, etc.
 */
static void
xfer_cleanup		       (XferCallbackData *cdata);

/*
 * called when user clicks on "Cancel" or closes dialog window (?)
 * -> stops async gnome-vfs job
 */
static void
xfer_dialog_on_response_event	(GtkWidget *widget, 
				gint response, 
				XferCallbackData *data);

static void
create_error_dialog		(XferCallbackData *cdata);

static void
create_replace_dialog		(XferCallbackData *cdata);



/****************************
 *
 * 	MAIN
 *
 */

//static GtkWidget *xfer_progress_dialog = NULL;


void
copy_or_move_uri	       (GList *source_uri_list,
				GList *target_uri_list,
				gint source_panel,
				gboolean move)
{
	GnomeVFSURI *source_dir = NULL, *target_dir = NULL;
	const GList *p;
	XferCallbackData *callback_data;
	GnomeVFSResult result;

	g_return_if_fail (IS_PANEL(source_panel));
	
	callback_data = malloc (sizeof (XferCallbackData));
	if (!callback_data) {
		 report_gnome_vfs_error (GNOME_VFS_ERROR_NO_MEMORY, TRUE);
		 return;
	}

	callback_data->source_panel = source_panel;
	callback_data->action = move ? XFER_MOVE : XFER_COPY;

	callback_data->updates = UPDATE_NONE;
	if (move) {
		source_dir = panels_data[source_panel].uri_wdir;
		if (source_dir)
		for (p = source_uri_list; p; p = p->next) 
			if (gnome_vfs_uri_is_parent (source_dir, (GnomeVFSURI *)p->data, FALSE)) {
				callback_data->updates |= UPDATE_SOURCE;
				break;
			}
	}
	
	/** find out which file_lists should be updated **/
  	target_dir = (GnomeVFSURI *) panels_data[OTHER_PANEL(source_panel)].uri_wdir;

	for (p = target_uri_list; p; p = p->next) {
		if (target_dir && gnome_vfs_uri_is_parent (target_dir, 
					(GnomeVFSURI *)p->data, 
					FALSE)) 
			callback_data->updates |= UPDATE_TARGET;
		if (source_dir && 
				gnome_vfs_uri_is_parent (source_dir, 
					(GnomeVFSURI *)p->data, 
					FALSE)) 
			callback_data->updates |= UPDATE_SOURCE;
		if (	(!source_dir || callback_data->updates & UPDATE_SOURCE) &&
			(!target_dir || callback_data->updates & UPDATE_TARGET)) break;
	}
	
	xfer_init (callback_data);

	result =  gnome_vfs_async_xfer (
			&callback_data->async_handle,
			source_uri_list,
			target_uri_list,
			GNOME_VFS_XFER_RECURSIVE 
				| (move ? GNOME_VFS_XFER_REMOVESOURCE : 0)
			,
			GNOME_VFS_XFER_ERROR_MODE_QUERY,
			GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
			ASYNC_JOB_PRIORITY,
			/* async callback */
			(GnomeVFSAsyncXferProgressCallback) xfer_progress_callback, 
			callback_data,
			/* sync callback */
			NULL, 
			NULL);

	if (result != GNOME_VFS_OK)
		report_gnome_vfs_error (result, TRUE);
}

void
copy_or_move		       (gint source_panel,
				gboolean move)
{
	GList *source, *target;
	
	get_uri_lists_from_flist (source_panel, &source, &target);
	
	if (!source || !target) return;

	copy_or_move_uri (source, target, source_panel, move);
}


void
delete_uri		       (GList *uri_list,
				gint source_panel)
{
	XferCallbackData *callback_data;
	GnomeVFSResult result;
	GnomeVFSURI *source_dir = NULL, *target_dir = NULL;

TRACEM("enter");
	callback_data = malloc (sizeof (XferCallbackData));
	if (!callback_data) {
		report_gnome_vfs_error (GNOME_VFS_ERROR_NO_MEMORY, TRUE);
		return;
	}

	callback_data->source_panel = source_panel;
	callback_data->action = XFER_DELETE;
	callback_data->status_window = NULL; 
	callback_data->updates = UPDATE_SOURCE;

	source_dir = panels_data[source_panel].uri_wdir;
	
	if (!source_dir) goto skip;
	
	target_dir = panels_data[OTHER_PANEL(source_panel)].uri_wdir;
	if (!target_dir) goto skip;

	if (gnome_vfs_uri_equal (source_dir, target_dir))
		callback_data->updates |= UPDATE_TARGET;

skip:

	xfer_init (callback_data);

	result =  gnome_vfs_async_xfer (
			&callback_data->async_handle,
			uri_list,
			NULL,
			GNOME_VFS_XFER_DELETE_ITEMS | GNOME_VFS_XFER_RECURSIVE,
			GNOME_VFS_XFER_ERROR_MODE_QUERY,
			GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
			ASYNC_JOB_PRIORITY,
			(GnomeVFSAsyncXferProgressCallback) xfer_progress_callback, /* async callback */
			callback_data,
			NULL, /* sync callback */
			NULL);

	if (result != GNOME_VFS_OK)
		report_gnome_vfs_error (result, TRUE);
}

void
delete_files		       (gint source_panel)
{
	GList *list;
	
	get_uri_lists_from_flist (source_panel, &list, NULL);
	
	delete_uri (list, source_panel);
}



/****************************
 *
 * 	SUPPORT
 *
 */

static gint
xfer_progress_callback	       (GnomeVFSAsyncHandle *handle,
				GnomeVFSXferProgressInfo *info,
				XferCallbackData *xfer_data)
{

	GtkWidget *progress_dialog = NULL;
	GtkLabel *label_desc;
	gchar *desc = NULL;
	gint return_code = TRUE;
	const gchar *message_string = _("Source: %s\nTarget: %s\n");

#define error_dialog (xfer_data->error_dialog)
#define replace_dialog (xfer_data->replace_dialog)

	g_return_val_if_fail (xfer_data != NULL, FALSE);

	progress_dialog = xfer_data -> status_window;
	if (!progress_dialog) 
		return FALSE; 

	/** update progess_dialog **/
	label_desc = GTK_LABEL (lookup_widget (progress_dialog, "label_desc"));

	g_return_val_if_fail (label_desc != NULL, FALSE);

	/** update status description **/
	
	if (info->phase == GNOME_VFS_XFER_PHASE_INITIAL
	    || info->phase != xfer_data->last_phase 
	    || info->file_index != xfer_data->last_file_index
	) switch (info->phase) {
#define set_desc(D) {\
	gtk_label_set_text (label_desc, D);\
	TRACEM("Setting label to %s\n", D);\
}
		case GNOME_VFS_XFER_PHASE_INITIAL:
			set_desc (_("Preparing ..."));
			xfer_data->percents_done = 0;
			xfer_data->last_phase = info->phase;
			xfer_data->last_file_index = info->file_index;
			break;
		case GNOME_VFS_XFER_CHECKING_DESTINATION:
			set_desc (_("Checking destination ..."));
			break;
		case GNOME_VFS_XFER_PHASE_COLLECTING:
			set_desc (_("Collecting files ..."));
			break;
		case GNOME_VFS_XFER_PHASE_READYTOGO:
			set_desc (_("Ready"));
			break;
		case GNOME_VFS_XFER_PHASE_COPYING:
		case GNOME_VFS_XFER_PHASE_MOVING:
			desc = g_strdup_printf (message_string, 
					info->source_name,
					info->target_name);
			set_desc (desc?:_("Out of memory."));
			if (desc) free (desc);
			break;
		case GNOME_VFS_XFER_PHASE_DELETESOURCE:
			desc = g_strdup_printf (message_string, 
					info->source_name,
					"");
			set_desc (desc?:_("Out of memory."));
			if (desc) free (desc);
			break;
			
		case GNOME_VFS_XFER_PHASE_CLOSESOURCE:
		case GNOME_VFS_XFER_PHASE_CLOSETARGET:
			set_desc (_("Closing source & target"));
			if (progress_dialog) 
				gtk_progress_bar_set_fraction (
					GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "current_pbar")),
					1.0);
			break;
		case GNOME_VFS_XFER_PHASE_COMPLETED:
			set_desc (_("Completed"));
			if (!progress_dialog) break;

			gtk_progress_bar_set_fraction (
				GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "current_pbar")), 1.0);
			gtk_progress_bar_set_fraction (
				GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "files_pbar")), 1.0);
			gtk_progress_bar_set_fraction (
				GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "bytes_pbar")), 1.0);
			xfer_cleanup (xfer_data);
			return FALSE;
			break;
		default:
		/** well, it seems like not all phases are reported,
		 * so may be this will be implemented later */
#undef set_desc
	}

//	gtk_widget_draw (progress_dialog, progress_dialog->allocation);

	if (info->bytes_copied <= info->file_size)/* because of bug in 'gnome_vfs'*/
	gtk_progress_bar_set_fraction (
		GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "current_pbar")),
		info->file_size ? (1.0*info->bytes_copied/info->file_size) : 0.0);
	gtk_progress_bar_set_fraction (
		GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "files_pbar")),
		info->files_total ? (1.0*((info->file_index?:1)-1)/info->files_total) : 0.0);
	gtk_progress_bar_set_fraction (
		GTK_PROGRESS_BAR (lookup_widget (progress_dialog, "bytes_pbar")),
		info->bytes_total ? (1.0*info->total_bytes_copied/info->bytes_total) : 0.0);

	
	if (info->bytes_total) {
		gchar title[50];
		title[49] = '\0';

		if (xfer_data->percents_done != 100*(info->total_bytes_copied)/(info->bytes_total)) {
			xfer_data->percents_done = 
				100*(info->total_bytes_copied)/(info->bytes_total);
			snprintf(title, 99, _("%s (%2d%% done)"),
				actions_desc[xfer_data->action],
				xfer_data->percents_done);
			
			gtk_window_set_title (GTK_WINDOW(progress_dialog), title);
		}
	}
	

/**
	printf("\ntransfering %s to %s\n", info->source_name, info->target_name);
	printf("Status:%02d Phase:%02d Files:%03lu/%03lu Bytes:%03"GNOME_VFS_SIZE_FORMAT_STR"/%03"GNOME_VFS_SIZE_FORMAT_STR" BTotal:%03"GNOME_VFS_SIZE_FORMAT_STR"/%03"GNOME_VFS_SIZE_FORMAT_STR" (%2.2f%%)\n",
			info->status, info->phase, info->file_index, info->files_total,
			info->bytes_copied, info->file_size, info->total_bytes_copied,  info->bytes_total,
			1.0*info->total_bytes_copied/info->bytes_total);
**/

/** TODO: handle errors & overwrites **/
	switch (info->status) {
		case GNOME_VFS_XFER_PROGRESS_STATUS_OK: 
			/* everything is OK */ 
			break; 
		case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
			TRACEM("Error: %s\n", gnome_vfs_result_to_string (info->vfs_status));

			if (!error_dialog)  {
				create_error_dialog (xfer_data);
			}
			if (!error_dialog) { 
				return_code = GNOME_VFS_XFER_ERROR_ACTION_ABORT;
				break; 
			}

			gtk_label_set_text (
				GTK_LABEL(xfer_data->error_label),
				gnome_vfs_result_to_string (info->vfs_status));
			return_code = gtk_dialog_run (GTK_DIALOG(error_dialog));
			if (return_code<0) 
				return_code = GNOME_VFS_XFER_OVERWRITE_ACTION_ABORT;
			break;
		case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
			TRACEM("Overwrite?");
			
			if (!replace_dialog)
				create_replace_dialog (xfer_data);
			if (!replace_dialog) { 
				return_code = GNOME_VFS_XFER_OVERWRITE_ACTION_ABORT; 
				break; 
			}

			desc = g_strdup_printf (message_string,
					info->source_name,
					info->target_name);
			if (desc) {
				gtk_label_set_text (
					GTK_LABEL(xfer_data->replace_label),
					desc);
				free (desc);
				return_code = gtk_dialog_run (GTK_DIALOG(replace_dialog));
			} else 
				return_code = GNOME_VFS_XFER_OVERWRITE_ACTION_ABORT;
			
			if (return_code<0) 
				return_code = GNOME_VFS_XFER_OVERWRITE_ACTION_ABORT;
			break;
		case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
			{ GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (GTK_WINDOW(progress_dialog), 
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					_("Oooops! Panic !\nGnome VFS Xfer Progress Status: Duplicate\n"));
			gtk_dialog_run (GTK_DIALOG(dialog));
			gtk_widget_destroy (dialog);
			}
			
			return_code = FALSE;
			break;
	}
			
			
#if DEBUG 
	if (info->status == 1)
		printf("Error: %s\n", gnome_vfs_result_to_string (info->vfs_status));
#endif 

	xfer_data->last_phase = info->phase;
	xfer_data->last_file_index = info->file_index;

	return return_code;
#undef error_dialog
#undef replace_dialog
}


/*
 * get_uri_lists_from_flist
 */
struct SelectionForEachData {
	GnomeVFSURI *source_uri, *target_uri;
	GList **source_list, **target_list;
};

static void
selection_for_each_function	(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, struct SelectionForEachData *data)
{
	GfmRowData *row_data;
	GnomeVFSFileInfo *info;
	
	gtk_tree_model_get (model, iter, GFM_COLUMN_DATA, &row_data, -1);
	if (!row_data)
		g_assert_not_reached ();
	info = row_data->info;

	if (!strcmp(info->name, ".."))
		return;

#define add(what) \
	if (data->what##_uri)\
		*data->what##_list = g_list_prepend (\
				*data->what##_list,\
				gnome_vfs_uri_append_file_name (\
					data->what##_uri,\
					info->name\
				)\
		)
	add(source);
	add(target);
#undef add
	
}

static void
get_uri_lists_from_flist (gint panel, GList **source_list, GList **target_list)
{
	GnomeVFSURI *source_uri = NULL, *target_uri = NULL;
	struct SelectionForEachData data;

	g_return_if_fail (IS_PANEL(panel));

	if (source_list) {
		data.source_uri = panels_data[panel].uri_wdir;
		*source_list = NULL;
	} else data.source_uri = NULL;

	if (target_list) {
		data.target_uri = panels_data[OTHER_PANEL(panel)].uri_wdir;
		*target_list = NULL;
	} else data.target_uri = NULL;

	data.source_list = source_list;
	data.target_list = target_list;

	gtk_tree_selection_selected_foreach (
		gtk_tree_view_get_selection(panels_data[panel].files_view), 
		(GtkTreeSelectionForeachFunc) selection_for_each_function, 
		&data);

	if (source_uri)
		*source_list =  g_list_reverse (*source_list);
	if (target_uri)
		*target_list =  g_list_reverse (*target_list);
}

static void
update_file_lists	       (XferCallbackData *cdata)
{

	g_return_if_fail (cdata != NULL);
	g_return_if_fail (IS_PANEL(cdata->source_panel));
	
	TRACEM("Entering update_file_list");

	if (cdata->updates & UPDATE_TARGET)  {
		file_list_update (OTHER_PANEL(cdata->source_panel));
	}

	if (cdata->updates & UPDATE_SOURCE) {
		file_list_update (cdata->source_panel);
	}

	gtk_widget_grab_focus (GTK_WIDGET(panels_data[cdata->source_panel].files_view));
	
	TRACEM("Leaving update_file_lists.");
}	

static void
xfer_init   (XferCallbackData *cdata) {
	
	GtkWidget *xfer_progress_dialog;

	async_jobs = g_list_prepend (async_jobs, cdata);
	cdata->async_job = async_jobs;
	
	cdata->error_dialog = NULL;
	cdata->replace_dialog = NULL;

	/* create status dialog */
	xfer_progress_dialog = create_widget ("xfer_progress_dialog");
	
	g_return_if_fail ((cdata->status_window = xfer_progress_dialog));
	
	gtk_window_set_title (GTK_WINDOW (cdata->status_window), actions_desc[cdata->action]);

	gtk_label_set_text (
		GTK_LABEL(lookup_widget (cdata->status_window, "label_desc")),
		_("Initializing ..."));

	gtk_progress_bar_set_fraction (
		GTK_PROGRESS_BAR(lookup_widget (cdata->status_window, "current_pbar")),
		0);
	gtk_progress_bar_set_fraction (
		GTK_PROGRESS_BAR(lookup_widget (cdata->status_window, "files_pbar")),
		0);
	gtk_progress_bar_set_fraction (
		GTK_PROGRESS_BAR(lookup_widget (cdata->status_window, "bytes_pbar")),
		0);
	
	if (cdata->status_window) {
		gtk_widget_show (cdata->status_window);
		gtk_widget_grab_focus (cdata->status_window);
	}

	g_signal_connect (cdata->status_window, "response", (GCallback) xfer_dialog_on_response_event, cdata);
	
}

static void
xfer_cleanup		       (XferCallbackData *cdata)
{
	
	g_return_if_fail (cdata != NULL);
	
	TRACEM("Entering xfer_cleanup");

	update_file_lists (cdata);
	
	async_jobs = g_list_remove_link (async_jobs, cdata->async_job);
	g_list_free (cdata->async_job);

	if (cdata->status_window) {
		gtk_widget_destroy (cdata->status_window);
	}

	if (cdata->error_dialog) {
		gtk_widget_destroy (cdata->error_dialog);
	}

	if (cdata->replace_dialog) {
		gtk_widget_destroy (cdata->replace_dialog);
	}

	free (cdata);
	
	TRACEM("Leaving xfer_cleanup");

}

static void
xfer_dialog_on_response_event	(GtkWidget *widget, 
				gint response, 
				XferCallbackData *data)
{
	TRACE
	g_return_if_fail (data != NULL);
	
	TRACEM("	stopping");
	

	gnome_vfs_async_cancel (data->async_handle);
	xfer_cleanup (data);

	return;
}

void
copy_move_delete_shutdown	(void)
{
	TRACE
	
	while (async_jobs) {
		TRACE
		gnome_vfs_async_cancel (((XferCallbackData *)async_jobs->data)->async_handle);
		xfer_cleanup ((XferCallbackData *)async_jobs->data);
	}
	TRACE
}

static void
create_error_dialog		(XferCallbackData *cdata)
{
	GtkWidget *dialog, *label;

	g_return_if_fail (cdata != NULL);
	dialog = gtk_dialog_new_with_buttons ("Error", GTK_WINDOW(main_window), 0, 
			GTK_STOCK_CANCEL, GNOME_VFS_XFER_ERROR_ACTION_ABORT,
			"Retry", GNOME_VFS_XFER_ERROR_ACTION_RETRY,
			"Skip", GNOME_VFS_XFER_ERROR_ACTION_SKIP,
			NULL);
	label = gtk_label_new ("");
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
	g_signal_connect_swapped (GTK_OBJECT (dialog), 
		"response", 
		G_CALLBACK (gtk_widget_hide),
		GTK_OBJECT (dialog));	
	
	cdata->error_dialog = dialog;
	cdata->error_label = label;
}

static void
create_replace_dialog		(XferCallbackData *cdata)
{
	GtkWidget *dialog, *label;

	g_return_if_fail (cdata != NULL);
	dialog = gtk_dialog_new_with_buttons (_("Error"), GTK_WINDOW(main_window), 0, 
			GTK_STOCK_CANCEL, GNOME_VFS_XFER_OVERWRITE_ACTION_ABORT,
			_("Replace"), GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE,
			_("Replace All"), GNOME_VFS_XFER_OVERWRITE_ACTION_REPLACE_ALL,
			_("Skip"), GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP,
			_("Skip All"), GNOME_VFS_XFER_OVERWRITE_ACTION_SKIP_ALL,
			NULL);
	label = gtk_label_new (_("File exists"));
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
	g_signal_connect_swapped (GTK_OBJECT (dialog), 
		"response", 
		G_CALLBACK (gtk_widget_hide),
		GTK_OBJECT (dialog));	

	cdata->replace_dialog = dialog;
	cdata->replace_label = label;
}

/*
 * symlinks
 */
static void
create_symlink_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			GnomeVFSURI *uri);

void
create_symlink_uri		(const GnomeVFSURI *target_reference,
				GnomeVFSURI *uri)
{
	char *target;
	GnomeVFSAsyncHandle *handle;
	
	g_return_if_fail (target_reference != NULL);
	g_return_if_fail (uri != NULL);

TRACE
	target = gnome_vfs_uri_to_string (target_reference, GNOME_VFS_URI_HIDE_NONE);
	g_return_if_fail (target != NULL);
TRACE
	gnome_vfs_async_create_symbolic_link (&handle, uri, target, GFM_VFS_PRIORITY_XFER, 
			(GnomeVFSAsyncCreateCallback) create_symlink_callback, uri);
TRACE
	g_free (target);
}

void
create_symlink			(gint source_panel)
{
	GnomeVFSURI *uri, *target;
	GtkTreePath *path;
	GtkTreeIter iter;
	GfmRowData *row_data = NULL;
	
	g_return_if_fail (IS_PANEL (source_panel));
TRACE
	if (gnome_vfs_uri_equal (panels_data[source_panel].uri_wdir, 
				panels_data[OTHER_PANEL(source_panel)].uri_wdir)) {
		report_gnome_vfs_error (GNOME_VFS_ERROR_FILE_EXISTS, TRUE);
		return;
	}

TRACE
	gtk_tree_view_get_cursor (panels_data[source_panel].files_view, &path, NULL);
	g_return_if_fail (gtk_tree_model_get_iter (GTK_TREE_MODEL(panels_data[source_panel].files_data), &iter, path));
	gtk_tree_path_free (path);

	gtk_tree_model_get (GTK_TREE_MODEL(panels_data[source_panel].files_data), &iter, GFM_COLUMN_DATA, &row_data, -1);
	g_return_if_fail (row_data != NULL);

TRACE
	target = gnome_vfs_uri_append_file_name (panels_data[source_panel].uri_wdir, row_data->info->name);
	uri = gnome_vfs_uri_append_file_name (panels_data[OTHER_PANEL(source_panel)].uri_wdir, row_data->info->name);
	gfm_row_data_free (row_data); row_data = NULL;

TRACE
	create_symlink_uri (target, uri);

TRACE
	gnome_vfs_uri_unref (uri);
	gnome_vfs_uri_unref (target);
}

static void
create_symlink_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			GnomeVFSURI *uri)
{
	GnomeVFSURI *parent;
	gint panel;
	
TRACE
	if (result != GNOME_VFS_OK) {
		report_gnome_vfs_error (result, TRUE);
		return;
	}

	report_gnome_vfs_error (result, FALSE);

	g_return_if_fail (gnome_vfs_uri_has_parent (uri));
	g_return_if_fail ((parent = gnome_vfs_uri_get_parent (uri)));

	for (panel = PANEL_LEFT; panel < NUM_PANELS; panel++) 
		if (gnome_vfs_uri_equal (parent, panels_data[panel].uri_wdir))
			file_list_update (panel);
	gnome_vfs_uri_unref (parent);
}
