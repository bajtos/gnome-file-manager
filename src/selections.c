#include "selections.h"
#include "gfm_vfs.h"
#include "gfm_list_store.h"
#include <gtk/gtk.h>
#include "interface.h"

#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

/*
 * selections_select_all()
 * selections_unselect_all()
 * selections_toggle_selections()
 */
void
selections_select_all 		(GtkTreeView *view)
{
	gtk_tree_selection_select_all (gtk_tree_view_get_selection (view));
}

void 
selections_unselect_all		(GtkTreeView *view)
{
	gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (view));
}


static gboolean
invert_selection (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GtkTreeSelection *selection) 
{
	if (gtk_tree_selection_iter_is_selected(selection,iter)) 
		gtk_tree_selection_select_iter (selection,iter);
	else
		gtk_tree_selection_unselect_iter (selection,iter);
	return FALSE;
}

void
selections_invert_selection	(GtkTreeView *view)
{
	gtk_tree_model_foreach (
		gtk_tree_view_get_model (view), 
		(GtkTreeModelForeachFunc)invert_selection,
		gtk_tree_view_get_selection (view));
}

/*************************************
 *
 * 	selection monitor
 *
 *************************************/

static void
on_changed_callback (GtkTreeSelection *treeselection, gint panel);

void
selections_install_monitor	(GtkTreeView *view)
{
	g_return_if_fail (GTK_IS_TREE_VIEW (view));

	g_signal_connect (gtk_tree_view_get_selection(view), "changed", (GCallback) on_changed_callback, 
			GINT_TO_POINTER(get_panel_id(view)));
}

/*
 * on_changed_callback()
 */
struct OnChangedCallbackData {
	GnomeVFSFileSize total_size;
	guint total_files;
};

static void
count_totals (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, struct OnChangedCallbackData *cdata);

static void
on_changed_callback (GtkTreeSelection *treeselection, gint panel)
{
	struct OnChangedCallbackData cdata = {0,0};
	gchar *label;
	
	g_return_if_fail (IS_PANEL (panel));
	gtk_tree_selection_selected_foreach (treeselection, (GtkTreeSelectionForeachFunc)count_totals, &cdata);

	label = g_strdup_printf ("%u %s: %'"GNOME_VFS_SIZE_FORMAT_STR" %s",
			cdata.total_files,
			cdata.total_files!=1?_("files"):_("file"),
			cdata.total_size,
			cdata.total_size!=1?_("bytes"):_("byte")
	);
	g_return_if_fail (label);
	TRACEM("%s\n",label)
	
	encode_to_utf8 (&label);
	gtk_label_set_text (panels_data[panel].finfo, label);
	g_free(label);
}

static void
count_totals (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, struct OnChangedCallbackData *cdata)
{
	GfmRowData *row_data;

	g_return_if_fail (cdata != NULL);

	cdata->total_files++;

	gtk_tree_model_get (model, iter, GFM_COLUMN_DATA, &row_data, -1);
	g_return_if_fail (row_data != NULL);
	
	if (gfm_is_directory(row_data->real_info?:row_data->info))
		return;

	if ((row_data->info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) == 0)
		return;
	cdata->total_size += row_data->info->size;
}
