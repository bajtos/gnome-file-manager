#include "gfm_vfs.h"
#include "interface.h"
#include <stdlib.h>
#include <string.h>
#include <gnome.h>

gchar *
human_readable_file_size (GnomeVFSFileSize size)
{
	
	gchar suffixes[] = {'b', 'K', 'M', 'G', 'T', '?', '?', '?'}; /* TODO: add correct suffixes */ 
	gint s=0;

	while (size > 10240) {
		size >>= 10;
		s++;
	}

	return g_strdup_printf ("%"GNOME_VFS_SIZE_FORMAT_STR"%c", size, suffixes[s]);
	
}



gboolean
gfm_is_directory(const GnomeVFSFileInfo *info)
{
	return (!info || (\
		(	((info)->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) &&\
			((info)->type == GNOME_VFS_FILE_TYPE_DIRECTORY)\
		) || (\
			((info)->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) &&\
			!strcmp((info)->mime_type, "x-directory/normal") \
		)));
}


void
report_gnome_vfs_error (GnomeVFSResult error, gboolean popup)
{
	if (popup) {
		GtkWidget *dialog;

		if (!error) return;
		
		dialog = gtk_message_dialog_new (GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Error occured: %s"), gnome_vfs_result_to_string (error));

	} 
	
	gnome_appbar_set_status (app_bar,
		gnome_vfs_result_to_string (error));
	

}

