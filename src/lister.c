#include "interface.h"
#include "lister.h"
#include "gfm_list_store.h"
#include "gfm_vfs.h"
#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *
create_lister_window	(const gchar *title);

/* view text */
static void
view_text		(GtkWidget *window, GnomeVFSURI *uri);

/*
 * handlers for asynchronous reading of viewed file
 */
static void
text_open_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer cdata);
static void
text_read_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer buffer,
			GnomeVFSFileSize bytes_requested,
			GnomeVFSFileSize bytes_read,
			gpointer cdata);
static void
text_close_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer cdata);

/*************************************
 *
 * 	main
 *
 *************************************/

void
lister_view_uri		(GnomeVFSURI *uri, const gchar *mime_type)
{
	GtkWidget *lister;
	gchar *title, *text_uri;
	
	g_return_if_fail (uri != NULL);
	g_return_if_fail (mime_type != NULL);

	if (strncmp(mime_type, "text/", 5)) {
		g_warning (G_STRLOC": Not implemented yet");
		return;
	}

	text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
	g_return_if_fail (text_uri);
	
	title = g_strdup_printf ("Lister - %s", text_uri);
	g_free (text_uri);
	g_return_if_fail (title);
	
	lister = create_lister_window(title);
	g_free (title);

	view_text (lister, uri);
}

void
lister_view_from_tree_view	(GtkTreeView *tree_view)
{
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GfmRowData *row_data = NULL;
	GnomeVFSURI *uri = NULL;
	GtkTreeModel *model;
	
	
	g_return_if_fail (tree_view != NULL);
	gtk_tree_view_get_cursor (tree_view, &path, NULL);
	model = gtk_tree_view_get_model (tree_view);
	if (!model) goto done;

	if (!gtk_tree_model_get_iter (model, &iter, path)) goto done;

	gtk_tree_model_get (model, &iter, GFM_COLUMN_DATA, &row_data, -1);
	if (!row_data) goto done;

	uri = gnome_vfs_uri_append_file_name (panels_data[get_panel_id(tree_view)].uri_wdir, row_data->info->name);
	if (!uri) goto done;

	lister_view_uri (uri, row_data->info->mime_type);
	
done:
	if (path)
		gtk_tree_path_free (path);
	if (row_data)
		gfm_row_data_free (row_data);
	if (uri)
		gnome_vfs_uri_unref (uri);
}

static GtkWidget *
create_lister_window	(const gchar *title)
{
	GtkWidget *window;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_return_val_if_fail (window!=NULL, window);

	g_object_set (G_OBJECT(window),
		"title", title,
		"resizable", TRUE,
		"allow-grow", TRUE,
		"allow-shrink", TRUE,
		"destroy-with-parent", TRUE,
		"default-width", 640,
		"default-height", 480,
		/* TODO: "icon", lister_icon */
		NULL);
	
/*
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (GTK_WINDOW_GET_CLASS(window)), 
		GDK_Escape, 0, "delete", 0);
*/			
	return window;
}


/*************************************
 *
 * 	view text
 *
 *************************************/


static void
view_text		(GtkWidget *window, GnomeVFSURI *uri)
{
	GtkWidget *view, *swin;
	GtkTextBuffer *buffer;
	GnomeVFSAsyncHandle *handle;
	

	g_return_if_fail (window);

	swin = gtk_scrolled_window_new (NULL, NULL);
	if (!swin) goto error;
	gtk_container_add (GTK_CONTAINER(window), swin);
	gtk_widget_show (swin);


	view = gtk_text_view_new ();
	if (!view) goto error;
	g_object_set (G_OBJECT(view),
		"editable", FALSE,
		"cursor-visible", FALSE,
		NULL);
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(view));
	gtk_text_buffer_set_text (buffer, "", 1);

	gtk_container_add (GTK_CONTAINER(swin), view);

	gnome_vfs_async_open_uri (&handle, uri, GNOME_VFS_OPEN_READ, GFM_VFS_PRIORITY_LISTER, text_open_callback, buffer);
	g_object_set_data_full (G_OBJECT(view), "async-handle", handle, (GDestroyNotify) gnome_vfs_async_cancel);

	gtk_widget_show (view);
	gtk_widget_show (window);
	return;

error:
	g_object_unref (G_OBJECT(window));
}

enum {TEXT_BUFFER_SIZE = 10240};

static void
text_open_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer cdata)
{
	gchar *buffer;
	
	g_return_if_fail (GTK_IS_TEXT_BUFFER (cdata));
	
	if (result != GNOME_VFS_OK) {
		gchar *message;

		message = g_strdup_printf ("Error: %s", gnome_vfs_result_to_string (result));
		g_return_if_fail (message != NULL);
		gtk_text_buffer_set_text (GTK_TEXT_BUFFER(cdata), message, -1);
		g_free (message);
	}

	buffer = g_new (gchar, TEXT_BUFFER_SIZE);
	g_return_if_fail (buffer != NULL);

	gtk_text_buffer_create_tag (GTK_TEXT_BUFFER(cdata), "default-tag", "font", "terminal 10", NULL);
	gnome_vfs_async_read (handle, buffer, TEXT_BUFFER_SIZE, text_read_callback, cdata);
}

static void
text_read_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer buffer,
			GnomeVFSFileSize bytes_requested,
			GnomeVFSFileSize bytes_read,
			gpointer cdata)
{
	GtkTextIter iter;
	GtkTextBuffer *text_buffer;
	
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_TEXT_BUFFER(cdata));

	text_buffer = GTK_TEXT_BUFFER(cdata);
	if (result != GNOME_VFS_OK) {
		gchar *message;

		message = g_strdup_printf ("\n<<Error: %s>>", gnome_vfs_result_to_string (result));
		g_return_if_fail (message != NULL);
		gtk_text_buffer_set_text (text_buffer, message, -1);
		g_free (message);
	}

	

	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (text_buffer, &iter, (gchar *)buffer, bytes_read, "default-tag", NULL);
	
	if (bytes_requested == bytes_read)
		gnome_vfs_async_read (handle, buffer, TEXT_BUFFER_SIZE, text_read_callback, cdata);
	else {
		g_free (buffer);
		gnome_vfs_async_close (handle, text_close_callback, cdata);
	}
}

static void
text_close_callback	(GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer cdata)
{
}
