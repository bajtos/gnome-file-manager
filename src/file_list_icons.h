#ifndef __FILE_LIST_ICONS_H__
#define __FILE_LIST_ICONS_H__

#include "gfm_vfs.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

void
file_list_icons_init (void);

GdkPixbuf *
file_list_icons_get_icon (const GnomeVFSFileInfo *info);

#endif
