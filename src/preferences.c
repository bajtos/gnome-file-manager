#include "interface.h"
#include "gfm_list_store.h"
#include "gfm_vfs.h"
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include "preferences.h"
#include "file_list_interface.h"
#include "configuration.h"

#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

static GtkWidget
	*arrow_left = NULL,
	*arrow_right = NULL,
	*arrow_up = NULL,
	*arrow_down = NULL,
	*option_panel = NULL;
static GtkTreeView 
	*view_current = NULL,
	*view_avail = NULL;

static GtkTreeModel
	*store_avail = NULL,
	*store_current = NULL;

static GConfClient *gconf_client = NULL;

enum { COLUMN_ID, COLUMN_NAME, NUM_COLUMNS };

/* fill GtkWidget* variables, parameter widget must be constructed through libglade! */
static void
fill_global_static_variables	(GtkWidget *widget);

/* function called by create_(current|avail)_view */
static void
create_view			(gchar *widget_name, GtkTreeView **view, GtkTreeModel **model);

/* updates settings in GConf */
static void
store_settings			(void);

/* load settings from gconf */
static void
load_settings			(void);

/* sometimes we do not have settings in GConf (e.g. running first time)
 * so we must load this from tree_view */
static void
load_settings_from_widgets (void);


static void
gconf_client_notify_handler	(GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data);


/*************************************
 *
 * 	CODE
 *
 *************************************/

static void
fill_global_static_variables	(GtkWidget *widget)
{

	g_return_if_fail (widget != NULL);

#define FILL(w) w = lookup_widget (widget, #w); g_return_if_fail (w != NULL);
	FILL(arrow_up)
	FILL(arrow_down)
	FILL(arrow_left)
	FILL(arrow_right)
	FILL(option_panel)
#undef FILL
}

static void
create_view			(gchar *widget_name, GtkTreeView **view, GtkTreeModel **model)
{
	GtkTreeViewColumn *view_column = NULL;
	GtkCellRenderer *cell_renderer = NULL;

	g_return_if_fail (view != NULL);
	g_return_if_fail (model != NULL);
	
	if (!(*model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING)))) goto error;
	if (!(*view = GTK_TREE_VIEW(gtk_tree_view_new_with_model (*model)))) goto error;
	g_object_unref (*model); /* tree_view holds reference */
	if (!(cell_renderer = gtk_cell_renderer_text_new ())) goto error;
	if (!(view_column = gtk_tree_view_column_new_with_attributes ("", cell_renderer, 
						"text", COLUMN_NAME, NULL))) goto error;

	gtk_tree_view_append_column (*view, view_column);
	g_object_set (GTK_OBJECT(*view), 
			"name", widget_name, 
			"visible", TRUE,
			"headers-visible", FALSE,
			NULL);

	return;
error:
	g_warning (G_STRLOC);
	if (*model) { g_object_unref (G_OBJECT(*model)); *model = NULL; }
	if (*view) { g_object_unref (G_OBJECT(*view)); *view = NULL; }
	if (view_column)  g_object_unref (G_OBJECT(view_column));
	if (cell_renderer)  g_object_unref (G_OBJECT(cell_renderer));
}

GtkWidget *
create_view_current 		(gchar *widget_name, 
				gchar *unused_string1, 
				gchar *unused_string2,
				gint unused_int1, 
				gint unused_int2)
{
	create_view (widget_name, &view_current, &store_current);
	return GTK_WIDGET(view_current);
}

GtkWidget *
create_view_avail 		(gchar *widget_name, 
				gchar *unused_string1, 
				gchar *unused_string2,
				gint unused_int1, 
				gint unused_int2)
{
	gint col;
	GtkTreeIter iter;
	
	create_view (widget_name, &view_avail, &store_avail);
	if (!view_avail) return NULL;

	/* fill list store */
	for (col = GFM_COLUMN_ICON; col < GFM_NUM_COLUMNS; col++) {
TRACEM("appending %d:%s\n", col, file_list_column_titles[col]);
		gtk_list_store_append (GTK_LIST_STORE(store_avail), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store_avail), &iter,
				COLUMN_ID, col,
				COLUMN_NAME, file_list_column_titles[col],
				-1);
	}

TRACEM ("%s\n", gtk_tree_model_get_iter_first (store_avail, &iter)?"OK":"!!!");
	return GTK_WIDGET(view_avail);
}


/*************************************
 * 
 * 	EVENT HANDLING
 *
 *************************************/

void
on_option_panel_changed		(GtkOptionMenu *option_menu, gpointer data)
{

	load_settings ();
}

void
preferences_run	(void)
{
	static GtkWidget *dialog = NULL;
	gchar *key; gint panel;
	GError *error = NULL;
	guint notify_id[NUM_PANELS];

	if (!dialog) {
		dialog = create_widget ("preferences_dialog");
		g_return_if_fail (dialog != NULL);

		fill_global_static_variables (dialog);
	} else g_return_if_fail (!GTK_WIDGET_VISIBLE (dialog));


	if (!gconf_client) {
		gconf_client = gconf_client_get_default();
		g_return_if_fail (gconf_client != NULL);
	
		/* unref gconf_client during final cleanup */
		g_object_set_data_full (G_OBJECT(dialog), "my-gconf-client", gconf_client, g_object_unref);
		
		/* preload/watch domain '[panel]/file-list' */
		for (panel=PANEL_LEFT; panel<NUM_PANELS; panel++) {
			key = g_strconcat (CONFIGURATION_BASE, panel_names [panel], "file-list", NULL);
			g_return_if_fail (key);
			gconf_client_add_dir (gconf_client, key, GCONF_CLIENT_PRELOAD_NONE, &error);
			g_free (key);
			if (error) {
				g_warning (G_STRLOC": gconf_client_add_dir() failed -- %s", error?error->message:"NULL");
				return;
			}
		}
	}
	
	/* request notification for file-list/ domain */
	for (panel=PANEL_LEFT; panel<NUM_PANELS; panel++) {
		/* request notification for '[panel]/file-list/column-types' */
		key = g_strconcat (CONFIGURATION_BASE, panel_names [panel], "file-list/column-types", NULL);
		notify_id[panel] = gconf_client_notify_add (
					gconf_client, key, 
					gconf_client_notify_handler, GINT_TO_POINTER (panel),
					NULL /* destroy-notify */, &error);
		g_free (key);
		if (error) {
			g_warning (G_STRLOC": gconf_client_notify_add() failed -- %s", error?error->message:"NULL");
			return;
		}
	}

	/* fill ListStore, etc. */
	load_settings();
	
	
	/* run dialog */
	gtk_dialog_run (GTK_DIALOG(dialog));
	gtk_widget_hide (dialog);

	/* remove notifications */
	for (panel=PANEL_LEFT; panel<NUM_PANELS; panel++)
		gconf_client_notify_remove (gconf_client, notify_id[panel]);
		
}

static void
swap_iters			(GtkTreeModel *model, GtkTreeIter *iter1, GtkTreeIter *iter2)
{
	gint col1, col2; gchar *name1, *name2;

	gtk_tree_model_get (model, iter1, COLUMN_ID, &col1, COLUMN_NAME, &name1, -1);
	gtk_tree_model_get (model, iter2, COLUMN_ID, &col2, COLUMN_NAME, &name2, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), iter2, COLUMN_ID, col1, COLUMN_NAME, name1, -1);
	gtk_list_store_set (GTK_LIST_STORE(model), iter1, COLUMN_ID, col2, COLUMN_NAME, name2, -1);
	g_free (name1);
	g_free (name2);
}

gboolean
on_arrow_up_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data)
{
	GtkTreeIter iter, iter0;
	GtkTreePath *path;

	g_return_val_if_fail (view_current, FALSE);
	gtk_tree_view_get_cursor (view_current, &path, NULL);
	if (!path) 
		goto done;
	if (!gtk_tree_model_get_iter (store_current, &iter, path))
		goto done;

	if (!gtk_tree_path_prev (path))
		goto done;
	if (!gtk_tree_model_get_iter (store_current, &iter0, path))
		goto done;

	g_object_freeze_notify (G_OBJECT (view_current));
	swap_iters (store_current, &iter0, &iter);
	gtk_tree_view_set_cursor (view_current, path, NULL, FALSE);
	g_object_thaw_notify (G_OBJECT (view_current));

	store_settings();
done:
	gtk_tree_path_free (path);
	return FALSE;
}


gboolean
on_arrow_left_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data)
{
	GtkTreeIter iter_cur, iter_avail, new_iter;
	GtkTreePath *path_cur, *path_avail;
	gint col; gchar *name;
	
	g_return_val_if_fail (view_current, FALSE);
	g_return_val_if_fail (view_avail, FALSE);
	gtk_tree_view_get_cursor (view_current, &path_cur, NULL);
	gtk_tree_view_get_cursor (view_avail, &path_avail, NULL);
	if (!path_avail) 
		goto done;

	if (path_cur) {
		if (!gtk_tree_model_get_iter (store_current, &iter_cur, path_cur))
			goto done;
	}
	if (!gtk_tree_model_get_iter (store_avail, &iter_avail, path_avail))
		goto done;

	gtk_tree_model_get (store_avail, &iter_avail, COLUMN_ID, &col, COLUMN_NAME, &name, -1);
	if (path_cur) 
		gtk_list_store_insert_before (GTK_LIST_STORE(store_current), &new_iter, &iter_cur);
	else {
		gtk_list_store_append (GTK_LIST_STORE(store_current), &new_iter);
		path_cur = gtk_tree_model_get_path (store_current, &new_iter);
	}
	gtk_list_store_set(GTK_LIST_STORE(store_current), &new_iter, COLUMN_ID, col, COLUMN_NAME, name, -1);

	gtk_tree_view_set_cursor (view_current, path_cur, NULL, FALSE);
	
	store_settings();
done:
	gtk_tree_path_free (path_cur);
	gtk_tree_path_free (path_avail);
	return FALSE;
}


gboolean
on_arrow_right_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_val_if_fail (view_current, FALSE);
	gtk_tree_view_get_cursor (view_current, &path, NULL);
	if (!gtk_tree_model_get_iter (store_current, &iter, path))
		goto done;

	gtk_list_store_remove (GTK_LIST_STORE(store_current), &iter);
	gtk_tree_view_set_cursor (view_current, path, NULL, FALSE);

	store_settings();
done:
	gtk_tree_path_free (path);
	return FALSE;
}


gboolean
on_arrow_down_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data)
{
	GtkTreeIter iter, iter0;
	GtkTreePath *path;

	g_return_val_if_fail (view_current, FALSE);
	gtk_tree_view_get_cursor (view_current, &path, NULL);
	if (!gtk_tree_model_get_iter (store_current, &iter, path))
		goto done;

	gtk_tree_path_next (path);
	if (!gtk_tree_model_get_iter (store_current, &iter0, path))
		goto done;

	g_object_freeze_notify (G_OBJECT (view_current));
	swap_iters (store_current, &iter0, &iter);
	gtk_tree_view_set_cursor (view_current, path, NULL, FALSE);
	g_object_thaw_notify (G_OBJECT (view_current));

	store_settings();
done:
	gtk_tree_path_free (path);
	return FALSE;
}


/*************************************
 *
 * 	settings & GConf
 *
 *************************************/

/*
 * store_settings
 */
static gboolean
foreach_func	(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GSList **list);
	
static void
store_settings			(void)
{
	GConfClient *client = gconf_client;
	gchar *key;
	GSList *values = NULL;
	gint panel;
	
	g_return_if_fail (store_current && option_panel);
	panel = gtk_option_menu_get_history (GTK_OPTION_MENU(option_panel));
	g_return_if_fail (client != NULL);
	key = g_strconcat (CONFIGURATION_BASE, panel_names [panel], "file-list/column-types", NULL);
	g_return_if_fail (key != NULL);

	gtk_tree_model_foreach (store_current, (GtkTreeModelForeachFunc) foreach_func, &values);
	values = g_slist_reverse (values);
	if (!gconf_client_set_list (client, key, GCONF_VALUE_INT, values, NULL)) 
		g_warning (G_STRLOC": could not set %s.", key);
	g_free (key);
	g_slist_free (values);
}

static gboolean
foreach_func	(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GSList **list)
{
	gint col; gchar *name;
	
	g_return_val_if_fail (model && iter, FALSE);

	gtk_tree_model_get (model, iter, COLUMN_ID, &col, COLUMN_NAME, &name, -1);
	*list = g_slist_prepend (*list, GINT_TO_POINTER(col));
TRACEM("%d:%s\n",col,name);
	return FALSE;
}


/*
 * load_settings()
 */
static void
load_settings			(void)
{
	GConfClient *client = gconf_client;
	gchar *key;
	GSList *values = NULL, *val;
	gint panel, type;
	GtkTreeIter iter;
	GError *error = NULL;

	g_return_if_fail (store_current && view_current && option_panel);
	g_return_if_fail (client != NULL);

	panel = gtk_option_menu_get_history (GTK_OPTION_MENU(option_panel));

	gtk_list_store_clear (GTK_LIST_STORE(store_current));

	
	key = g_strconcat (CONFIGURATION_BASE, panel_names [panel], "file-list/column-types", NULL);
	g_return_if_fail (key != NULL);


	values = gconf_client_get_list (client, key, GCONF_VALUE_INT, &error);
	if (!values) {
		g_warning (G_STRLOC": could not load %s -- %s.", key, error?error->message:"NULL");
		load_settings_from_widgets();
	}
	g_free (key);
	
	for (val = values; val; val=val->next) {
		type = GPOINTER_TO_INT (val->data);
		gtk_list_store_append (GTK_LIST_STORE(store_current), &iter);
		gtk_list_store_set (GTK_LIST_STORE(store_current), &iter,
				COLUMN_ID, type,
				COLUMN_NAME, file_list_column_titles[type],
				-1);
	}
	g_slist_free (values);
	
	gtk_widget_grab_focus (GTK_WIDGET(view_current));
}

static void
load_settings_from_widgets (void) 
{
	gint panel, type;
	GList *columns = NULL, *col;
	GtkTreeIter iter;

	g_return_if_fail (store_current && view_current);
	panel = gtk_option_menu_get_history (GTK_OPTION_MENU(option_panel));

	columns = gtk_tree_view_get_columns (panels_data[panel].files_view);
	for (col = columns; col; col = col->next) {
		type = gtk_tree_view_column_get_sort_column_id (GTK_TREE_VIEW_COLUMN(col->data));
		gtk_list_store_append (GTK_LIST_STORE(store_current), &iter);
		gtk_list_store_set (GTK_LIST_STORE(store_current), &iter,
				COLUMN_ID, type,
				COLUMN_NAME, file_list_column_titles[type],
				-1);
	}
	g_list_free (columns);

}

static void
gconf_client_notify_handler	(GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data)
{
	g_return_if_fail (option_panel);
	if (gtk_option_menu_get_history (GTK_OPTION_MENU(option_panel)) !=
	    GPOINTER_TO_INT (user_data))
		return; /* panel that is not visible was changed */

	load_settings ();
}
