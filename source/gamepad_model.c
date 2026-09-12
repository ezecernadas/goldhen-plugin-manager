/*
 * Pure model of GoldHEN's gamepad.ini. See gamepad_model.h.
 * libc only: no PS4 dependencies.
 */
#include <stdlib.h>
#include <string.h>

#include "gamepad_model.h"

/* ---------- small helpers ---------- */

static char *gp_strndup_(const char *s, size_t n)
{
	char *out = malloc(n + 1);

	if (!out)
		return NULL;
	memcpy(out, s, n);
	out[n] = '\0';
	return out;
}

static char *gp_strdup_(const char *s)
{
	return gp_strndup_(s, strlen(s));
}

static int is_blank_(char c)
{
	return c == ' ' || c == '\t';
}

/* Trims leading/trailing spaces and tabs in place (start pointer returned). */
static char *trim_(char *s)
{
	size_t len;

	while (is_blank_(*s))
		s++;
	len = strlen(s);
	while (len > 0 && is_blank_(s[len - 1]))
		s[--len] = '\0';
	return s;
}

static int section_reserve_(gp_section_t *sec, size_t want)
{
	gp_kv_t *grown;
	size_t ncap;

	if (want <= sec->cap)
		return 0;
	ncap = sec->cap ? sec->cap * 2 : 8;
	while (ncap < want)
		ncap *= 2;
	grown = realloc(sec->items, ncap * sizeof(*grown));
	if (!grown)
		return -1;
	sec->items = grown;
	sec->cap = ncap;
	return 0;
}

static int model_reserve_(gp_model_t *m, size_t want)
{
	gp_section_t *grown;
	size_t ncap;

	if (want <= m->cap)
		return 0;
	ncap = m->cap ? m->cap * 2 : 4;
	while (ncap < want)
		ncap *= 2;
	grown = realloc(m->sections, ncap * sizeof(*grown));
	if (!grown)
		return -1;
	m->sections = grown;
	m->cap = ncap;
	return 0;
}

static void section_free_items_(gp_section_t *sec)
{
	for (size_t i = 0; i < sec->count; i++)
	{
		free(sec->items[i].key);
		free(sec->items[i].value);
	}
	free(sec->items);
	sec->items = NULL;
	sec->count = 0;
	sec->cap = 0;
}

static gp_kv_t *section_find_(const gp_section_t *sec, const char *key)
{
	for (size_t i = 0; i < sec->count; i++)
		if (strcmp(sec->items[i].key, key) == 0)
			return &sec->items[i];
	return NULL;
}

/* Sets or appends key=value inside a section. */
static int section_set_(gp_section_t *sec, const char *key, const char *value)
{
	gp_kv_t *kv = section_find_(sec, key);
	char *nv = gp_strdup_(value);

	if (!nv)
		return -1;

	if (kv)
	{
		free(kv->value);
		kv->value = nv;
		return 0;
	}

	if (section_reserve_(sec, sec->count + 1) != 0)
	{
		free(nv);
		return -1;
	}
	kv = &sec->items[sec->count];
	kv->key = gp_strdup_(key);
	if (!kv->key)
	{
		free(nv);
		return -1;
	}
	kv->value = nv;
	sec->count++;
	return 0;
}

/* ---------- lifecycle ---------- */

gp_model_t *gp_model_new(void)
{
	gp_model_t *m = calloc(1, sizeof(*m));

	return m;
}

void gp_model_clear(gp_model_t *m)
{
	if (!m)
		return;

	for (size_t i = 0; i < m->count; i++)
	{
		section_free_items_(&m->sections[i]);
		free(m->sections[i].id);
	}
	free(m->sections);
	m->sections = NULL;
	m->count = 0;
	m->cap = 0;
}

void gp_model_free(gp_model_t *m)
{
	if (!m)
		return;
	gp_model_clear(m);
	free(m);
}

/* ---------- sections ---------- */

gp_section_t *gp_model_find_section(const gp_model_t *m, const char *id)
{
	if (!m || !id)
		return NULL;

	for (size_t i = 0; i < m->count; i++)
		if (strcmp(m->sections[i].id, id) == 0)
			return &m->sections[i];
	return NULL;
}

gp_section_t *gp_model_ensure_section(gp_model_t *m, const char *id)
{
	gp_section_t *sec;

	if (!m || !id)
		return NULL;

	sec = gp_model_find_section(m, id);
	if (sec)
		return sec;

	if (model_reserve_(m, m->count + 1) != 0)
		return NULL;

	sec = &m->sections[m->count];
	memset(sec, 0, sizeof(*sec));
	sec->id = gp_strdup_(id);
	if (!sec->id)
		return NULL;
	m->count++;
	return sec;
}

/* ---------- parsing ---------- */

static int parse_line_(gp_model_t *m, char *line, gp_section_t **current)
{
	char *eq;
	char *key;
	char *value;

	line = trim_(line);

	if (line[0] == '\0' || line[0] == ';' || line[0] == '#')
		return 0;

	if (line[0] == '[')
	{
		char *end = strchr(line, ']');
		char *id;

		if (end)
			*end = '\0';
		id = trim_(line + 1);
		*current = gp_model_ensure_section(m, id);
		return *current ? 0 : -1;
	}

	if (!*current)
	{
		*current = gp_model_ensure_section(m, "");
		if (!*current)
			return -1;
	}

	eq = strchr(line, '=');
	if (eq)
	{
		*eq = '\0';
		key = trim_(line);
		value = trim_(eq + 1);
	}
	else
	{
		key = line;
		value = line + strlen(line);   /* "" */
	}

	if (key[0] == '\0')
		return 0;   /* "=value" carries nothing usable */

	return section_set_(*current, key, value);
}

int gp_ini_parse(gp_model_t *m, const char *text)
{
	gp_section_t *current = NULL;
	const char *p;
	int rc = 0;

	if (!m)
		return -1;
	if (!text)
		return 0;

	/* UTF-8 byte order mark */
	if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
		text += 3;

	p = text;
	while (*p && rc == 0)
	{
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		char *line;

		if (len > 0 && p[len - 1] == '\r')
			len--;

		line = gp_strndup_(p, len);
		if (!line)
			return -1;
		rc = parse_line_(m, line, &current);
		free(line);

		if (!nl)
			break;
		p = nl + 1;
	}

	return rc;
}

/* ---------- serialization ---------- */

typedef struct
{
	char *buf;
	size_t len;
	size_t cap;
	int failed;
} sb_t;

static void sb_put_(sb_t *sb, const char *s)
{
	size_t n = strlen(s);

	if (sb->failed)
		return;
	if (sb->len + n + 1 > sb->cap)
	{
		size_t ncap = sb->cap ? sb->cap * 2 : 256;
		char *grown;

		while (ncap < sb->len + n + 1)
			ncap *= 2;
		grown = realloc(sb->buf, ncap);
		if (!grown)
		{
			sb->failed = 1;
			return;
		}
		sb->buf = grown;
		sb->cap = ncap;
	}
	memcpy(sb->buf + sb->len, s, n);
	sb->len += n;
	sb->buf[sb->len] = '\0';
}

static void serialize_items_(sb_t *sb, const gp_section_t *sec)
{
	for (size_t i = 0; i < sec->count; i++)
	{
		sb_put_(sb, sec->items[i].key);
		sb_put_(sb, "=");
		sb_put_(sb, sec->items[i].value);
		sb_put_(sb, "\n");
	}
	sb_put_(sb, "\n");
}

char *gp_ini_serialize(const gp_model_t *m)
{
	sb_t sb = { NULL, 0, 0, 0 };

	if (!m)
		return NULL;

	sb_put_(&sb, "");   /* guarantees a non-NULL "" for an empty model */

	/* header-less lines first, only when there is something to emit */
	for (size_t i = 0; i < m->count; i++)
		if (m->sections[i].id[0] == '\0' && m->sections[i].count > 0)
			serialize_items_(&sb, &m->sections[i]);

	for (size_t i = 0; i < m->count; i++)
	{
		const gp_section_t *sec = &m->sections[i];

		if (sec->id[0] == '\0')
			continue;
		sb_put_(&sb, "[");
		sb_put_(&sb, sec->id);
		sb_put_(&sb, "]\n");
		serialize_items_(&sb, sec);
	}

	if (sb.failed)
	{
		free(sb.buf);
		return NULL;
	}
	return sb.buf;
}

/* ---------- access ---------- */

const char *gp_get(const gp_model_t *m, const char *section, const char *key)
{
	const gp_section_t *sec = gp_model_find_section(m, section);
	const gp_kv_t *kv;

	if (!sec || !key)
		return NULL;
	kv = section_find_(sec, key);
	return kv ? kv->value : NULL;
}

int gp_set(gp_model_t *m, const char *section, const char *key, const char *value)
{
	gp_section_t *sec;

	if (!m || !section || !key || key[0] == '\0')
		return -1;

	if (!value)
	{
		gp_kv_t *kv;
		size_t idx;

		sec = gp_model_find_section(m, section);
		if (!sec)
			return 0;
		kv = section_find_(sec, key);
		if (!kv)
			return 0;

		idx = (size_t)(kv - sec->items);
		free(kv->key);
		free(kv->value);
		memmove(&sec->items[idx], &sec->items[idx + 1], (sec->count - idx - 1) * sizeof(*sec->items));
		sec->count--;

		/* Drop the section once its last key is gone: an empty "[id]" carries
		 * no configuration and would only add noise to the regenerated file. */
		if (sec->count == 0)
		{
			size_t si = (size_t)(sec - m->sections);

			section_free_items_(sec);
			free(sec->id);
			memmove(&m->sections[si], &m->sections[si + 1], (m->count - si - 1) * sizeof(*m->sections));
			m->count--;
		}
		return 0;
	}

	sec = gp_model_ensure_section(m, section);
	if (!sec)
		return -1;
	return section_set_(sec, key, value);
}

bool gp_model_equal(const gp_model_t *a, const gp_model_t *b)
{
	if (!a || !b)
		return a == b;
	if (a->count != b->count)
		return false;

	for (size_t i = 0; i < a->count; i++)
	{
		const gp_section_t *sa = &a->sections[i];
		const gp_section_t *sb = &b->sections[i];

		if (strcmp(sa->id, sb->id) != 0 || sa->count != sb->count)
			return false;
		for (size_t j = 0; j < sa->count; j++)
			if (strcmp(sa->items[j].key, sb->items[j].key) != 0 ||
				strcmp(sa->items[j].value, sb->items[j].value) != 0)
				return false;
	}
	return true;
}
