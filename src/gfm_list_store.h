#ifndef __GFM_LIST_STORE_H__
#define __GFM_LIST_STORE_H__

/*************************************
 * 
 * 	GfmListStore: extension of GtkListStore, difference is in sorting
 *
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>
#include "gfm_vfs.h"

enum _GfmListStoreColumns {
	GFM_COLUMN_DATA,
	GFM_COLUMN_ICON,
	GFM_COLUMN_NAME,
	GFM_COLUMN_SIZE,
	GFM_COLUMN_MTIME,
	GFM_COLUMN_ATIME,
	GFM_COLUMN_CTIME,
	GFM_COLUMN_PERM,
	GFM_COLUMN_UID,
	GFM_COLUMN_GID,

	GFM_NUM_COLUMNS
};

typedef enum _GfmListStoreColumns GfmListStoreColumns;

extern GType gfm_list_store_column_types[GFM_NUM_COLUMNS];

#define GFM_TYPE_LIST_STORE            (gfm_list_store_get_type ())
#define GFM_LIST_STORE(obj)            (GTK_CHECK_CAST ((obj), GFM_TYPE_LIST_STORE, GfmListStore))
#define GFM_LIST_STORE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GFM_TYPE_LIST_STORE, GfmListStoreClass))
#define GFM_IS_LIST_STORE(obj)         (GTK_CHECK_TYPE ((obj), GFM_TYPE_LIST_STORE))
#define GFM_IS_LIST_STORE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GFM_TYPE_LIST_STORE))
#define GFM_LIST_STORE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GFM_TYPE_LIST_STORE, GfmListStoreClass))

typedef struct _GfmListStore GfmListStore;
typedef struct _GfmListStoreClass GfmListStoreClass;
typedef struct _GfmRowData GfmRowData;


GtkType 		gfm_list_store_get_type 	(void);
GfmListStore *		gfm_list_store_new		(void);

GfmRowData * 	gfm_row_data_new			(void);
GfmRowData *	gfm_row_data_dup			(GfmRowData		*row_data);
void		gfm_row_data_free			(GfmRowData		*row_data);




struct _GfmListStore
{
	GtkListStore parent;

	/* private */
	gint sort_column_id;
	GtkSortType sort_order;
};


struct _GfmListStoreClass
{
	GtkListStoreClass parent_class;
};


struct _GfmRowData
{
	GnomeVFSFileInfo *info;
	GnomeVFSFileInfo *real_info;	/* after dereferencing symlink */
};
	
#endif /* __GFM_LIST_STORE_H__ */
