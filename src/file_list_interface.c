#include <ctype.h>
#include <time.h>
#include <glib.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <pango/pango.h>
#include "file_list_interface.h"
#include "file_list_icons.h"
#include "configuration.h"
#include "gfm_list_store.h"
#include "selections.h"
#include "file_list_tips.h"

#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

gchar *file_list_column_titles[GFM_NUM_COLUMNS];
	/* initialized in create_file_list */

gint file_list_column_widths [NUM_PANELS][GFM_NUM_COLUMNS] = 
 {{-2, FILE_ICON_SIZE+4,   80,  60, 72, 72, 72, 60, 60, 60},
  {-2, FILE_ICON_SIZE+4,   80,  60, 72, 72, 72, 60, 60, 60}};

/* function for creating file list, called by libglade */
GtkWidget *
create_file_list (gchar *widget_name, gchar *unused_string1, gchar *unused_string2,
		gint panel, gint unused_int);	


static void set_column_view (GtkTreeViewColumn *column, gint type, gint pos, gint panel);

static void load_column_widths (gint panel);
static void store_column_widths (gint panel);
static void apply_column_widths (gint panel);

/* GConf & notification */

static void
gconf_client_notify_handler	(GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data);

				
				

/* called when resizing column */
void
on_file_list_size_allocate	(GtkWidget *widget,
				GtkAllocation *allocation,
				gint panel);

/* changing file attributes inside GtkTreeView */
struct SetFileInfoCallbackData {
	GtkListStore *model;
	GtkTreeIter *iter;
	GfmRowData *row_data;
};

static void
set_file_info_callback 	(GnomeVFSAsyncHandle *handle, 
			GnomeVFSResult result, 
			GnomeVFSFileInfo *file_info, 
			struct SetFileInfoCallbackData *cdata);

static void 
rename_file 		(GtkCellRendererText *cellrenderer, 
			gchar *path_string, 
			gchar *new_value, 
			gint panel);

static void 
change_permissions 	(GtkCellRendererText *cellrenderer, 
			gchar *path_string, 
			gchar *new_value, 
			gint panel);

/** 
 *
 * functions for rendering data from GnomeVFSFileInfo 
 *
 **/ 
static void render_icon	(GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data);
static void render_name (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data);
static void render_size (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data);
static void render_date_time (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data);
static void render_perm (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data);
static void render_guid (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data);


/*************************************
 *
 * 	C O D E
 *
 *************************************/ 


/*************************************
 *
 * 	UPDATE_FILE_LIST_SETTINGS
 *
 *************************************/

void
update_file_list_settings (gint panel)
{
	GConfClient *client;
	GSList *column_types = NULL, *it;
	GList *column_views, *iv;
	GError *error = NULL;
	GtkListStore *store = panels_data[panel].files_data;
	GtkTreeView *tview = panels_data[panel].files_view;
	GtkTreeViewColumn *col;
	gchar *key;
	gint pos;

	g_return_if_fail (store != NULL);
	g_return_if_fail (tview != NULL);

	client = gconf_client_get_default ();
	g_return_if_fail (client != NULL);

#if DEBUG
printf("updating settings: %d. panel\n", panel+1);
#endif 
	key = g_strconcat (CONFIGURATION_BASE, panel_names[panel], "file-list/column-types", NULL);
	if (key) {
		column_types = gconf_client_get_list (client, key, GCONF_VALUE_INT ,&error);
		g_free (key);
	};
	g_object_unref (G_OBJECT(client));
	
	
	if (error) {
		g_warning ("Could not get file_list settings: %s", error->message);
		g_error_free (error);
	}

	if (!column_types) {
		/** default settings **/
#if DEBUG
puts("aplying default settings");
#endif 
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_ICON));
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_NAME));
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_SIZE));
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_MTIME));
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_PERM));
	
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_UID));
		column_types = g_slist_append (column_types, GINT_TO_POINTER (GFM_COLUMN_GID));
	}

	g_assert (column_types != NULL);
	
	column_views = gtk_tree_view_get_columns (tview);
	
	for (iv=column_views, it=column_types, pos=0; 
	  iv || it; 
	  it=(it?it->next:it), iv=(iv?iv->next:iv), pos++)  {
		if (iv && it) {
#if DEBUG 
printf("updating column %d\n", GPOINTER_TO_INT(it->data));
#endif 
			set_column_view (GTK_TREE_VIEW_COLUMN(iv->data), GPOINTER_TO_INT(it->data), pos, panel);	
		} else if (iv) {
	
#if DEBUG 
puts("removing column");
#endif 
			gtk_tree_view_remove_column (tview, iv->data);
		} else {
#if DEBUG 		
printf("adding column %d\n", GPOINTER_TO_INT(it->data));
#endif 
			col = gtk_tree_view_column_new ();
			set_column_view (col, GPOINTER_TO_INT(it->data), pos, panel);
			gtk_tree_view_append_column (tview ,col);
		}
	}
	
#if DEBUG
puts("cleanup");
#endif

	/** cleanup **/
	g_slist_free (column_types);
	g_list_free (column_views);
	 		
#if DEBUG
puts("done.");
#endif
}


/*************************************
 * 
 * 	CREATE_FILE_LIST
 *
 *************************************/

GtkWidget *
create_file_list (gchar *widget_name, gchar *unused_string1, gchar *unused_string2,
		gint panel, gint unused_int)	
{
	PanelData *pd = &panels_data[panel];
	gchar *key = NULL; 
	GConfClient *client = gconf_client_get_default();
	GError *error = NULL;
	gint i;
	
	g_return_val_if_fail (IS_PANEL(panel), NULL);
	g_return_val_if_fail (pd->files_view == NULL, GTK_WIDGET(pd->files_view));

	/** initialize file_list_column_titles **/
	i=0;
	file_list_column_titles[i++] = "?";
	file_list_column_titles[i++] = _("Icon");
	file_list_column_titles[i++] = _("Name");
	file_list_column_titles[i++] = _("Size");
	file_list_column_titles[i++] = _("ATime");
	file_list_column_titles[i++] = _("MTime");
	file_list_column_titles[i++] = _("CTime");
	file_list_column_titles[i++] = _("Perm");
	file_list_column_titles[i++] = _("UID");
	file_list_column_titles[i++] = _("GID");

	/** Data storage **/
	pd->files_data = GTK_LIST_STORE (gfm_list_store_new());
	/** Tree View **/
	pd->files_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model (
					GTK_TREE_MODEL(pd->files_data)));
	g_object_unref (G_OBJECT(pd->files_data)); /* file_view holds reference */
	g_object_set (GTK_OBJECT(pd->files_view), 
			"name", widget_name, 
			"visible", TRUE,
			"headers-visible", TRUE,
			NULL);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (pd->files_view), GTK_SELECTION_MULTIPLE);
	selections_install_monitor (pd->files_view);
	g_signal_connect_after (G_OBJECT(pd->files_view), "size-allocate",
			(GCallback)on_file_list_size_allocate, GINT_TO_POINTER(panel));

	update_file_list_settings (panel);

	/** other associated stuff in PanelData **/
	pd->load_dir_handle = malloc (sizeof(gpointer));
			/* FIXME: shoudn't we use g_malloc? */
			/* FIXME: memory leak, nobody releases this pointer */
	*pd->load_dir_handle = NULL;

	pd->uri_wdir = pd->uri_ndir = NULL; 
		/* not necessary, since panels_data is global static var. */

	file_list_tips_install (panel);
	
	/* add GConf notification */
	if (!client) goto error;
	key = g_strconcat (CONFIGURATION_BASE, panel_names[panel], "file-list", NULL);
	if (!key) goto error;
	gconf_client_add_dir (client, key, GCONF_CLIENT_PRELOAD_NONE, &error);
	if (error) {
		g_warning (G_STRLOC": gconf_client_add_dir() failed -- %s", error?error->message:"NULL");
		goto error;
	}

	gconf_client_notify_add (client, key, 
				gconf_client_notify_handler, GINT_TO_POINTER (panel),
				NULL /* destroy-notify */, &error);
	if (error) {
		g_warning (G_STRLOC": gconf_client_notify_add() failed -- %s", error?error->message:"NULL");
		goto error;
	}

	goto ok;
error:
	g_warning ("Could not setup GConf notification -> you may encounter strange problems with configuration");
	if (key)
		g_free (key);
ok:
	if (client)
		g_object_unref (G_OBJECT(client));
	return GTK_WIDGET(panels_data[panel].files_view);
}



/************************************* 
 *
 * 	SET_GFM_COLUMN_VIEW
 *
 *************************************/ 

static void set_column_view (GtkTreeViewColumn *column, gint type, gint pos, gint panel)
{
	GtkCellRenderer *cell_renderer;
	gint min_widths[GFM_NUM_COLUMNS] = {-2, -1, 60, 30, -1, -1, -1, -1, -1, -1};
	gint max_widths[GFM_NUM_COLUMNS] = {-2, -1,  -1, -1, -1, -1, -1, -1, -1, -1};
	GCallback edit_callbacks [GFM_NUM_COLUMNS] = {
		NULL, NULL, /* DATA, ICON */
		(GCallback) rename_file, 
		NULL,  /* SIZE */
		NULL, NULL, NULL,  /* TIMES */ 
		(GCallback) change_permissions, 
		NULL, NULL /* U/G ID */
	};
	GtkTreeCellDataFunc data_func = NULL;
	
	g_return_if_fail (column != NULL);
	g_return_if_fail (type>0 && type <GFM_NUM_COLUMNS);

#if DEBUG
printf("	entering set_column_view: type=%d pos=%d panel=%d\n",type,pos,panel);
#endif
	
	/* clear old settings */
	gtk_tree_view_column_clear (column);

	/* load fixed widths from gconf */
	load_column_widths (panel);

	/* set properties */
	cell_renderer = (type == GFM_COLUMN_ICON) ? gtk_cell_renderer_pixbuf_new () : gtk_cell_renderer_text_new ();
	g_return_if_fail (cell_renderer != NULL);

	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	g_object_set (G_OBJECT(column),
			"title", (type>GFM_COLUMN_ICON)?file_list_column_titles [type]:"",
			"visible", TRUE,
			"resizable", type != GFM_COLUMN_ICON,
			"alignment", 0.0,
			"sizing", GTK_TREE_VIEW_COLUMN_FIXED,
			"min-width", min_widths [type],
			"max-width", max_widths [type],
			"fixed-width", file_list_column_widths [panel][type],
			"reorderable", FALSE,
			NULL);
	if (edit_callbacks [type]) {
		g_object_set (G_OBJECT(cell_renderer),
				"editable", TRUE,
				NULL);
		g_signal_connect (cell_renderer, "edited", edit_callbacks[type], GINT_TO_POINTER(panel));
	}
	
	/* set data function */
	switch (type) {
		case GFM_COLUMN_ICON:
			data_func = render_icon;
			break;
		case GFM_COLUMN_NAME:
			data_func = render_name;
			break;
		case GFM_COLUMN_SIZE:
			data_func = render_size;
			break;
		case GFM_COLUMN_ATIME:
		case GFM_COLUMN_MTIME:
		case GFM_COLUMN_CTIME:
			data_func = render_date_time;
			break;
		case GFM_COLUMN_PERM:
			data_func = render_perm;
			break;
		case GFM_COLUMN_UID:
		case GFM_COLUMN_GID:
			data_func = render_guid;
			break;
		default:
			g_assert_not_reached ();
	}
	gtk_tree_view_column_set_cell_data_func (column, cell_renderer, data_func, GINT_TO_POINTER(type), NULL);

	gtk_tree_view_column_set_sort_column_id (column, type);
		
	if (type == GFM_COLUMN_NAME && pos != -1) {
		g_object_set (G_OBJECT(panels_data[panel].files_view),
					"enable-search", TRUE,
					"search-column", GFM_COLUMN_NAME,
					NULL);

		gtk_tree_sortable_set_sort_column_id (
			GTK_TREE_SORTABLE(panels_data[panel].files_data), 
			type, 
			GTK_SORT_ASCENDING);	
	}
#if DEBUG 
puts("		done");
#endif 
}


/**********************************
 * 
 *   C E L L   R E N D E R E R S
 * 
 */

static void render_icon	(GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	GdkPixbuf *icon;
	GnomeVFSFileInfo *info;
	GfmRowData *row_data;

	g_assert (GPOINTER_TO_INT(user_data) == GFM_COLUMN_ICON);
TRACE

	gtk_tree_model_get (tree_model, iter,
			GFM_COLUMN_DATA, &row_data,
			GFM_COLUMN_ICON, &icon,
			-1);


	info = row_data->real_info?:row_data->info;
	if (!icon) {
TRACE
		icon = file_list_icons_get_icon (info);
		if (!icon) 
			goto done;
TRACE
		gtk_list_store_set (GTK_LIST_STORE(tree_model), iter, GFM_COLUMN_ICON, icon, -1);
	}

TRACE
	g_object_set (G_OBJECT (cell),
		"pixbuf", icon,
		NULL);
	
done:
	gfm_row_data_free (row_data);
TRACE
}


static void render_name (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data)
{

	gchar *text;
	GnomeVFSFileInfo *info;
	GfmRowData *row_data;

	g_assert (GPOINTER_TO_INT(user_data) == GFM_COLUMN_NAME);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN(tree_column));
	g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
TRACE
	gtk_tree_model_get (tree_model, iter,
			GFM_COLUMN_DATA, &row_data,
			GFM_COLUMN_NAME, &text,
			-1);
	info = row_data->info;
	
	if (!text) {
		text = g_strdup(info->name);
		encode_to_utf8 (&text);
		gtk_list_store_set (GTK_LIST_STORE(tree_model), iter, GFM_COLUMN_NAME, text, -1);
	}

	g_object_set (G_OBJECT(cell), "text", text, NULL);

	gfm_row_data_free (row_data);
	g_free (text);
TRACE
}


static void render_size (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	gchar *text;
	GnomeVFSFileInfo *info;
	GfmRowData *row_data;

	g_assert (GPOINTER_TO_INT(user_data) == GFM_COLUMN_SIZE);
TRACE
	gtk_tree_model_get (tree_model, iter,
			GFM_COLUMN_DATA, &row_data,
			GFM_COLUMN_SIZE, &text,
			-1);

	info = row_data->info;
	if (!text) {
		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) 
//			text = g_strdup_printf("%"GNOME_VFS_SIZE_FORMAT_STR, info->size);
			text = human_readable_file_size (info->size);
		else
			text = g_strdup("");
		gtk_list_store_set (GTK_LIST_STORE(tree_model), iter, GFM_COLUMN_SIZE, text, -1);
	}
	
	g_object_set (G_OBJECT(cell), 
			"text", text,
			"xalign", 1.0,
			NULL);

	
	g_free (text);
	gfm_row_data_free (row_data);
TRACE
}


#include <string.h>
static void render_date_time (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	gchar *text;
	GfmRowData *row_data;
	GnomeVFSFileInfo *info;
	time_t tm;
	gint flag;
	struct tm then, now;

TRACE
	gtk_tree_model_get (tree_model, iter,
			GFM_COLUMN_DATA, &row_data,
			GPOINTER_TO_INT (user_data), &text,
			-1);

	info = row_data->info;
	if (!text) {
		tm = time (NULL);
		localtime_r (&tm, &now);

		switch (GPOINTER_TO_INT(user_data)) {
			case GFM_COLUMN_MTIME:
				tm = info->mtime;
				flag = GNOME_VFS_FILE_INFO_FIELDS_MTIME;
				break;
			case GFM_COLUMN_ATIME:
				tm = info->atime;
				flag = GNOME_VFS_FILE_INFO_FIELDS_ATIME;
				break;
			case GFM_COLUMN_CTIME:
				tm = info->ctime;
				flag = GNOME_VFS_FILE_INFO_FIELDS_CTIME;
				break;
			default:
				flag = 0; /* to avoid warnings */
				g_assert_not_reached();
			
		}

		if (info->valid_fields & flag ) {
			localtime_r (&tm, &then);

			text = malloc ((12+1)*sizeof(gchar));
			
			strftime (text, 13,
				(now.tm_year == then.tm_year) ? "%b %e %H:%M" : "%b %e %Y",
				&then);
		} else 
			text = strdup ("");

		
		encode_to_utf8 (&text);
		gtk_list_store_set (GTK_LIST_STORE(tree_model), iter, GPOINTER_TO_INT (user_data), text, -1);
	}

	g_object_set (G_OBJECT(cell), "text", text, NULL);
	
	free (text);
	gfm_row_data_free (row_data);
TRACE
}



static void render_perm (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	gchar *text;
	GnomeVFSFileInfo *info;
	GfmRowData *row_data;

	g_assert (GPOINTER_TO_INT(user_data) == GFM_COLUMN_PERM);
	
	TRACE
	gtk_tree_model_get (tree_model, iter,
			GFM_COLUMN_DATA, &row_data,
			GFM_COLUMN_PERM, &text,
			-1);

	info = row_data->info;
	if (!text) {

		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS) {
			text = malloc ((9+1) * sizeof(gchar));
#define F(Set) (info->permissions & GNOME_VFS_PERM_##Set)
#define FLAG(Flag, Set) ((Set) ? (Flag) : '-')
#define FLAGx(SFlag, SSet, xSet) ((xSet) ? ((SSet) ? (SFlag) : 'x') : ((SSet) ? toupper(SFlag):'-'))
			
			text[0] = FLAG  ('r', F(USER_READ));
			text[1] = FLAG  ('w', F(USER_WRITE));
			text[2] = FLAGx ('s', F(SUID),
						F(USER_EXEC));
			text[3] = FLAG  ('r', F(GROUP_READ));
			text[4] = FLAG  ('w', F(GROUP_WRITE));
			text[5] = FLAGx ('s', F(SGID),
						F(GROUP_EXEC));
			text[6] = FLAG  ('r', F(OTHER_READ));
			text[7] = FLAG  ('w', F(OTHER_WRITE));
			text[8] = FLAGx ('t', F(STICKY),
						F(OTHER_EXEC));
			text[9] = 0;
#undef F
#undef FLAG
#undef FLAGx
		} else
			text = strdup("");
		gtk_list_store_set (GTK_LIST_STORE(tree_model), iter, GFM_COLUMN_PERM, text, -1);
	}

	g_object_set (G_OBJECT(cell), "text", text, NULL);
	
	free (text);
	gfm_row_data_free (row_data);
	TRACE
}

#include <pwd.h>
#include <grp.h>
static void render_guid (GtkTreeViewColumn *tree_column,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	/* TODO: how does this aproach work with remote uids/gids? */

	gchar *text;
	GnomeVFSFileInfo *info;
	GfmRowData *row_data;
	gint column = GPOINTER_TO_INT(user_data);
	static GHashTable *hash_uid, *hash_gid; /* key: (u|g)id, value: name */

	g_assert (column == GFM_COLUMN_GID || column == GFM_COLUMN_UID);

	
	TRACE
	gtk_tree_model_get (tree_model, iter,
			GFM_COLUMN_DATA, &row_data,
			column, &text,
			-1);

	TRACE
	info = row_data->info;
	if (!text) {
		if (column == GFM_COLUMN_UID) {
	TRACE
			if (!hash_uid) 
				hash_uid = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	TRACE
			text = g_hash_table_lookup (hash_uid, GUINT_TO_POINTER(info->uid));
			if (!text) {
				struct passwd *pw;

	TRACE
				pw = getpwuid(info->uid);
	TRACE
				if (pw->pw_name)
					text = strdup(pw->pw_name);
				else
					text = g_strdup_printf ("%u", info->uid);
	TRACE
				g_hash_table_insert (hash_uid, GUINT_TO_POINTER(info->uid), text);
	TRACE
//				free (pw);
	TRACE
			}
		} else {
	TRACE
			if (!hash_gid) 
				hash_gid = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	TRACE
			text = g_hash_table_lookup (hash_gid, GUINT_TO_POINTER(info->gid));
	TRACE
			if (!text) {
				struct group *gr;

	TRACE
				gr = getgrgid(info->gid);
				if (gr->gr_name)
					text = strdup(gr->gr_name);
				else
					text = g_strdup_printf ("%u", info->gid);
	TRACE
				g_hash_table_insert (hash_gid, GUINT_TO_POINTER(info->gid), text);
	TRACE
//				free (gr);
			}
		}

	TRACEM("group/users is %s, column %d\n", text, column);
		gtk_list_store_set (GTK_LIST_STORE(tree_model), iter, column, text, -1);
	}
	
	TRACE
	g_object_set (G_OBJECT(cell), "text", text, NULL);
	
	gfm_row_data_free (row_data);
	TRACE
}


/*
 * rename_file ()
 * set_file_info_callback()
 * change_permissions()
 */
static void rename_file (GtkCellRendererText *cellrenderer, gchar *path_string, gchar *new_value, gint panel)
{
	GtkTreeIter iter;
	GfmRowData *row_data;
	GtkTreeModel *model;
	GnomeVFSURI *uri;
	struct SetFileInfoCallbackData *cdata;
	GnomeVFSAsyncHandle *handle;
	
	g_return_if_fail (IS_PANEL(panel));
	g_return_if_fail (new_value != NULL && new_value[0]!='\0');

	model = GTK_TREE_MODEL(panels_data[panel].files_data);
	
	g_return_if_fail (gtk_tree_model_get_iter_from_string (model, &iter, path_string));

	gtk_tree_model_get (model, &iter, GFM_COLUMN_DATA, &row_data, -1);
	g_return_if_fail (row_data!=NULL);
	g_return_if_fail (row_data->info!=NULL);

	if (!strcmp(row_data->info->name, new_value))
		return;

	uri = gnome_vfs_uri_append_file_name (panels_data[panel].uri_wdir, row_data->info->name);
	g_free (row_data->info->name);
	row_data->info->name = g_strdup (new_value);
	
	
	g_return_if_fail ((cdata = g_new (struct SetFileInfoCallbackData, 1)));
	cdata->model = GTK_LIST_STORE(model);
	cdata->iter = gtk_tree_iter_copy (&iter);
	cdata->row_data = row_data;

	TRACE
	gnome_vfs_async_set_file_info (&handle, 
			uri, row_data->info, 
			GNOME_VFS_SET_FILE_INFO_NAME, 
			GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
			GFM_VFS_PRIORITY_CHANGE_FILE_INFO,
			(GnomeVFSAsyncSetFileInfoCallback) set_file_info_callback,
			cdata);
	TRACE
	gnome_vfs_uri_unref (uri);
}

static void
set_file_info_callback (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, GnomeVFSFileInfo *file_info, 
			struct SetFileInfoCallbackData *cdata)
{
	if (result != GNOME_VFS_OK) {
		report_gnome_vfs_error (result, TRUE);
		goto done;
	}

	TRACE
	gnome_vfs_file_info_unref (cdata->row_data->info);
	cdata->row_data->info = file_info;
	gnome_vfs_file_info_ref (file_info);
	gtk_list_store_set (cdata->model, cdata->iter, 
		GFM_COLUMN_DATA, cdata->row_data, 
		GFM_COLUMN_NAME, NULL,  /* redraw name */
		GFM_COLUMN_PERM, NULL,  /* redraw permissions */
		-1);
done:
	TRACE
	gfm_row_data_free (cdata->row_data);
	gtk_tree_iter_free (cdata->iter);
	g_free (cdata);
}


static void 
change_permissions (GtkCellRendererText *cellrenderer, gchar *path_string, gchar *new_value, gint panel)
{
	GtkTreeIter iter;
	GfmRowData *row_data;
	GtkTreeModel *model;
	GnomeVFSURI *uri;
	struct SetFileInfoCallbackData *cdata;
	GnomeVFSAsyncHandle *handle;
	GnomeVFSFilePermissions new_permissions = 0;
	gint scope, perm;
	GnomeVFSFilePermissions unix_to_vfs [12] = {
		GNOME_VFS_PERM_OTHER_EXEC, GNOME_VFS_PERM_OTHER_WRITE, GNOME_VFS_PERM_OTHER_READ,
		GNOME_VFS_PERM_GROUP_EXEC, GNOME_VFS_PERM_GROUP_WRITE, GNOME_VFS_PERM_GROUP_READ,
		GNOME_VFS_PERM_USER_EXEC, GNOME_VFS_PERM_USER_WRITE, GNOME_VFS_PERM_USER_READ,
		GNOME_VFS_PERM_STICKY, GNOME_VFS_PERM_SGID, GNOME_VFS_PERM_SUID,

	}; /* indexed by position in binary representation */

	
	TRACEM("%s\n",new_value);
	g_return_if_fail (IS_PANEL(panel));
	g_return_if_fail (new_value != NULL && new_value[0]!='\0');

	/* check if new value is correct permissions string and construct GnomeVFSFilePermissions */
	if (new_value[0] >= '0' && new_value[0] <= '7') {
		/* numerical format */
		TRACE
		if (strlen (new_value)>4 || strlen(new_value)<3) 
			goto format_error;
		for (scope = ((strlen(new_value)==4)?3:2); *new_value; new_value++, scope--) {
			TRACEM ("%c\n",*new_value);
			if (*new_value<'0' || *new_value>'7')
				goto format_error;
			
			for (perm = 2; perm>=0; perm--) 
				if (((*new_value-'0')&(1<<perm))>>perm)
					new_permissions += unix_to_vfs [3*scope+perm];
		}
	} else { 
		/* rwx format */
		if (strlen (new_value)!=9)
			goto format_error;
		for (scope = 2, perm=2; *new_value; new_value++, perm--) {
			if (perm<0) {
				perm = 2;
				scope-- ;
			}
			if (scope < 0)
				goto format_error;

			TRACEM ("scope: %d perm: %d char:%c\n",scope, perm, *new_value);
			switch (perm) {
				case 2: /* read */
					switch (*new_value) {
						case 'r': new_permissions += unix_to_vfs [3*scope+perm];
						case '-': break;
						default:  goto format_error;
					} break;
				case 1: /* write */
					switch (*new_value) {
						case 'w': new_permissions += unix_to_vfs [3*scope+perm];
						case '-': break;
						default: goto format_error;
					} break;
				case 0: /* execute -- we must handle special set UID/GID/sticky bit */
					switch (*new_value) {
						case 'x': new_permissions += unix_to_vfs [3*scope+perm]; break;
						case 's': 
							if (!scope) goto format_error;
							new_permissions += unix_to_vfs [9+perm];
							break;
						case 't':
							if (scope) goto format_error;
							new_permissions += unix_to_vfs [9+perm];
							break;
						case '-': break;
						default:
							goto format_error;
					}
			}
		}
	}
							

	/* update file info */
	model = GTK_TREE_MODEL(panels_data[panel].files_data);
	
	g_return_if_fail (gtk_tree_model_get_iter_from_string (model, &iter, path_string));

	gtk_tree_model_get (model, &iter, GFM_COLUMN_DATA, &row_data, -1);
	g_return_if_fail (row_data!=NULL);
	g_return_if_fail (row_data->info!=NULL);

	uri = gnome_vfs_uri_append_file_name (panels_data[panel].uri_wdir, row_data->info->name);
	
	row_data->info->permissions = new_permissions;
	
	g_return_if_fail ((cdata = g_new (struct SetFileInfoCallbackData, 1)));
	cdata->model = GTK_LIST_STORE(model);
	cdata->iter = gtk_tree_iter_copy (&iter);
	cdata->row_data = row_data;

	TRACE
	gnome_vfs_async_set_file_info (&handle, 
			uri, row_data->info, 
			GNOME_VFS_SET_FILE_INFO_PERMISSIONS, 
			GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
			GFM_VFS_PRIORITY_CHANGE_FILE_INFO,
			(GnomeVFSAsyncSetFileInfoCallback) set_file_info_callback,
			cdata);
	TRACE
	gnome_vfs_uri_unref (uri);
	return;

format_error: 
	report_gnome_vfs_error (GNOME_VFS_ERROR_WRONG_FORMAT, TRUE);	
	return;
}


/*
 * gconf_client_notify_handler()
 */
static void
gconf_client_notify_handler	(GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data)
{
	gint panel = GPOINTER_TO_INT (user_data);
	gchar *leaf;
	
	g_return_if_fail (entry != NULL);
	g_return_if_fail (IS_PANEL(panel));
	g_return_if_fail (entry->key != NULL);

	leaf = strrchr (entry->key, '/');
	g_return_if_fail (leaf != NULL);
	leaf++; /* skip '/' */
TRACEM("Leaf: %s\n", leaf);

	if (!strcmp (leaf, "column-types")) {
		update_file_list_settings (panel);
	} else 
	if (!strcmp (leaf, "column-widths")) {
		load_column_widths(panel);
		apply_column_widths(panel);
	}

}

/*
 * load_column_widths
 */

static void
load_column_widths (gint panel)
{
	GConfClient *client = gconf_client_get_default ();
	GError *error = NULL;
	GSList *column_widths = NULL, *it;
	gchar *key;
	gint c;

	g_return_if_fail (client != NULL);

TRACEM("panel: %d\n", panel);
	key = g_strconcat (CONFIGURATION_BASE, panel_names[panel], "file-list/column-widths", NULL);
	if (!key) {
		g_warning (G_STRLOC ": could not create key\n");
		return;
	}
TRACEM("key: %s\n", key);

	column_widths = gconf_client_get_list (client, key, GCONF_VALUE_INT ,&error);
	free (key);

TRACE
	if (!column_widths) {
		g_warning (G_STRLOC "could not load fixed widths from GConf -- %s", error?error->message:"NULL");
		TRACE
		if (error) g_error_free (error);
		return;
	}

TRACE
	for (it = column_widths, c = GFM_COLUMN_NAME;
		it && c < GFM_NUM_COLUMNS;
		it = it->next, c++)
	{
		gint w = GPOINTER_TO_INT (it->data);
TRACE
		if (w>0)
			file_list_column_widths[panel][c] = w;
	}
TRACE
	g_slist_free (column_widths);
}

/*
 * store_column_widths()
 */
static void
store_column_widths (gint panel)
{
	GConfClient *client;
	gchar *key;
	GSList *values = NULL;
	GList *columns = NULL, *col;
	gint c, type;
	gboolean dirty = FALSE;

	g_return_if_fail (IS_PANEL(panel));


TRACE
	columns = gtk_tree_view_get_columns (panels_data[panel].files_view);
	for (col = columns; col; col = col->next)  {
		type = gtk_tree_view_column_get_sort_column_id (
					GTK_TREE_VIEW_COLUMN(col->data));
		
		c = gtk_tree_view_column_get_width (GTK_TREE_VIEW_COLUMN(col->data));
		if (c != file_list_column_widths [panel][type])
			dirty = TRUE;
		file_list_column_widths [panel][type] = c;
	}
	g_list_free (columns);

	if (!dirty) {
		TRACE
		return;
	}
	
	client = gconf_client_get_default ();
	g_return_if_fail (client != NULL);
TRACE
	for (c = GFM_COLUMN_NAME;
		c < GFM_NUM_COLUMNS;
		c++)
	{
TRACE
TRACEM("%d-%d:%d [%d]\n", panel, c, file_list_column_widths[panel][c],g_slist_length(values));
		values = g_slist_prepend (values, GINT_TO_POINTER(file_list_column_widths[panel][c]));
TRACE
	}
	values = g_slist_reverse (values);

TRACE

	key = g_strconcat (CONFIGURATION_BASE, panel_names [panel], "file-list/column-widths", NULL);
	if (key) {
		if (!gconf_client_set_list (client, key, GCONF_VALUE_INT, values, NULL))
			g_warning ("%s not saved!", key);
		free (key);
	} else g_warning (G_STRLOC": out of memory!");
	g_slist_free (values);

TRACE
	g_object_unref (G_OBJECT(client));
}

/*
 * apply_column_widths()
 */
static void apply_column_widths (gint panel)
{
	GList *columns = NULL, *col;
	gint type;

	g_return_if_fail (IS_PANEL(panel));

TRACE
	columns = gtk_tree_view_get_columns (panels_data[panel].files_view);
		
	for (col = columns; col; col = col->next)  {
		type = gtk_tree_view_column_get_sort_column_id (
					GTK_TREE_VIEW_COLUMN(col->data));

		
		g_object_set (G_OBJECT(col->data),
			"fixed-width", file_list_column_widths [panel][type],
			NULL);
	}
	g_list_free (columns);

}

/*
 * on_file_list_size_allocate
 */
void
on_file_list_size_allocate	(GtkWidget *widget,
				GtkAllocation *allocation,
				gint panel)
{
	if (!widget || !GTK_WIDGET_VISIBLE(widget))
		return;

	g_return_if_fail (IS_PANEL(panel));

TRACE
	store_column_widths (panel);
}
