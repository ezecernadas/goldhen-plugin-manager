/*
 * GoldHEN Plugin Manager - pure plugins.ini / plugin_manager.json model.
 * Standard C99 only. See include/ini_model.h for the format rules.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ini_model.h"

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static char *pm_strndup_(const char *s, size_t n)
{
    char *d = malloc(n + 1);
    if (!d)
        return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static char *pm_strdup_(const char *s)
{
    return pm_strndup_(s, strlen(s));
}

/* Copies s[0..n) collapsing consecutive '/' into one. Case is preserved:
 * the PS4 filesystem is case-sensitive. */
static char *normalize_path_(const char *s, size_t n)
{
    char *out = malloc(n + 1);
    size_t i, o = 0;
    if (!out)
        return NULL;
    for (i = 0; i < n; i++) {
        if (s[i] == '/' && o > 0 && out[o - 1] == '/')
            continue;
        out[o++] = s[i];
    }
    out[o] = '\0';
    return out;
}

static bool is_blank_(char c)
{
    return c == ' ' || c == '\t';
}

/* Trims [begin, end) in place by moving the bounds; returns new begin, sets *end. */
static const char *trim_(const char *begin, const char **end)
{
    const char *e = *end;
    while (begin < e && is_blank_(*begin))
        begin++;
    while (e > begin && is_blank_(e[-1]))
        e--;
    *end = e;
    return begin;
}

/* Growable string buffer. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    bool failed;
} sb_t;

static void sb_init_(sb_t *sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    sb->failed = false;
}

static void sb_reserve_(sb_t *sb, size_t extra)
{
    size_t need;
    char *nd;

    if (sb->failed)
        return;
    need = sb->len + extra + 1;
    if (need <= sb->cap)
        return;
    while (sb->cap < need)
        sb->cap = sb->cap ? sb->cap * 2 : 256;
    nd = realloc(sb->data, sb->cap);
    if (!nd) {
        sb->failed = true;
        return;
    }
    sb->data = nd;
}

static void sb_putn_(sb_t *sb, const char *s, size_t n)
{
    sb_reserve_(sb, n);
    if (sb->failed)
        return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_puts_(sb_t *sb, const char *s)
{
    sb_putn_(sb, s, strlen(s));
}

static void sb_putc_(sb_t *sb, char c)
{
    sb_putn_(sb, &c, 1);
}

/* Returns the buffer as a malloc'd string ("" when nothing was written). */
static char *sb_finish_(sb_t *sb)
{
    if (sb->failed) {
        free(sb->data);
        return NULL;
    }
    if (!sb->data)
        return pm_strdup_("");
    return sb->data;
}

/* ------------------------------------------------------------------ */
/* Model                                                               */
/* ------------------------------------------------------------------ */

pm_model_t *pm_model_new(void)
{
    pm_model_t *m = calloc(1, sizeof(*m));
    return m;
}

static void target_free_(pm_target_t *t)
{
    size_t i;
    for (i = 0; i < t->count; i++) {
        free(t->plugins[i].path);
        free(t->plugins[i].args);
    }
    free(t->plugins);
    free(t->id);
}

void pm_model_clear(pm_model_t *m)
{
    size_t i;
    if (!m)
        return;
    for (i = 0; i < m->count; i++)
        target_free_(&m->targets[i]);
    free(m->targets);
    m->targets = NULL;
    m->count = 0;
    m->cap = 0;
}

void pm_model_free(pm_model_t *m)
{
    if (!m)
        return;
    pm_model_clear(m);
    free(m);
}

pm_target_t *pm_model_find_target(const pm_model_t *m, const char *id)
{
    size_t i;
    if (!m || !id)
        return NULL;
    for (i = 0; i < m->count; i++)
        if (strcmp(m->targets[i].id, id) == 0)
            return &m->targets[i];
    return NULL;
}

pm_target_t *pm_model_ensure_target(pm_model_t *m, const char *id)
{
    pm_target_t *t;
    char *copy;

    if (!m || !id)
        return NULL;
    t = pm_model_find_target(m, id);
    if (t)
        return t;

    if (m->count == m->cap) {
        size_t ncap = m->cap ? m->cap * 2 : 8;
        pm_target_t *nt = realloc(m->targets, ncap * sizeof(*nt));
        if (!nt)
            return NULL;
        m->targets = nt;
        m->cap = ncap;
    }
    copy = pm_strdup_(id);
    if (!copy)
        return NULL;

    t = &m->targets[m->count++];
    memset(t, 0, sizeof(*t));
    t->id = copy;
    return t;
}

pm_plugin_t *pm_target_find_plugin(const pm_target_t *t, const char *path)
{
    size_t i;
    char *norm;
    pm_plugin_t *found = NULL;

    if (!t || !path)
        return NULL;
    norm = normalize_path_(path, strlen(path));
    if (!norm) {
        /* Fall back to an exact comparison rather than reporting "absent". */
        for (i = 0; i < t->count; i++)
            if (strcmp(t->plugins[i].path, path) == 0)
                return &t->plugins[i];
        return NULL;
    }
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->plugins[i].path, norm) == 0) {
            found = &t->plugins[i];
            break;
        }
    }
    free(norm);
    return found;
}

/* Appends a plugin with an owned copy of path and args (args may be NULL). */
static pm_plugin_t *target_add_plugin_(pm_target_t *t, const char *path, size_t path_len,
                                       const char *args, size_t args_len)
{
    pm_plugin_t *p;
    char *pcopy;
    char *acopy = NULL;

    if (t->count == t->cap) {
        size_t ncap = t->cap ? t->cap * 2 : 8;
        pm_plugin_t *np = realloc(t->plugins, ncap * sizeof(*np));
        if (!np)
            return NULL;
        t->plugins = np;
        t->cap = ncap;
    }
    pcopy = normalize_path_(path, path_len);
    if (!pcopy)
        return NULL;
    if (args && args_len > 0) {
        acopy = pm_strndup_(args, args_len);
        if (!acopy) {
            free(pcopy);
            return NULL;
        }
    }
    p = &t->plugins[t->count++];
    p->path = pcopy;
    p->args = acopy;
    p->enabled = false;
    return p;
}

int pm_model_set_enabled(pm_model_t *m, const char *target_id, const char *path, bool enabled)
{
    pm_target_t *t;
    pm_plugin_t *p;

    if (!m || !target_id || !path)
        return -1;
    t = pm_model_ensure_target(m, target_id);
    if (!t)
        return -1;
    p = pm_target_find_plugin(t, path);
    if (!p) {
        p = target_add_plugin_(t, path, strlen(path), NULL, 0);
        if (!p)
            return -1;
    }
    p->enabled = enabled;
    return 0;
}

bool pm_model_is_enabled(const pm_model_t *m, const char *target_id, const char *path)
{
    const pm_plugin_t *p = pm_target_find_plugin(pm_model_find_target(m, target_id), path);
    return p ? p->enabled : false;
}

static bool str_eq_nullable_(const char *a, const char *b)
{
    if (!a || !b)
        return a == b;
    return strcmp(a, b) == 0;
}

bool pm_model_equal(const pm_model_t *a, const pm_model_t *b)
{
    size_t i, j;

    if (!a || !b)
        return a == b;
    if (a->count != b->count)
        return false;
    for (i = 0; i < a->count; i++) {
        const pm_target_t *ta = &a->targets[i];
        const pm_target_t *tb = &b->targets[i];
        if (strcmp(ta->id, tb->id) != 0 || ta->count != tb->count)
            return false;
        for (j = 0; j < ta->count; j++) {
            const pm_plugin_t *pa = &ta->plugins[j];
            const pm_plugin_t *pb = &tb->plugins[j];
            if (strcmp(pa->path, pb->path) != 0 || pa->enabled != pb->enabled
                || !str_eq_nullable_(pa->args, pb->args))
                return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* INI                                                                 */
/* ------------------------------------------------------------------ */

/* Handles one trimmed, non-empty line. Returns 0, or -1 on allocation failure. */
static int ini_handle_line_(pm_model_t *m, pm_target_t **cur, const char *b, const char *e)
{
    pm_target_t *t;
    pm_plugin_t *p;
    const char *semi;
    const char *path_b, *path_e;
    const char *args_b = NULL, *args_e = NULL;
    char *id;

    if (*b == '#')
        return 0;

    if (*b == '[') {
        const char *nb = b + 1;
        const char *ne = e;
        if (ne > nb && ne[-1] == ']')
            ne--;
        nb = trim_(nb, &ne);
        id = pm_strndup_(nb, (size_t)(ne - nb));
        if (!id)
            return -1;
        t = pm_model_ensure_target(m, id);
        free(id);
        if (!t)
            return -1;
        *cur = t;
        return 0;
    }

    /* Plugin line: split at the first ';'. */
    semi = memchr(b, ';', (size_t)(e - b));
    path_b = b;
    path_e = semi ? semi : e;
    path_b = trim_(path_b, &path_e);
    if (path_b == path_e)
        return 0; /* empty path: skipped (see header) */

    if (semi) {
        args_b = semi + 1;
        args_e = e;
        args_b = trim_(args_b, &args_e);
        if (args_b == args_e)
            args_b = args_e = NULL;
    }

    if (!*cur) {
        *cur = pm_model_ensure_target(m, "");
        if (!*cur)
            return -1;
    }
    t = *cur;

    {
        char *tmp = pm_strndup_(path_b, (size_t)(path_e - path_b));
        if (!tmp)
            return -1;
        p = pm_target_find_plugin(t, tmp);
        free(tmp);
    }

    if (p) {
        p->enabled = true;
        if (!p->args && args_b) {
            p->args = pm_strndup_(args_b, (size_t)(args_e - args_b));
            if (!p->args)
                return -1;
        }
        return 0;
    }

    p = target_add_plugin_(t, path_b, (size_t)(path_e - path_b),
                           args_b, args_b ? (size_t)(args_e - args_b) : 0);
    if (!p)
        return -1;
    p->enabled = true;
    return 0;
}

int pm_ini_parse(pm_model_t *m, const char *text)
{
    pm_target_t *cur = NULL;
    const char *line;

    if (!m)
        return -1;
    if (!text)
        return 0;

    /* Skip a UTF-8 byte order mark left by Windows editors. */
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        text += 3;

    line = text;
    for (;;) {
        const char *nl = strchr(line, '\n');
        const char *e = nl ? nl : line + strlen(line);
        const char *b = line;

        if (e > b && e[-1] == '\r')
            e--;
        b = trim_(b, &e);
        if (b < e && ini_handle_line_(m, &cur, b, e) != 0)
            return -1;

        if (!nl)
            break;
        line = nl + 1;
    }
    return 0;
}

static void ini_emit_target_(sb_t *sb, const pm_target_t *t)
{
    size_t i;

    if (t->id[0] != '\0') {
        sb_putc_(sb, '[');
        sb_puts_(sb, t->id);
        sb_puts_(sb, "]\n");
    }
    for (i = 0; i < t->count; i++) {
        const pm_plugin_t *p = &t->plugins[i];
        if (!p->enabled)
            continue;
        sb_puts_(sb, p->path);
        if (p->args) {
            sb_puts_(sb, " ; ");
            sb_puts_(sb, p->args);
        }
        sb_putc_(sb, '\n');
    }
    sb_putc_(sb, '\n');
}

char *pm_ini_serialize(const pm_model_t *m)
{
    sb_t sb;
    size_t i;

    sb_init_(&sb);
    if (!m)
        return sb_finish_(&sb);

    /* Headerless target (id "") first, when it has anything enabled. */
    for (i = 0; i < m->count; i++) {
        const pm_target_t *t = &m->targets[i];
        if (t->id[0] == '\0') {
            size_t j;
            bool any = false;
            for (j = 0; j < t->count && !any; j++)
                any = t->plugins[j].enabled;
            if (any)
                ini_emit_target_(&sb, t);
        }
    }
    for (i = 0; i < m->count; i++) {
        const pm_target_t *t = &m->targets[i];
        if (t->id[0] != '\0')
            ini_emit_target_(&sb, t);
    }
    return sb_finish_(&sb);
}

/* ------------------------------------------------------------------ */
/* JSON writer                                                         */
/* ------------------------------------------------------------------ */

static void json_put_string_(sb_t *sb, const char *s)
{
    sb_putc_(sb, '"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  sb_puts_(sb, "\\\""); break;
        case '\\': sb_puts_(sb, "\\\\"); break;
        case '\n': sb_puts_(sb, "\\n"); break;
        case '\r': sb_puts_(sb, "\\r"); break;
        case '\t': sb_puts_(sb, "\\t"); break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                sb_puts_(sb, buf);
            } else {
                sb_putc_(sb, (char)c);
            }
        }
    }
    sb_putc_(sb, '"');
}

char *pm_json_serialize(const pm_model_t *m)
{
    sb_t sb;
    size_t i, j;

    sb_init_(&sb);
    sb_puts_(&sb, "{\"version\":1,\"targets\":[");
    for (i = 0; m && i < m->count; i++) {
        const pm_target_t *t = &m->targets[i];
        if (i)
            sb_putc_(&sb, ',');
        sb_puts_(&sb, "{\"id\":");
        json_put_string_(&sb, t->id);
        sb_puts_(&sb, ",\"plugins\":[");
        for (j = 0; j < t->count; j++) {
            const pm_plugin_t *p = &t->plugins[j];
            if (j)
                sb_putc_(&sb, ',');
            sb_puts_(&sb, "{\"path\":");
            json_put_string_(&sb, p->path);
            sb_puts_(&sb, ",\"args\":");
            if (p->args)
                json_put_string_(&sb, p->args);
            else
                sb_puts_(&sb, "null");
            sb_puts_(&sb, ",\"enabled\":");
            sb_puts_(&sb, p->enabled ? "true" : "false");
            sb_putc_(&sb, '}');
        }
        sb_puts_(&sb, "]}");
    }
    sb_puts_(&sb, "]}\n");
    return sb_finish_(&sb);
}

/* ------------------------------------------------------------------ */
/* JSON reader (recursive descent for exactly the sidecar schema)     */
/* ------------------------------------------------------------------ */

#define JP_OK       0
#define JP_ALLOC   -1
#define JP_BAD     -2

typedef struct {
    const char *p;
} jp_t;

static void jp_ws_(jp_t *jp)
{
    while (*jp->p == ' ' || *jp->p == '\t' || *jp->p == '\n' || *jp->p == '\r')
        jp->p++;
}

static bool jp_accept_(jp_t *jp, char c)
{
    jp_ws_(jp);
    if (*jp->p != c)
        return false;
    jp->p++;
    return true;
}

static bool jp_literal_(jp_t *jp, const char *lit)
{
    size_t n = strlen(lit);
    jp_ws_(jp);
    if (strncmp(jp->p, lit, n) != 0)
        return false;
    jp->p += n;
    return true;
}

static int hexval_(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void sb_put_utf8_(sb_t *sb, unsigned cp)
{
    if (cp < 0x80) {
        sb_putc_(sb, (char)cp);
    } else if (cp < 0x800) {
        sb_putc_(sb, (char)(0xC0 | (cp >> 6)));
        sb_putc_(sb, (char)(0x80 | (cp & 0x3F)));
    } else {
        sb_putc_(sb, (char)(0xE0 | (cp >> 12)));
        sb_putc_(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc_(sb, (char)(0x80 | (cp & 0x3F)));
    }
}

/* Parses a JSON string into *out (malloc'd). */
static int jp_string_(jp_t *jp, char **out)
{
    sb_t sb;

    if (!jp_accept_(jp, '"'))
        return JP_BAD;
    sb_init_(&sb);
    for (;;) {
        unsigned char c = (unsigned char)*jp->p;
        if (c == '\0') {
            free(sb.data);
            return JP_BAD;
        }
        jp->p++;
        if (c == '"')
            break;
        if (c < 0x20) {
            free(sb.data);
            return JP_BAD;
        }
        if (c != '\\') {
            sb_putc_(&sb, (char)c);
            continue;
        }
        c = (unsigned char)*jp->p++;
        switch (c) {
        case '"':  sb_putc_(&sb, '"'); break;
        case '\\': sb_putc_(&sb, '\\'); break;
        case '/':  sb_putc_(&sb, '/'); break;
        case 'b':  sb_putc_(&sb, '\b'); break;
        case 'f':  sb_putc_(&sb, '\f'); break;
        case 'n':  sb_putc_(&sb, '\n'); break;
        case 'r':  sb_putc_(&sb, '\r'); break;
        case 't':  sb_putc_(&sb, '\t'); break;
        case 'u': {
            unsigned cp = 0;
            int k;
            for (k = 0; k < 4; k++) {
                int h = hexval_(*jp->p);
                if (h < 0) {
                    free(sb.data);
                    return JP_BAD;
                }
                cp = (cp << 4) | (unsigned)h;
                jp->p++;
            }
            sb_put_utf8_(&sb, cp);
            break;
        }
        default:
            free(sb.data);
            return JP_BAD;
        }
    }
    *out = sb_finish_(&sb);
    return *out ? JP_OK : JP_ALLOC;
}

static int jp_key_(jp_t *jp, char **key)
{
    int rc = jp_string_(jp, key);
    if (rc != JP_OK)
        return rc;
    if (!jp_accept_(jp, ':')) {
        free(*key);
        *key = NULL;
        return JP_BAD;
    }
    return JP_OK;
}

static int jp_plugin_(jp_t *jp, pm_target_t *t)
{
    char *path = NULL, *args = NULL;
    bool enabled = false;
    bool have_path = false, have_args = false, have_enabled = false;
    int rc = JP_BAD;

    if (!jp_accept_(jp, '{'))
        return JP_BAD;
    if (!jp_accept_(jp, '}')) {
        for (;;) {
            char *key;
            rc = jp_key_(jp, &key);
            if (rc != JP_OK)
                goto out;
            if (strcmp(key, "path") == 0 && !have_path) {
                rc = jp_string_(jp, &path);
                have_path = true;
            } else if (strcmp(key, "args") == 0 && !have_args) {
                if (jp_literal_(jp, "null"))
                    rc = JP_OK;
                else
                    rc = jp_string_(jp, &args);
                have_args = true;
            } else if (strcmp(key, "enabled") == 0 && !have_enabled) {
                if (jp_literal_(jp, "true")) { enabled = true; rc = JP_OK; }
                else if (jp_literal_(jp, "false")) { enabled = false; rc = JP_OK; }
                else rc = JP_BAD;
                have_enabled = true;
            } else {
                rc = JP_BAD;
            }
            free(key);
            if (rc != JP_OK)
                goto out;
            if (jp_accept_(jp, ','))
                continue;
            if (jp_accept_(jp, '}'))
                break;
            rc = JP_BAD;
            goto out;
        }
    }
    if (!have_path || !have_args || !have_enabled) {
        rc = JP_BAD;
        goto out;
    }
    if (args && args[0] == '\0') {
        free(args);
        args = NULL;
    }
    {
        pm_plugin_t *p = target_add_plugin_(t, path, strlen(path), args, args ? strlen(args) : 0);
        if (!p) {
            rc = JP_ALLOC;
            goto out;
        }
        p->enabled = enabled;
    }
    rc = JP_OK;
out:
    free(path);
    free(args);
    return rc;
}

static int jp_target_(jp_t *jp, pm_model_t *m)
{
    char *id = NULL;
    pm_target_t *t = NULL;
    bool have_id = false, have_plugins = false;
    int rc = JP_BAD;
    /* Plugins may appear before "id"; stage them in a temporary target. */
    pm_target_t staging;

    memset(&staging, 0, sizeof(staging));

    if (!jp_accept_(jp, '{'))
        return JP_BAD;
    if (!jp_accept_(jp, '}')) {
        for (;;) {
            char *key;
            rc = jp_key_(jp, &key);
            if (rc != JP_OK)
                goto out;
            if (strcmp(key, "id") == 0 && !have_id) {
                rc = jp_string_(jp, &id);
                have_id = true;
            } else if (strcmp(key, "plugins") == 0 && !have_plugins) {
                have_plugins = true;
                if (!jp_accept_(jp, '[')) {
                    rc = JP_BAD;
                } else if (jp_accept_(jp, ']')) {
                    rc = JP_OK;
                } else {
                    for (;;) {
                        rc = jp_plugin_(jp, &staging);
                        if (rc != JP_OK)
                            break;
                        if (jp_accept_(jp, ','))
                            continue;
                        if (jp_accept_(jp, ']'))
                            break;
                        rc = JP_BAD;
                        break;
                    }
                }
            } else {
                rc = JP_BAD;
            }
            free(key);
            if (rc != JP_OK)
                goto out;
            if (jp_accept_(jp, ','))
                continue;
            if (jp_accept_(jp, '}'))
                break;
            rc = JP_BAD;
            goto out;
        }
    }
    if (!have_id || !have_plugins) {
        rc = JP_BAD;
        goto out;
    }
    if (pm_model_find_target(m, id)) {
        rc = JP_BAD; /* duplicate target id */
        goto out;
    }
    t = pm_model_ensure_target(m, id);
    if (!t) {
        rc = JP_ALLOC;
        goto out;
    }
    /* Move staged plugins into the real target. */
    t->plugins = staging.plugins;
    t->count = staging.count;
    t->cap = staging.cap;
    memset(&staging, 0, sizeof(staging));
    rc = JP_OK;
out:
    target_free_(&staging);
    free(id);
    return rc;
}

int pm_json_parse(pm_model_t *m, const char *text)
{
    jp_t jp;
    bool have_version = false, have_targets = false;
    int rc;

    if (!m)
        return JP_ALLOC;
    if (!text)
        return JP_BAD;
    jp.p = text;

    if (!jp_accept_(&jp, '{'))
        return JP_BAD;
    if (!jp_accept_(&jp, '}')) {
        for (;;) {
            char *key;
            rc = jp_key_(&jp, &key);
            if (rc != JP_OK)
                return rc;
            if (strcmp(key, "version") == 0 && !have_version) {
                have_version = true;
                rc = jp_literal_(&jp, "1") ? JP_OK : JP_BAD;
                if (rc == JP_OK && isdigit((unsigned char)*jp.p))
                    rc = JP_BAD; /* e.g. "10" */
            } else if (strcmp(key, "targets") == 0 && !have_targets) {
                have_targets = true;
                if (!jp_accept_(&jp, '[')) {
                    rc = JP_BAD;
                } else if (jp_accept_(&jp, ']')) {
                    rc = JP_OK;
                } else {
                    for (;;) {
                        rc = jp_target_(&jp, m);
                        if (rc != JP_OK)
                            break;
                        if (jp_accept_(&jp, ','))
                            continue;
                        if (jp_accept_(&jp, ']'))
                            break;
                        rc = JP_BAD;
                        break;
                    }
                }
            } else {
                rc = JP_BAD;
            }
            free(key);
            if (rc != JP_OK)
                return rc;
            if (jp_accept_(&jp, ','))
                continue;
            if (jp_accept_(&jp, '}'))
                break;
            return JP_BAD;
        }
    }
    if (!have_version || !have_targets)
        return JP_BAD;
    jp_ws_(&jp);
    if (*jp.p != '\0')
        return JP_BAD;
    return JP_OK;
}

/* ------------------------------------------------------------------ */
/* Display name                                                        */
/* ------------------------------------------------------------------ */

void pm_plugin_display_name(const char *path, char *out, size_t out_size)
{
    const char *base;
    const char *slash;
    size_t len;
    size_t i, o = 0;
    bool word_start = true;

    if (out_size == 0)
        return;
    out[0] = '\0';
    if (!path)
        return;

    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    len = strlen(base);
    if (len >= 4) {
        const char *ext = base + len - 4;
        if (ext[0] == '.' && tolower((unsigned char)ext[1]) == 'p'
            && tolower((unsigned char)ext[2]) == 'r' && tolower((unsigned char)ext[3]) == 'x')
            len -= 4;
    }

    for (i = 0; i < len && o + 1 < out_size; i++) {
        unsigned char c = (unsigned char)base[i];
        if (c == '_')
            c = ' ';
        if (c == ' ') {
            word_start = true;
        } else if (word_start) {
            c = (unsigned char)toupper(c);
            word_start = false;
        }
        out[o++] = (char)c;
    }
    out[o] = '\0';
}
