#include <libgnomevfs/gnome-vfs.h>
#include <gtk/gtk.h>
#include <glib.h>

void
render_name		(GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer data);


enum
{
  COLUMN_ONE,
  COLUMN_TWO,
  N_COLUMNS
};


void populate_tree_model (GtkListStore *model)
{
	GnomeVFSDirectoryHandle *handle;
	GnomeVFSFileInfo *info;
	GnomeVFSResult res;
	GtkTreeIter iter;
//	GValue *value;
	
#define URI "/"

	gnome_vfs_init();
	res = gnome_vfs_directory_open (&handle, URI, GNOME_VFS_FILE_INFO_DEFAULT);
	if (res) goto error;

	for (;;) {
		info = gnome_vfs_file_info_new ();
		res = gnome_vfs_directory_read_next (handle, info);
		
		if (res == GNOME_VFS_ERROR_EOF) break;
		
		if (res && res != GNOME_VFS_ERROR_EOF) {
			gnome_vfs_file_info_unref (info);
			goto error;
		}

	
puts(info->name);
//		value = g_new0 (GValue, 1); 
//		g_value_init (value, G_TYPE_POINTER);
//		g_value_set_pointer (value, info);
		gtk_list_store_append (GTK_LIST_STORE(model), &iter); /** ?? **/	
//		gtk_list_store_set_value (GTK_LIST_STORE(model), &iter, COLUMN_ONE, value);
//		gtk_list_store_set (model, &iter, COLUMN_ONE, info->name, -1);
		gtk_list_store_set (model, &iter, COLUMN_ONE, info, COLUMN_TWO, 0, -1);
	} 

	
	goto done;	

error:
	puts(gnome_vfs_result_to_string (res));

done:
	gnome_vfs_directory_close (handle);
}




void
init_list	(GtkWidget *window)
{

  GtkListStore *model;
  GtkWidget *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell_renderer;



  /* Create a model.  We are using the store model for now, though we
 *    * could use any other GtkTreeModel */
  model = gtk_list_store_new (N_COLUMNS, G_TYPE_POINTER, G_TYPE_INT);
puts("model created");

  /* Create a view */
  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
puts("view created");

  /* The view now holds a reference.  We can get rid of our own
 *    * reference */
  g_object_unref (G_OBJECT (model));

  /* Create a cell render and arbitrarily make it red for demonstration
 *    * purposes */
  cell_renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (cell_renderer), "foreground", "red", NULL);
puts("cell renderer created");

  /* Create a column, associating the "text" attribute of the
 *    * cell_renderer to the first column of the model */
/*  column = gtk_tree_view_column_new_with_attributes ("title",
						     cell_renderer,
						     "text", COLUMN_ONE,
						     NULL);
*/
  column = gtk_tree_view_column_new_with_attributes ("title", cell_renderer, NULL);
puts("column created");
  gtk_tree_view_column_set_cell_data_func (column, cell_renderer,render_name ,NULL,NULL);
puts("datafunc set");

  /* Add the column to the view. */
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
puts("column appended");


  gtk_widget_show (GTK_WIDGET(view));
puts("view shown");
  gtk_container_add (GTK_CONTAINER(window), view);
puts("view inserted");

  /* custom function to fill the model with data */
  populate_tree_model (model);
puts("model populated");

}


int 
main (int argc, char **argv)
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	gnome_vfs_init();

  	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	init_list (window);
puts("showing...");
	gtk_widget_show (window);

puts("running...");
	gtk_main();
puts("exiting...");
	gtk_exit(0);

	return 0;

}	

void
render_name		(GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer data)
{
	GValue *val;
	GnomeVFSFileInfo *info;
	gint valid;


puts("entering render_name");
//	g_object_set (G_OBJECT(cell), "text", "bu", NULL);
//	return;
	
	gtk_tree_model_get (tree_model, iter, 
			COLUMN_ONE, &info, 
			COLUMN_TWO, &valid, 
			-1);
puts("value get");
//	if (valid) goto done;

puts(info->name);
	
//puts("checking type");	
//	g_return_if_fail (G_VALUE_HOLDS_POINTER(val));
//puts("extracting data");	
//	info = g_value_get_pointer (val);
	/* FIXME: handle possible errors */
	g_return_if_fail (info != NULL);
//	g_value_unset (val);

	g_return_if_fail (GTK_IS_CELL_RENDERER_TEXT (cell));

	g_object_set (G_OBJECT(cell), "text", info->name, NULL);
	gtk_list_store_set(GTK_LIST_STORE(tree_model), iter, COLUMN_TWO, 1, -1);

done:
puts("leaving render_name");
}
