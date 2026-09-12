/*
 * GoldHEN Plugin Manager - application-wide paths, constants and state.
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include "structs.h"
#include "ini_model.h"

#define APP_TITLE_ID                "PLGM00001"
#define APP_VERSION                 "01.00"

#define APP_ASSETS_PATH             "/mnt/sandbox/" APP_TITLE_ID "_000/app0/assets/"

/* Debug builds send LOG() output through dbglogger; override the destination
 * with make DEBUGLOG=1 DEBUGLOG_DEST=udp:<host-ip>:30000 */
#ifndef DEBUG_LOG_DEST
#define DEBUG_LOG_DEST              "udp:239.255.0.100:30000"
#endif

#define GOLDHEN_PATH                "/data/GoldHEN"
#define GOLDHEN_PLUGINS_DIR         GOLDHEN_PATH "/plugins/"
#define GOLDHEN_INI                 GOLDHEN_PATH "/plugins.ini"
#define GOLDHEN_INI_TMP             GOLDHEN_PATH "/plugins.ini.tmp"
#define GOLDHEN_INI_BAK             GOLDHEN_PATH "/plugins.ini.bak"
#define PM_STATE_JSON               GOLDHEN_PATH "/plugin_manager.json"
#define PM_STATE_JSON_TMP           GOLDHEN_PATH "/plugin_manager.json.tmp"
#define PM_STATE_JSON_BROKEN        GOLDHEN_PATH "/plugin_manager.json.broken"

#define APPDB_PATH                  "/system_data/priv/mms/app.db"
#define APPMETA_ICON_FMT            "/user/appmeta/%s/icon0.png"

/*
 * CATEGORY values (tbl_appinfo, key='CATEGORY') considered user games/apps.
 * UNVERIFIED HYPOTHESIS: these values were not confirmed against a real
 * app.db dump. Adjust on-console if system apps show up or games are missing.
 */
#define APPDB_USER_CATEGORIES       "'gd','gde'"

/* Button glyphs registered as special font characters (see main.c). */
#define CHAR_BTN_X                  0x10
#define CHAR_BTN_S                  0x11
#define CHAR_BTN_T                  0x12
#define CHAR_BTN_O                  0x13

#define show_message(...)	show_dialog(0, __VA_ARGS__)

int show_dialog(int dialog_type, const char * format, ...);
int init_loading_screen(const char* msg);
void stop_loading_screen(void);

/* One installed application from app.db. */
typedef struct app_entry
{
	char title_id[16];
	char* name;
} app_entry_t;

/* Application state owned by main.c. */
typedef struct pm_app
{
	pm_model_t* model;      /* plugin state, source of truth */
	list_t* installed;      /* app_entry_t*, sorted by name */
	list_t* plugins;        /* char* full path of every .prx in GOLDHEN_PLUGINS_DIR */
	char target_id[16];     /* "default" or a title id */
	char target_name[128];  /* "Global" or the game name */
	bool dirty;             /* unsaved toggles on the plugins screen */
	bool read_only;         /* state could not be loaded: toggling/saving disabled */
} pm_app_t;

extern pm_app_t app;

#endif /* APP_H */
