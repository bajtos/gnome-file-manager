#ifndef __FILE_LIST_H__
#define __FILE_LIST_H__
#include <gnome.h>
#include <gfm_vfs.h>


/*
 * updates file list 
 */
void
file_list_update (gint panel);

/*
 * changes directory displayed in file_list
 */
void
file_list_chdir (gint panel, GnomeVFSURI *uri);


/*
 * aborts currently running operation (mainly reading directory, ...)
 */
void
file_list_abort (gint panel);


void
file_list_toggle_hidden_files (gint panel);


#endif
