#include "mime_actions.h"
#include "gfm_vfs.h"
#include "interface.h"
#include "gfm_list_store.h"
#include "file_list.h"
#include <glib.h>
#include <string.h>

#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

static void
execute_mime_application	(const GnomeVFSMimeApplication *application, const gchar *parameters);

/* performs some magic and guess if application->command needs to run in terminal */
static void
set_requires_terminal (GnomeVFSMimeApplication *application);

/* returns filename in format suitable for passing to shell */
static char *
get_quoted_filename_from_uri	(const GnomeVFSURI *uri, GnomeVFSMimeApplicationArgumentType format);


void
execute_default_action		(GtkTreeView *view, GtkTreePath *path)
{

	GtkTreeModel *model;
	GtkTreeIter iter;
	
	GfmRowData *data;
	GnomeVFSFileInfo *info = NULL;
	GnomeVFSURI *uri, *new_uri;
	gint panel;
	GnomeVFSMimeApplication *application;
	gchar *filename;

	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	
TRACE
	model = gtk_tree_view_get_model (view);
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_model_get (model, &iter, GFM_COLUMN_DATA, &data, -1);
	
	panel = get_panel_id(view);
	uri = panels_data [panel].uri_wdir;
	g_return_if_fail (uri != NULL);

	/* change directory */
	info = data->real_info?:data->info;
	if (gfm_is_directory (info)) {
		if (strcmp(info->name, ".."))
			new_uri = gnome_vfs_uri_append_path (uri, info->name);
		else 
			new_uri = gnome_vfs_uri_get_parent (uri);
		

		file_list_chdir (panel, new_uri);
		gnome_vfs_uri_unref (new_uri);
		return;
	}

	/* execute program/application */
	if (GFM_IS_EXECUTABLE (data->info) && (!data->real_info || GFM_IS_EXECUTABLE (data->real_info))) {
		GnomeVFSMimeApplication app;

		app.id = NULL;
		app.name = NULL;
		app.can_open_multiple_files = FALSE;
		app.supported_uri_schemes = NULL;
		app.expects_uris = GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_PATHS;
		
		new_uri = gnome_vfs_uri_append_path (uri, info->name);
		app.command = get_quoted_filename_from_uri (uri, GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_PATHS); 
		gnome_vfs_uri_unref (new_uri);

		set_requires_terminal (&app);

		execute_mime_application (&app, NULL);
		g_free (app.command);

		return;
	}
		
	/* run default application */
	info = data->real_info?:data->info;
	switch (gnome_vfs_mime_get_default_action_type (info->mime_type)) {
		case GNOME_VFS_MIME_ACTION_TYPE_NONE:
			TRACEM("No default action for %s", info->mime_type);
			break;
		case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
			TRACEM("Bonobo components not supported");
		case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
			application = gnome_vfs_mime_get_default_application (info->mime_type);
			g_return_if_fail (application != NULL);
			new_uri = gnome_vfs_uri_append_path (uri, info->name);
			filename = get_quoted_filename_from_uri (
					new_uri, 
					application->expects_uris); 
			if (filename) {
				execute_mime_application (application, filename);
				g_free (filename);
			} else {
				g_warning (G_STRLOC": out of memory.");
			}
			gnome_vfs_uri_unref (new_uri);
			break;
		default:
			g_assert_not_reached();
	}
}


void
edit_with_default_editor	(GtkTreeView *view, GtkTreePath *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GfmRowData *row_data = NULL;
	GnomeVFSURI *uri = NULL;
	gint panel;
	gchar *filename = NULL;
	GnomeVFSFileInfo *info;
	GnomeVFSMimeApplication *editor;
		
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	
	panel = get_panel_id (view);
	model = GTK_TREE_MODEL(panels_data[panel].files_data);

	g_return_if_fail (GFM_URI_IS_LOCAL (panels_data[panel].uri_wdir));
	
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_model_get (model, &iter, GFM_COLUMN_DATA, &row_data,  -1);
	
	info = row_data->real_info?:row_data->info;
	if (info->type != GNOME_VFS_FILE_TYPE_REGULAR) goto done;

	uri = gnome_vfs_uri_append_file_name (panels_data[panel].uri_wdir, row_data->info->name);

	/* get default editor */
	editor = gnome_vfs_mime_get_default_application ("text/plain"); /* this is according to control-center */
	if (!editor) goto done;

	filename = get_quoted_filename_from_uri (uri, 
			editor->expects_uris);
	
	execute_mime_application (editor, filename);

	gnome_vfs_mime_application_free (editor);
	
done:
	if (uri) gnome_vfs_uri_unref (uri);
	if (row_data) gfm_row_data_free (row_data);
	if (filename) g_free (filename);

}

#include <errno.h>
#include <string.h>

static void
execute_mime_application	(const GnomeVFSMimeApplication *application, const gchar *parameters )
{
	gchar *cmd;
	gint pid;

	g_return_if_fail (application != NULL);
	
	if (parameters)
		cmd = g_strconcat (application->command, " ", parameters, NULL);
	else 
		cmd = application->command;
	g_return_if_fail (cmd);
	
TRACEM("executing '%s'\n", cmd);
	if (application->requires_terminal) {
		TRACE
		pid = gnome_execute_terminal_shell (NULL, cmd);
	} else {
		TRACE
		pid = gnome_execute_shell (NULL, cmd);
	}
	
	if (pid == -1)
		g_warning (G_STRLOC ": Could not fork - %s", strerror (errno));

	if (parameters)
		g_free (cmd);
}

#include <unistd.h>
#include <sys/wait.h>
static void
set_requires_terminal (GnomeVFSMimeApplication *application)
{
	gchar *cmd;
	int pid, status;
	int fdstdout, pipe_fd[2];

	g_return_if_fail (application != NULL);
	
	application->requires_terminal = TRUE;
	g_return_if_fail (application->command != NULL);
	
	/* 
	 * check if application is dynamically linked with libX11 
	 */
	cmd = g_strconcat ("ldd ",application->command, " 2>/dev/null| grep -q libX11 2>/dev/null; echo $?", NULL);
	g_return_if_fail (cmd);
	
	if (pipe(pipe_fd)) {
		g_free (cmd);
		g_warning (G_STRLOC ": Could not create pipe - %s", strerror (errno));
		return;
	}

	/* set up pipe to catch output of cmd */
	fdstdout = dup (1);
	dup2 (pipe_fd[1], 1);
	
	pid = gnome_execute_shell (NULL, cmd);
	g_free (cmd);
	
	/* restore output */
	dup2(fdstdout, 1);
	
	/* process child's output */
	if (pid == -1)
		g_warning (G_STRLOC ": Could not fork - %s", strerror (errno));
	else {
		char res;
		waitpid (pid, &status, 0);
		if (read (pipe_fd[0], &res, 1)==1) {
			TRACEM("%c", res);
			if (res == '0')
				application->requires_terminal = FALSE;
		} else {
			TRACEM("nothing");
		}
			
	}
		
	close (pipe_fd[0]); close (pipe_fd[1]);
}

static char *
get_quoted_filename_from_uri	(const GnomeVFSURI *uri, GnomeVFSMimeApplicationArgumentType format)
{
	gchar *unquoted, *quoted;
	gint fields;
	gboolean uri_format;
	
	g_return_val_if_fail (uri != NULL, NULL);


	uri_format = format == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS
		|| (format == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS_FOR_NON_FILES && !GFM_URI_IS_LOCAL(uri));
			
	if (uri_format)
		fields = GNOME_VFS_URI_HIDE_NONE;
	else
		fields = GNOME_VFS_URI_HIDE_USER_NAME |
			GNOME_VFS_URI_HIDE_PASSWORD |
			GNOME_VFS_URI_HIDE_HOST_NAME |
			GNOME_VFS_URI_HIDE_HOST_PORT |
			GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD |
			GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER;

TRACE
	unquoted = gnome_vfs_uri_to_string (uri, fields);

	if (unquoted && !uri_format) {
		gchar *temp;
		temp = unquoted;
		
TRACE
		unquoted = gnome_vfs_unescape_string_for_display (temp);
		g_free (temp);
	}

	g_return_val_if_fail (unquoted!=NULL, NULL);
TRACEM("unquoted: %s\n", unquoted);
	quoted = g_shell_quote (unquoted);
TRACEM("quoted: %s\n", quoted);
	g_free (unquoted);
TRACE
	return quoted;
}
