/*
 * Persistence: plugin listing, JSON sidecar, atomic plugins.ini regeneration.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "types.h"
#include "app.h"
#include "common.h"
#include "storage.h"

static int has_prx_extension(const char* name)
{
	size_t len = strlen(name);

	return (len > 4 && strcasecmp(name + len - 4, ".prx") == 0);
}

static int compare_paths(const void* a, const void* b)
{
	return strcasecmp(*(const char* const*)a, *(const char* const*)b);
}

int storage_list_prx(list_t* out)
{
	DIR* d;
	struct dirent* dir;
	char** paths = NULL;
	int count = 0, cap = 0;

	d = opendir(GOLDHEN_PLUGINS_DIR);
	if (!d)
	{
		LOG("Cannot open " GOLDHEN_PLUGINS_DIR);
		return 0;
	}

	while ((dir = readdir(d)) != NULL)
	{
		char* full;
		size_t len;

		if (!has_prx_extension(dir->d_name))
			continue;

		if (count == cap)
		{
			int ncap = cap ? cap * 2 : 16;
			char** grown = realloc(paths, ncap * sizeof(char*));
			if (!grown)
				break;
			paths = grown;
			cap = ncap;
		}

		len = strlen(GOLDHEN_PLUGINS_DIR) + strlen(dir->d_name) + 1;
		full = malloc(len);
		if (!full)
			break;

		snprintf(full, len, GOLDHEN_PLUGINS_DIR "%s", dir->d_name);
		paths[count++] = full;
	}
	closedir(d);

	if (count > 1)
		qsort(paths, count, sizeof(char*), compare_paths);

	for (int i = 0; i < count; i++)
		list_append(out, paths[i]);

	free(paths);
	LOG("Found %d plugins", count);
	return count;
}

void storage_free_prx(list_t* list)
{
	list_node_t* node;
	char* path;

	if (!list)
		return;

	for (node = list_head(list); (path = list_get(node)); node = list_next(node))
		free(path);

	list_free(list);
}

/* Reads a whole file as a NUL-terminated string. Returns NULL when missing. */
static char* read_text_file(const char* path)
{
	FILE* fp;
	long size;
	char* buf;

	fp = fopen(path, "rb");
	if (!fp)
		return NULL;

	if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0)
	{
		fclose(fp);
		return NULL;
	}
	rewind(fp);

	buf = malloc((size_t)size + 1);
	if (!buf)
	{
		fclose(fp);
		return NULL;
	}

	if (fread(buf, 1, (size_t)size, fp) != (size_t)size)
	{
		free(buf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);

	buf[size] = '\0';
	return buf;
}

/* Writes text to tmp_path and atomically renames it over final_path. */
static int write_text_atomic(const char* tmp_path, const char* final_path, const char* text)
{
	FILE* fp;
	size_t len = strlen(text);

	fp = fopen(tmp_path, "wb");
	if (!fp)
	{
		LOG("Cannot create %s", tmp_path);
		return -1;
	}

	if (fwrite(text, 1, len, fp) != len || fflush(fp) != 0 || fsync(fileno(fp)) != 0)
	{
		LOG("Short write or fsync failure on %s", tmp_path);
		fclose(fp);
		unlink(tmp_path);
		return -1;
	}

	if (fclose(fp) != 0)
	{
		LOG("Close failed on %s", tmp_path);
		unlink(tmp_path);
		return -1;
	}

	if (rename(tmp_path, final_path) != 0)
	{
		LOG("Rename %s -> %s failed", tmp_path, final_path);
		unlink(tmp_path);
		return -1;
	}

	return 0;
}

static int import_ini(pm_model_t* m)
{
	char* text = read_text_file(GOLDHEN_INI);
	int ret = 0;

	if (text)
	{
		LOG("Importing existing " GOLDHEN_INI);
		ret = pm_ini_parse(m, text);
		free(text);
	}
	else
	{
		LOG("No " GOLDHEN_INI " found, starting empty");
	}

	return ret;
}

/* Writes the JSON sidecar atomically and fixes its permissions. */
static int write_json_sidecar(const pm_model_t* m)
{
	char* json = pm_json_serialize(m);
	int ret;

	if (!json)
		return -1;

	ret = write_text_atomic(PM_STATE_JSON_TMP, PM_STATE_JSON, json);
	free(json);
	if (ret != 0)
		return -1;

	chmod(PM_STATE_JSON, 0777);
	return 0;
}

int storage_load_state(pm_model_t* m)
{
	char* json = read_text_file(PM_STATE_JSON);
	int ret;

	if (json)
	{
		ret = pm_json_parse(m, json);
		free(json);

		if (ret == 0)
			return pm_model_ensure_target(m, PM_TARGET_DEFAULT) ? 0 : -1;

		// Drop whatever was parsed before the error so it cannot leak into
		// the re-imported state (or get saved back over the good file).
		pm_model_clear(m);

		if (ret == -1)
		{
			LOG("Out of memory while reading " PM_STATE_JSON);
			return -1;
		}

		LOG(PM_STATE_JSON " is malformed, moving it aside");
		unlink(PM_STATE_JSON_BROKEN);
		rename(PM_STATE_JSON, PM_STATE_JSON_BROKEN);
		show_message("The saved plugin state was unreadable and was moved to:\n" PM_STATE_JSON_BROKEN "\n\nRe-importing " GOLDHEN_INI);
	}

	if (import_ini(m) != 0)
	{
		pm_model_clear(m);
		return -1;
	}

	if (!pm_model_ensure_target(m, PM_TARGET_DEFAULT))
		return -1;

	// First run: only the sidecar is written. plugins.ini stays untouched
	// (and gets backed up) until the user saves something.
	if (write_json_sidecar(m) != 0)
		LOG("Could not write " PM_STATE_JSON ", continuing with the in-memory state");

	return 0;
}

int storage_save_state(const pm_model_t* m)
{
	char* ini;
	int ret;

	// One-time backup of the original file
	if (file_exists(GOLDHEN_INI) == SUCCESS && file_exists(GOLDHEN_INI_BAK) != SUCCESS)
	{
		if (copy_file(GOLDHEN_INI, GOLDHEN_INI_BAK) != SUCCESS)
		{
			LOG("Backup to " GOLDHEN_INI_BAK " failed, aborting save");
			return -1;
		}
		LOG("Backed up " GOLDHEN_INI " to " GOLDHEN_INI_BAK);
	}

	if (write_json_sidecar(m) != 0)
		return -1;

	ini = pm_ini_serialize(m);
	if (!ini)
		return -1;

	ret = write_text_atomic(GOLDHEN_INI_TMP, GOLDHEN_INI, ini);
	free(ini);
	if (ret != 0)
		return -1;

	chmod(GOLDHEN_INI, 0777);
	set_perms_directory(GOLDHEN_PLUGINS_DIR, 0777);

	LOG("Saved " GOLDHEN_INI);
	return 0;
}
