#include <stdarg.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "gfm_vfs.h"

#include "gfm_list_store.h"

#ifndef DEBUG
#define DEBUG 0
#endif 
#include "debug.h"


/** S O R T I N G **/


gint
gfm_list_store_sort_type	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_name	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_size	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_time	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_time	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_time	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_perm	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);

gint
gfm_list_store_sort_guid	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data);


GtkTreeIterCompareFunc gfm_list_store_compare_functions[GFM_NUM_COLUMNS] = {
	NULL,
	gfm_list_store_sort_type,
	gfm_list_store_sort_name,
	gfm_list_store_sort_size,
	gfm_list_store_sort_time,
	gfm_list_store_sort_time,
	gfm_list_store_sort_time,
	gfm_list_store_sort_perm,
	gfm_list_store_sort_guid,
	gfm_list_store_sort_guid
};






GType gfm_list_store_column_types[GFM_NUM_COLUMNS] = {
	-1,		/* GFM_COLUMN_DATA: GfmRowData as G_TYPE_BOXED */ 
		//FIXME: this problem should be fixed in gnome-vfs soon -> use something like G_TYPE_GNOME_VFS_INFO
	G_TYPE_OBJECT,	/* GFM_COLUMN_ICON: GdkPixBuf */
	G_TYPE_STRING,  /* GFM_COLUMN_NAME */
	G_TYPE_STRING,  /* GFM_COLUMN_SIZE */
	G_TYPE_STRING,  /* GFM_COLUMN_MTIME */
	G_TYPE_STRING,  /* GFM_COLUMN_ATIME */
	G_TYPE_STRING,  /* GFM_COLUMN_CTIME */
	G_TYPE_STRING,	/* GFM_COLUMN_PERM */
	G_TYPE_STRING,	/* GFM_COLUMN_UID */
	G_TYPE_STRING	/* GFM_COLUMN_GID */
};

#define G_SLIST(l) ((GSList *)(l))


GtkType
gfm_list_store_get_type	(void)
{
	static GType list_store_type = 0;

	if (!list_store_type) {
		static const GTypeInfo list_store_info = {
			sizeof (GfmListStoreClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			NULL,		/* class_init */
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GfmListStore),
			0,
			NULL,		/* instance_init ... everything is done in GtkListStore */
		};

		/* 
		 * register new boxed type, since gnome-vfs does not define g_type for GnomeVFSInfo 
		 * -> we will have dup&free in list store "for free"
		 */
		g_assert (gfm_list_store_column_types[GFM_COLUMN_DATA] == -1);
		gfm_list_store_column_types [GFM_COLUMN_DATA] = g_boxed_type_register_static (
			"gfm_row_data", 
			(GBoxedCopyFunc) gfm_row_data_dup,
			(GBoxedFreeFunc) gfm_row_data_free); 

		/*
		 * register our type
		 */
		list_store_type = g_type_register_static (GTK_TYPE_LIST_STORE, "GfmListStore", &list_store_info, 0);

	}
					
	return list_store_type;
}

GfmListStore * 
gfm_list_store_new		(void)
{
	GtkListStore *self;
	gint i;

	self = GTK_LIST_STORE (g_object_new (gfm_list_store_get_type (), NULL));

	gtk_list_store_set_column_types (self, GFM_NUM_COLUMNS, gfm_list_store_column_types);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE(self), gfm_list_store_compare_functions[GFM_COLUMN_NAME], 
			GINT_TO_POINTER (GFM_COLUMN_NAME), NULL);
	for (i=GFM_COLUMN_ICON; i<GFM_NUM_COLUMNS; i++) 
		gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(self), i, 
			gfm_list_store_compare_functions[i], GINT_TO_POINTER(i), 
			NULL);
	return GFM_LIST_STORE(self);
}



#define SORT_COLUMN GPOINTER_TO_INT(data)

/* 
 * special comparison for directories. this code is called from each sort callback
 */
#define LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)  {\
	gint is_dir_a = gfm_is_directory(row_data_a->real_info?:row_data_a->info)?1:0;\
	gint is_dir_b = gfm_is_directory(row_data_b->real_info?:row_data_b->info)?1:0;\
	gint mod = (GTK_LIST_STORE(model)->order==GTK_SORT_ASCENDING?-1:1);\
	if (strcmp(finfo_a->name, "..") == 0) return mod;\
	if (strcmp(finfo_b->name, "..") == 0) return -mod;\
	if (is_dir_a ^ is_dir_b) \
		return mod*(is_dir_a - is_dir_b);\
}
	

gint
gfm_list_store_sort_type	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	GfmRowData *row_data_a, *row_data_b;
	GnomeVFSFileInfo *finfo_a, *finfo_b;
	gchar *mime_type_a, *mime_type_b;
	gint ret;
	gint is_exec_a, is_exec_b;
	gint mod = (GTK_LIST_STORE(model)->order==GTK_SORT_ASCENDING?-1:1);\

	gtk_tree_model_get (model, a, GFM_COLUMN_DATA, &row_data_a, -1);  
	gtk_tree_model_get (model, b, GFM_COLUMN_DATA, &row_data_b, -1);  
	finfo_a = row_data_a->real_info?:row_data_a->info;
	finfo_b = row_data_b->real_info?:row_data_b->info;

	LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)

	is_exec_a = GFM_IS_EXECUTABLE (finfo_a);
	is_exec_b = GFM_IS_EXECUTABLE (finfo_b);
	if (is_exec_a ^ is_exec_b)
		return mod*(is_exec_a - is_exec_b);

	

	if (finfo_a->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
		mime_type_a = finfo_a->mime_type;
	else
		mime_type_a = NULL;

	if (finfo_b->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
		mime_type_b = finfo_b->mime_type;
	else
		mime_type_b = NULL;

	if (!mime_type_a) 
		mime_type_a = "";
	if (!mime_type_b) 
		mime_type_b = "";

	ret = strcmp (mime_type_a, mime_type_b);
	if (ret == 0)
		ret = strcmp (finfo_a->name, finfo_b->name);
	return ret;
}


gint
gfm_list_store_sort_name	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	GfmRowData *row_data_a, *row_data_b;
	GnomeVFSFileInfo *finfo_a, *finfo_b;
	
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (a != NULL || b != NULL, 0);

	gtk_tree_model_get (model, a, GFM_COLUMN_DATA, &row_data_a, -1);  
	gtk_tree_model_get (model, b, GFM_COLUMN_DATA, &row_data_b, -1);  
	finfo_a = row_data_a -> info;
	finfo_b = row_data_b -> info;

	LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)

	return strcmp (finfo_a->name, finfo_b->name);
}

gint
gfm_list_store_sort_size	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	GfmRowData *row_data_a, *row_data_b;
	GnomeVFSFileInfo *finfo_a, *finfo_b;
	GnomeVFSFileSize size_a, size_b;
	gint ret;
	
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (a != NULL || b != NULL, 0);
	
	gtk_tree_model_get (model, a, GFM_COLUMN_DATA, &row_data_a, -1);  
	gtk_tree_model_get (model, b, GFM_COLUMN_DATA, &row_data_b, -1);  
	finfo_a = row_data_a -> info;
	finfo_b = row_data_b -> info;

	LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)

	if (finfo_a->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE)
		size_a = finfo_a->size;
	else
		size_a = -1;

	if (finfo_b->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE)
		size_b = finfo_b->size;
	else
		size_b = -1;

	
	ret = size_a-size_b;
	if (ret == 0)
		ret = strcmp (finfo_a->name, finfo_b->name);
	return ret;
}

gint
gfm_list_store_sort_time	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	GfmRowData *row_data_a, *row_data_b;
	GnomeVFSFileInfo *finfo_a, *finfo_b;
	time_t time_a = 0, time_b = 0;
	gint ret = 1;
	
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (a != NULL || b != NULL, 0);
	
	gtk_tree_model_get (model, a, GFM_COLUMN_DATA, &row_data_a, -1);  
	gtk_tree_model_get (model, b, GFM_COLUMN_DATA, &row_data_b, -1);  
	finfo_a = row_data_a -> info;
	finfo_b = row_data_b -> info;

	LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)

	switch (SORT_COLUMN) {
	  case GFM_COLUMN_MTIME:
		if (finfo_a->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
			time_a = finfo_a->mtime;
		if (finfo_b->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
			time_b = finfo_b->mtime;
		break;

	  case GFM_COLUMN_CTIME:
		if (finfo_a->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_CTIME)
			time_a = finfo_a->ctime;
		if (finfo_b->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_CTIME)
			time_b = finfo_b->ctime;
		break;

	  case GFM_COLUMN_ATIME:
		if (finfo_a->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_ATIME)
			time_a = finfo_a->atime;
		if (finfo_b->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_ATIME)
			time_b = finfo_b->atime;
		break;

	  default: 
			ret = 0;
	}

	if (ret)
		ret = time_a - time_b;
	if (ret == 0)
		ret = strcmp (finfo_a->name, finfo_b->name);
	return ret;
	
}


gint
gfm_list_store_sort_perm	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	GfmRowData *row_data_a, *row_data_b;
	GnomeVFSFileInfo *finfo_a, *finfo_b;
	GnomeVFSFilePermissions perm_a, perm_b;
	gint32 pa, pb;
	
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (a != NULL || b != NULL, 0);
	
	gtk_tree_model_get (model, a, GFM_COLUMN_DATA, &row_data_a, -1);  
	gtk_tree_model_get (model, b, GFM_COLUMN_DATA, &row_data_b, -1);  
	finfo_a = row_data_a -> info;
	finfo_b = row_data_b -> info;

	LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)

	if (finfo_a->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)
		perm_a = finfo_a->permissions;
	else
		perm_a = 0;

	if (finfo_b->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)
		perm_b = finfo_b->permissions;
	else
		perm_b = 0;

	pa = perm_a;
	pb = perm_b;
#define PERM(a,x) ((a&GNOME_VFS_PERM_##x)?1:0)
	perm_a = (((((((((((	 PERM(pa, SUID) * 2
				+PERM(pa, SGID)) * 2
				+PERM(pa, STICKY)) * 2
				+PERM(pa, USER_READ)) * 2
				+PERM(pa, USER_WRITE)) * 2
				+PERM(pa, USER_EXEC)) * 2
				+PERM(pa, GROUP_READ)) * 2
				+PERM(pa, GROUP_WRITE)) * 2
				+PERM(pa, GROUP_EXEC)) * 2
				+PERM(pa, OTHER_READ)) * 2
				+PERM(pa, OTHER_WRITE)) * 2
				+PERM(pa, OTHER_EXEC));
	perm_b = (((((((((((	 PERM(pb, SUID) * 2
				+PERM(pb, SGID)) * 2
				+PERM(pb, STICKY)) * 2
				+PERM(pb, USER_READ)) * 2
				+PERM(pb, USER_WRITE)) * 2
				+PERM(pb, USER_EXEC)) * 2
				+PERM(pb, GROUP_READ)) * 2
				+PERM(pb, GROUP_WRITE)) * 2
				+PERM(pb, GROUP_EXEC)) * 2
				+PERM(pb, OTHER_READ)) * 2
				+PERM(pb, OTHER_WRITE)) * 2
				+PERM(pb, OTHER_EXEC));
#undef PERM
#if DEBUG  
printf("%s %d (%d) - %s %d (%d) \n", finfo_a->name, perm_a, pa, finfo_b->name, perm_b, pb);
#endif 
	return (perm_a - perm_b)?: strcmp (finfo_a->name, finfo_b->name);
}

gint
gfm_list_store_sort_guid	(GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	GfmRowData *row_data_a, *row_data_b;
	GnomeVFSFileInfo *finfo_a, *finfo_b;
	guint guid_a = 0, guid_b = 0;
	gint ret=1;
	
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (a != NULL || b != NULL, 0);

	gtk_tree_model_get (model, a, GFM_COLUMN_DATA, &row_data_a, -1);  
	gtk_tree_model_get (model, b, GFM_COLUMN_DATA, &row_data_b, -1);  
	finfo_a = row_data_a -> info;
	finfo_b = row_data_b -> info;

	LIST_DIRECTORIES_FIRST(row_data_a, row_data_b)
	
	switch (SORT_COLUMN) {
		case GFM_COLUMN_UID:
			TRACE
			guid_a = finfo_a->uid;
			guid_b = finfo_b->uid;
			break;
		
		case GFM_COLUMN_GID:
			TRACE
			guid_a = finfo_a->gid;
			guid_b = finfo_b->gid;
			break;

		default:
			TRACE
			ret = 0;
	}

	if (ret)
		ret = guid_a - guid_b; /* TODO: sort according to user/group names? */
	if (ret == 0)
		ret = strcmp (finfo_a->name, finfo_b->name);
	return ret;

}

#undef SORT_COLUMN

GfmRowData * gfm_row_data_new (void)
{
	GfmRowData *ret;

	ret = g_new0 (GfmRowData, 1);
	g_return_val_if_fail (ret != NULL, ret);

	return ret;
}

GfmRowData * gfm_row_data_dup (GfmRowData *row_data)
{
	GfmRowData *ret;

	g_return_val_if_fail (row_data, NULL);

	ret = gfm_row_data_new ();
	if (!ret)
		return ret;

	if (row_data->info)
		ret->info = gnome_vfs_file_info_dup (row_data->info);
	if (row_data->real_info)
		ret->real_info = gnome_vfs_file_info_dup (row_data->real_info);

	return ret;
}

void gfm_row_data_free (GfmRowData *row_data)
{
	g_return_if_fail (row_data);
	
	if (row_data->info) {
		gnome_vfs_file_info_unref (row_data->info);
		row_data->info = NULL;
	}

	if (row_data->real_info) {
		gnome_vfs_file_info_unref (row_data->real_info);
		row_data->real_info = NULL;
	}

	g_free (row_data);
}
