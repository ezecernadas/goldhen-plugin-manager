/*
 * Pure model of GoldHEN's gamepad.ini (the Gamepad Helper plugin config).
 *
 * The file is a list of `[section]` headers followed by `key=value` lines.
 * Sections are title ids ("default" applies to every title). This module
 * depends on libc only so it can be unit-tested on the host; unknown keys
 * are preserved verbatim so the file can be regenerated without data loss.
 * Comment lines (';' or '#') are dropped.
 */
#ifndef GAMEPAD_MODEL_H
#define GAMEPAD_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	char *key;
	char *value;    /* never NULL; "" for a line without '=' */
} gp_kv_t;

typedef struct
{
	char *id;       /* "default", a title id, or "" for lines before any header */
	gp_kv_t *items;
	size_t count;
	size_t cap;
} gp_section_t;

typedef struct
{
	gp_section_t *sections;
	size_t count;
	size_t cap;
} gp_model_t;

gp_model_t   *gp_model_new(void);
void          gp_model_free(gp_model_t *m);
void          gp_model_clear(gp_model_t *m);

/* Parses ini text into m (merging into existing sections). NULL text is
 * treated as empty. Returns 0 on success, -1 on allocation failure. */
int           gp_ini_parse(gp_model_t *m, const char *text);

/* Returns a malloc'd string (caller frees), NULL on allocation failure. */
char         *gp_ini_serialize(const gp_model_t *m);

gp_section_t *gp_model_find_section(const gp_model_t *m, const char *id);
gp_section_t *gp_model_ensure_section(gp_model_t *m, const char *id);

/* Value of key in section, or NULL when the section or key is absent. */
const char   *gp_get(const gp_model_t *m, const char *section, const char *key);

/* Sets key=value in section (creating the section when needed, updating an
 * existing key in place). A NULL value removes the key; removing an absent
 * key is a no-op. Returns 0 on success, -1 on allocation failure. */
int           gp_set(gp_model_t *m, const char *section, const char *key, const char *value);

/* Deep equality: same sections, keys and values in the same order. */
bool          gp_model_equal(const gp_model_t *a, const gp_model_t *b);

#ifdef __cplusplus
}
#endif

#endif /* GAMEPAD_MODEL_H */
