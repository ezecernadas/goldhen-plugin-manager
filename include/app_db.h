/*
 * Installed application enumeration from /system_data/priv/mms/app.db.
 * Requires the elevated credentials from initialize_jbc().
 */
#ifndef APP_DB_H
#define APP_DB_H

#include "structs.h"

/* Returns a list of app_entry_t* sorted by name (case-insensitive).
 * Never returns NULL; on any failure the list is empty and the error is logged. */
list_t* appdb_load_installed(void);

void appdb_free(list_t* list);

#endif /* APP_DB_H */
