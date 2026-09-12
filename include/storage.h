/*
 * On-disk persistence: plugin directory listing, JSON sidecar and
 * atomic regeneration of /data/GoldHEN/plugins.ini.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "structs.h"
#include "ini_model.h"

/* Appends the full path (malloc'd char*) of every *.prx in GOLDHEN_PLUGINS_DIR
 * to out, sorted case-insensitively. Returns the number of entries. */
int storage_list_prx(list_t* out);
void storage_free_prx(list_t* list);

/* Loads plugin_manager.json into m, or imports plugins.ini on first run
 * (writing only the sidecar; plugins.ini is left untouched). Always ensures
 * the "default" target exists. A malformed sidecar is moved aside and the ini
 * re-imported. Returns 0 on success, -1 on failure (m is left empty). */
int storage_load_state(pm_model_t* m);

/* Backs up plugins.ini once, writes the JSON sidecar and regenerates
 * plugins.ini atomically (tmp + rename), then fixes permissions.
 * Returns 0 on success, -1 on failure. */
int storage_save_state(const pm_model_t* m);

#endif /* STORAGE_H */
