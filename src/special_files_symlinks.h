#include "special_files.h"

typedef struct _SymlinksData	SymlinksData;

void
symlinks_init		(SymlinksData **sl_data, gint panel);

void
symlinks_add_file	(SymlinksData *sl_data, 
			GtkTreeIter *iter, 
			const GnomeVFSFileInfo *info,
			const GnomeVFSURI *uri);

void
symlinks_execute	(SymlinksData *sl_data);

void
symlinks_abort		(SymlinksData *sl_data);
