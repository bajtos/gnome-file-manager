#ifndef __FILE_LIST_INTERFACE_H__
#define __FILE_LIST_INTERFACE_H__

#include "interface.h"

/*
 * updates settings of file list in panel, that means at least number/type/width/whatever of columns
 * new values are taken from GConf
 */
void
update_file_list_settings (gint panel);


extern gchar *file_list_column_titles[];

#endif /* __FILE_LIST_INTERFACE_H__ */
