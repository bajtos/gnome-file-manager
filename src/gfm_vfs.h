#ifndef __GFM_VFS_H__
#define __GFM_VFS_H__

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>


enum GfmVfsPriorities {

	GFM_VFS_PRIORITY_LOADDIR = -10,
	GFM_VFS_PRIORITY_CHANGE_FILE_INFO = 0,
	GFM_VFS_PRIORITY_XFER = 5,
	GFM_VFS_PRIORITY_RESOLVE_SPECIAL_FILE = 5,
	GFM_VFS_PRIORITY_LISTER = 1,

	_end_GFM_VFS_PRIORITY_
};

/*
 * buggy gnome-vfs 2.0 (TODO: check, if this is fixed in recent version)
 * doesn't set GnomeVFSInfo->type properly (or it doesn't support things like
 * symlink to directory -> we solve this through special_files & mime_type
 */
gboolean
gfm_is_directory(const GnomeVFSFileInfo *info);

#define GFM_IS_EXECUTABLE(info) (info && (\
	/* regular file */\
	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) &&\
	(info->type == GNOME_VFS_FILE_TYPE_REGULAR) &&\
	/* on local filesystem */\
	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_FLAGS) &&\
	(info->flags & GNOME_VFS_FILE_FLAGS_LOCAL) &&\
	/* weak permission check */\
	(info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS) &&\
	(info->permissions & (	GNOME_VFS_PERM_USER_EXEC |\
				GNOME_VFS_PERM_GROUP_EXEC |\
				GNOME_VFS_PERM_OTHER_EXEC ))\
	))


#define GFM_URI_IS_LOCAL(uri) ((uri) && !strcmp(gnome_vfs_uri_get_scheme(uri), "file") && gnome_vfs_uri_is_local (uri))

#define GNOME_VFS_FILE_INFO(info) ((GnomeVFSFileInfo*)info)


gchar *
human_readable_file_size (GnomeVFSFileSize size);


void
report_gnome_vfs_error (GnomeVFSResult error, gboolean popup);


#endif /* __GFM_VFS_H__ */
