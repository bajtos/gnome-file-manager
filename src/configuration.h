#include <gnome.h>
#include "interface.h"

#define CONFIGURATION_BASE "/apps/" PACKAGE "/"

#define CONFIGURATION_PANEL_LEFT CONFIGURATION_BASE "panel-left/"
#define CONFIGURATION_PANEL_RIGHT CONFIGURATION_BASE "panel-right/"

extern char *panel_names[NUM_PANELS];

void
configuration_wd_save (GnomeEntry *gentry);

void
configuration_wd_load (GnomeEntry *gentry);


void
configuration_main_win_load (GtkWindow *window);

void
configuration_main_win_store (GtkWindow *window);


void
configuration_paned_load (GtkPaned *paned);

void
configuration_paned_store (GtkPaned *paned);

void
configuration_show_hidden_files_load (GtkCheckMenuItem *item);

void
configuration_show_hidden_files_store (GtkCheckMenuItem *item);

