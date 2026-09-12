/*
 * Installed application enumeration from app.db (SQLite, read-only).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "types.h"
#include "app.h"
#include "app_db.h"



static sqlite3* open_sqlite_db(const char* db_path)
{
	sqlite3 *db;
	static int init = -1;

	if (init > 0 || (init < 0 && (init = sqlite3_os_init()) != SQLITE_OK))
	{
		LOG("Error loading extension: %s", "orbis_rw");
		return NULL;
	}

	LOG("Opening '%s'...", db_path);
	if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, "orbis_rw") != SQLITE_OK)
	{
		LOG("Error opening db: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	LOG("app.db opened");
	return db;
}

list_t* appdb_load_installed(void)
{
	list_t* list = list_alloc();
	sqlite3* db;
	sqlite3_stmt* res = NULL;
	const char* query =
		"SELECT DISTINCT A.titleId, A.val FROM tbl_appinfo AS A"
		" INNER JOIN tbl_appinfo AS B ON A.titleId = B.titleId"
		" WHERE A.key = 'TITLE' AND B.key = 'CATEGORY' AND B.val IN (" APPDB_USER_CATEGORIES ")"
		" ORDER BY A.val COLLATE NOCASE";

	if (!list)
		return NULL;

	if ((db = open_sqlite_db(APPDB_PATH)) == NULL)
	{
		LOG("Failed to open " APPDB_PATH);
		return list;
	}

	if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK)
	{
		LOG("Query failed: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return list;
	}

	LOG("app.db query prepared");
	while (sqlite3_step(res) == SQLITE_ROW)
	{
		const char* title_id = (const char*) sqlite3_column_text(res, 0);
		const char* name = (const char*) sqlite3_column_text(res, 1);
		app_entry_t* entry;

		if (list_count(list) < 3)
			LOG("row: %s / %s", title_id ? title_id : "(null)", name ? name : "(null)");

		if (!title_id || !title_id[0] || strcmp(title_id, APP_TITLE_ID) == 0)
			continue;

		entry = calloc(1, sizeof(app_entry_t));
		if (!entry)
			break;

		strncpy(entry->title_id, title_id, sizeof(entry->title_id) - 1);
		entry->name = strdup((name && name[0]) ? name : title_id);
		list_append(list, entry);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	LOG("Found %zu installed apps", list_count(list));
	return list;
}

void appdb_free(list_t* list)
{
	list_node_t* node;
	app_entry_t* entry;

	if (!list)
		return;

	for (node = list_head(list); (entry = list_get(node)); node = list_next(node))
	{
		free(entry->name);
		free(entry);
	}

	list_free(list);
}
