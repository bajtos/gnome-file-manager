#ifndef __COPY_MOVE_DELETE_H__
#define __COPY_MOVE_DELETE_H__

#include "gfm_vfs.h"
#include "file_list.h"
#include <gtk/gtk.h>

/*
 * most operations are done in background, we support unlimited number of concurrect jobs
 * [note: there could be some limitations in gnome-vfs and/or in system resources, of course]
 */

/*
 * copies/moves files from source to target
 * + both uri_lists are destroyed afterwards
 * + file_lists are notified that something
 *   has changed
 * 
 */
void
copy_or_move_uri	       (GList *source_uri_list,
				GList *target_uri_list,
				gint source_panel,
				gboolean move);

void
create_symlink_uri		(const GnomeVFSURI *target_reference,
				GnomeVFSURI *uri);
void
delete_uri		       (GList *uri_list,
				gint source_panel);

/*
 * stops all jobs
 */
void
copy_move_delete_shutdown	(void);

/*
 * wrappers for more convient access from event-handling routines
 */
void
copy_or_move		       (gint source_panel,
				gboolean move);

void
delete_files		       (gint source_panel);
			

void
create_symlink			(gint source_panel);

#endif
