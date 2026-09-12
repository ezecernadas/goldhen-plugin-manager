/*
 * GoldHEN Plugin Manager - pure plugins.ini / plugin_manager.json model.
 *
 * This module depends on the C standard library only so it can be unit
 * tested on the host (`make test`) and compiled unchanged for the PS4.
 *
 * INI parsing notes
 * -----------------
 * - Lines are split on '\n'; a trailing '\r' is stripped (CRLF safe). A last
 *   line without a newline is processed.
 * - Leading/trailing spaces and tabs are trimmed on every line.
 * - Lines starting with '#' are comments and are NOT preserved.
 * - "[name]" starts a section. The name is trimmed. A header missing its
 *   closing bracket ("[name") is accepted leniently as the same section.
 *   A repeated section name merges into the existing target.
 * - Any other line is a plugin line: "path" or "path ; args". The text after
 *   the FIRST ';' is kept verbatim (trimmed) as the plugin arguments. A line
 *   whose path part is empty (";", "; something") is skipped, so ';' cannot
 *   be used to comment out a plugin; disabled plugins are simply omitted from
 *   the regenerated file and remembered in the JSON sidecar instead.
 * - Plugin lines before any header belong to the target with id "" and are
 *   serialized first, without a header.
 * - Within a target, a repeated path keeps the first entry; if that entry has
 *   no arguments and the repeat has some, the arguments are adopted.
 * - A leading UTF-8 byte order mark is ignored.
 * - Plugin paths are normalized by collapsing consecutive '/' characters, both
 *   when stored and when looked up. Case is preserved (case-sensitive FS).
 */
#ifndef INI_MODEL_H
#define INI_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *path;     /* plugin path as written in the ini, with "//" collapsed */
    char *args;     /* arguments after ';', or NULL when none (never "") */
    bool enabled;   /* emitted into plugins.ini when true */
} pm_plugin_t;

typedef struct {
    char *id;               /* "default" = Global; "" = lines before any header */
    pm_plugin_t *plugins;
    size_t count;
    size_t cap;
} pm_target_t;

typedef struct {
    pm_target_t *targets;
    size_t count;
    size_t cap;
} pm_model_t;

#define PM_TARGET_DEFAULT "default"
#define PM_JSON_VERSION   1

pm_model_t *pm_model_new(void);
void        pm_model_free(pm_model_t *m);
/* Frees every target and leaves m empty and reusable (NULL is ignored). */
void        pm_model_clear(pm_model_t *m);

/* Parses ini text (NULL treated as empty) and merges it into m.
 * Every plugin line found is enabled. Returns 0, or -1 on allocation failure. */
int         pm_ini_parse(pm_model_t *m, const char *text);

/* Returns a malloc'd plugins.ini text (caller frees), NULL on allocation
 * failure. Only enabled plugins are written; every target header is written. */
char       *pm_ini_serialize(const pm_model_t *m);

/* Returns a malloc'd JSON sidecar text with a trailing newline, NULL on
 * allocation failure. Schema:
 * {"version":1,"targets":[{"id":"...","plugins":[{"path":"...","args":"..."|null,"enabled":true|false}]}]} */
char       *pm_json_serialize(const pm_model_t *m);

/* Parses the JSON sidecar into an empty m. Returns 0, -1 on allocation
 * failure, -2 on malformed input (unknown keys, wrong version, trailing data). */
int         pm_json_parse(pm_model_t *m, const char *text);

pm_target_t *pm_model_find_target(const pm_model_t *m, const char *id);
pm_target_t *pm_model_ensure_target(pm_model_t *m, const char *id);
pm_plugin_t *pm_target_find_plugin(const pm_target_t *t, const char *path);

/* Creates the target and the plugin entry when missing (args NULL).
 * Never modifies existing arguments. Returns 0, or -1 on allocation failure. */
int         pm_model_set_enabled(pm_model_t *m, const char *target_id, const char *path, bool enabled);
bool        pm_model_is_enabled(const pm_model_t *m, const char *target_id, const char *path);

/* Arguments of a plugin in a target (the text after ';'), or NULL when the
 * target, the plugin or the arguments are absent. */
const char *pm_model_get_args(const pm_model_t *m, const char *target_id, const char *path);

/* Sets the arguments of a plugin in a target. NULL or "" clears them. Creates
 * the target and a DISABLED plugin entry when missing; never changes the
 * enabled flag. Returns 0, or -1 on allocation failure. */
int         pm_model_set_args(pm_model_t *m, const char *target_id, const char *path, const char *args);

/* Deep equality: same targets in the same order, same plugins in the same
 * order, same args and enabled flags. */
bool        pm_model_equal(const pm_model_t *a, const pm_model_t *b);

/* Human readable name: basename, ".prx" stripped (case-insensitive), '_'
 * replaced by ' ', first letter of each word upper-cased, rest unchanged.
 * Always NUL-terminates when out_size > 0; writes nothing when out_size == 0. */
void        pm_plugin_display_name(const char *path, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* INI_MODEL_H */
