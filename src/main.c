#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include "file_list.h"
#include "file_list_icons.h"
#include "interface.h"

#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

int
main (int argc, char *argv[])
{
  GError *error;

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

TRACE
  gnome_program_init(PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv, 
	GNOME_PARAM_APP_PREFIX, PREFIX,
	GNOME_PARAM_APP_DATADIR, DATADIR,
	GNOME_PARAM_HUMAN_READABLE_NAME, "Gnome FileManager",
	GNOME_PARAM_NONE);
            
TRACE

  gnome_vfs_init();
  if (! gnome_vfs_initialized()) {
	  printf(_("Gnome VFS init failed.\n"));
	  return 1;
  }
  atexit (gnome_vfs_shutdown);

  if (! gconf_init (argc, argv, &error)) {
	g_print (_("GConf init failed: %s\n"), error->message);
	g_error_free (error);
  }
  
TRACE
  file_list_icons_init();
  /*
   * we can't do this, since create_main_windows must be called from inside of gtk_main
   *
  main_window = create_widget ("main_window");

  gtk_init_add ((GtkFunction) gtk_widget_show, main_window);
  */
TRACE
  gtk_init_add ((GtkFunction) create_main_window, NULL); 
  gtk_main ();
 
  return 0; /* useless, just to avoid warnings */
}

