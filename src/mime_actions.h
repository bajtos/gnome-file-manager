#ifndef __MIME_ACTIONS_H__
#define __MIME_ACTIONS_H__

#include <gtk/gtk.h>

void
execute_default_action		(GtkTreeView *view, GtkTreePath *path);

void
edit_with_default_editor	(GtkTreeView *view, GtkTreePath *path);
#endif /* __MIME_ACTIONS_H__ */
