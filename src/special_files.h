#ifndef __SPECIAL_FILES_H__
#define __SPECIAL_FILES_H__

#include "gfm_list_store.h"
#include "gfm_vfs.h"


/*************************************
 *
 * default mime-type
 *
 */

void 
check_set_default_mime_type (GnomeVFSFileInfo *info);


/*************************************
 *
 * handle special file-types (like sym-link, *.desktop, etc.)
 * 
 * usage: 
 * 	initialize list
 * 	add all files with special_files_add_to_list () to the list,
 * 	then run special_files_process_list ()
 */

extern guint special_files_max_symlinks_level; /* maximal level of recursion of symlinks */


/* if *sf_data is NULL then it will be created */
void
special_files_init			(gint panel);

/* iter and row_data will be released inside */
void
special_files_add_file_to_list		(gint panel,
					GtkTreeIter *iter,
					GfmRowData *row_data);

/* sf_data will be released inside */
void
special_files_process_list		(gint panel);

/* stops all operations on panel */
void
special_files_abort			(gint panel);

#endif /* __SPECIAL_FILES_H__ */
