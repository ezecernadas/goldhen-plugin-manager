/*
 * Host-side tests for the pure gamepad.ini model.
 * Build and run with `make test` (no PS4 toolchain required).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamepad_model.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond) do { \
    if (cond) { g_passed++; } \
    else { g_failed++; fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_STR_EQ(a, b) do { \
    const char *_a = (a); const char *_b = (b); \
    if (_a && _b && strcmp(_a, _b) == 0) { g_passed++; } \
    else { g_failed++; fprintf(stderr, "  FAIL %s:%d: expected \"%s\" got \"%s\"\n", \
        __FILE__, __LINE__, _b ? _b : "(null)", _a ? _a : "(null)"); } \
} while (0)

#define RUN(fn) do { printf("- %s\n", #fn); fn(); } while (0)

/* Verbatim copy of a gamepad.ini taken from a console. */
static const char *REAL_INI =
    "; Load gamepad config for any title\n"
    "\n"
    "[default]\n"
    "enableDeadZone=0\n"
    "DeadZoneLeft=13\n"
    "DeadZoneRight=13\n"
    "\n"
    "enableCustomTouchPad=0\n"
    "TOUCH_L2=BUTTON_L3\n"
    "TOUCH_R2=BUTTON_TOUCH_PAD\n"
    "\n"
    "; Load gamepad config for Playroom\n"
    "\n"
    "[CUSA00001]\n"
    "enableCustomButton=0\n"
    "BUTTON_L3=BUTTON_CROSS\n"
    "\n"
    "VirationIntensity=medium\n"
    "\n"
    ";\n"
    "\n"
    "[CUSA07408]\n"
    "enableCustomButton=1\n"
    "BUTTON_R3=BUTTON_TOUCH_PAD\n"
    "\n"
    "[CUSA03281]\n"
    "enableCustomButton=1\n"
    "BUTTON_R3=BUTTON_TOUCH_PAD\n"
    "\n"
    "[CUSA04943]\n"
    "enableCustomButton=1\n"
    "BUTTON_L3=BUTTON_TOUCH_PAD\n"
    "enableDeadZone=1\n"
    "DeadZoneLeft=25\n"
    "BUTTON_CROSS=BUTTON_TRIANGLE\n";

static void test_empty_and_null(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(m != NULL);
    CHECK(gp_ini_parse(m, NULL) == 0);
    CHECK(m->count == 0);
    CHECK(gp_ini_parse(m, "") == 0);
    CHECK(m->count == 0);

    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "");
    free(out);
    gp_model_free(m);
}

static void test_real_file(void)
{
    gp_model_t *m = gp_model_new();

    CHECK(gp_ini_parse(m, REAL_INI) == 0);
    CHECK(m->count == 5);
    if (m->count == 5)
    {
        CHECK_STR_EQ(m->sections[0].id, "default");
        CHECK_STR_EQ(m->sections[1].id, "CUSA00001");
        CHECK_STR_EQ(m->sections[2].id, "CUSA07408");
        CHECK_STR_EQ(m->sections[3].id, "CUSA03281");
        CHECK_STR_EQ(m->sections[4].id, "CUSA04943");
        CHECK(m->sections[0].count == 6);
        CHECK(m->sections[4].count == 5);
    }
    CHECK_STR_EQ(gp_get(m, "default", "TOUCH_R2"), "BUTTON_TOUCH_PAD");
    CHECK_STR_EQ(gp_get(m, "default", "enableDeadZone"), "0");
    CHECK(gp_get(m, "CUSA04943", "DeadZoneRight") == NULL);
    CHECK_STR_EQ(gp_get(m, "CUSA04943", "DeadZoneLeft"), "25");
    CHECK_STR_EQ(gp_get(m, "CUSA00001", "VirationIntensity"), "medium");
    CHECK(gp_get(m, "CUSA99999", "enableDeadZone") == NULL);
    CHECK(gp_get(m, "default", "nope") == NULL);
    CHECK(gp_model_find_section(m, "") == NULL);   /* comments never create the headerless section */
    gp_model_free(m);
}

static void test_set_get_remove(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_model_find_section(m, "CUSA00001") == NULL);
    CHECK(gp_set(m, "CUSA00001", "enableDeadZone", "1") == 0);
    CHECK(gp_model_find_section(m, "CUSA00001") != NULL);
    CHECK_STR_EQ(gp_get(m, "CUSA00001", "enableDeadZone"), "1");

    CHECK(gp_set(m, "CUSA00001", "DeadZoneLeft", "13") == 0);
    CHECK(gp_set(m, "CUSA00001", "enableDeadZone", "0") == 0);   /* update in place */
    CHECK(m->sections[0].count == 2);
    CHECK_STR_EQ(m->sections[0].items[0].key, "enableDeadZone");
    CHECK_STR_EQ(m->sections[0].items[0].value, "0");

    CHECK(gp_set(m, "CUSA00001", "enableDeadZone", NULL) == 0);  /* remove */
    CHECK(gp_get(m, "CUSA00001", "enableDeadZone") == NULL);
    CHECK(m->sections[0].count == 1);
    CHECK_STR_EQ(m->sections[0].items[0].key, "DeadZoneLeft");

    CHECK(gp_set(m, "CUSA00001", "enableDeadZone", NULL) == 0);  /* absent: no-op */
    CHECK(gp_set(m, "CUSA55555", "x", NULL) == 0);               /* absent section: no-op */
    CHECK(gp_model_find_section(m, "CUSA55555") == NULL);
    CHECK(m->count == 1);

    CHECK(gp_model_ensure_section(m, "CUSA77777") != NULL);
    CHECK(gp_model_ensure_section(m, "CUSA77777") == gp_model_find_section(m, "CUSA77777"));
    CHECK(m->count == 2);

    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "[CUSA00001]\nDeadZoneLeft=13\n\n[CUSA77777]\n\n");
    free(out);
    gp_model_free(m);
}

static void test_unknown_keys_preserved(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_ini_parse(m, "[default]\nSomethingNew=42\nenableDeadZone=1\nFutureFlag=on\n") == 0);
    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\nSomethingNew=42\nenableDeadZone=1\nFutureFlag=on\n\n");
    free(out);
    gp_model_free(m);
}

static void test_duplicate_key_and_section(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_ini_parse(m, "[default]\nDeadZoneLeft=13\nenableDeadZone=1\nDeadZoneLeft=40\n\n[CUSA00001]\na=1\n\n[default]\nb=2\n") == 0);
    CHECK(m->count == 2);
    CHECK_STR_EQ(gp_get(m, "default", "DeadZoneLeft"), "40");
    CHECK(m->sections[0].count == 3);
    CHECK_STR_EQ(m->sections[0].items[0].key, "DeadZoneLeft");   /* position of the first kept */
    CHECK_STR_EQ(m->sections[0].items[2].key, "b");              /* merged from the second [default] */

    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\nDeadZoneLeft=40\nenableDeadZone=1\nb=2\n\n[CUSA00001]\na=1\n\n");
    free(out);
    gp_model_free(m);
}

static void test_whitespace_crlf_bom(void)
{
    gp_model_t *m = gp_model_new();
    const char *text =
        "\xEF\xBB\xBF  [default]  \r\n"
        "\t enableDeadZone = 1 \r\n"
        "\r\n"
        "  DeadZoneLeft=  13\r\n"
        "# hash comment\r\n"
        "  ; semicolon comment\r\n"
        "orphan line\r\n"
        "TOUCH_L2=BUTTON_L3=extra\r\n"
        "[CUSA00001";   /* missing ']' and no final newline */

    CHECK(gp_ini_parse(m, text) == 0);
    CHECK(m->count == 2);
    CHECK_STR_EQ(m->sections[0].id, "default");
    CHECK_STR_EQ(gp_get(m, "default", "enableDeadZone"), "1");
    CHECK_STR_EQ(gp_get(m, "default", "DeadZoneLeft"), "13");
    CHECK_STR_EQ(gp_get(m, "default", "orphan line"), "");
    CHECK_STR_EQ(gp_get(m, "default", "TOUCH_L2"), "BUTTON_L3=extra");
    CHECK(m->sections[0].count == 4);
    CHECK_STR_EQ(m->sections[1].id, "CUSA00001");
    CHECK(m->sections[1].count == 0);
    gp_model_free(m);
}

static void test_headerless_lines(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_ini_parse(m, "stray=1\n[default]\nenableDeadZone=1\n") == 0);
    CHECK(m->count == 2);
    CHECK_STR_EQ(m->sections[0].id, "");
    CHECK_STR_EQ(gp_get(m, "", "stray"), "1");
    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "stray=1\n\n[default]\nenableDeadZone=1\n\n");
    free(out);

    /* an empty headerless section is not emitted */
    gp_model_clear(m);
    CHECK(gp_model_ensure_section(m, "") != NULL);
    CHECK(gp_model_ensure_section(m, "default") != NULL);
    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\n\n");
    free(out);
    gp_model_free(m);
}

static void test_exact_serialization(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_set(m, "default", "enableDeadZone", "1") == 0);
    CHECK(gp_set(m, "default", "DeadZoneLeft", "13") == 0);
    CHECK(gp_set(m, "CUSA00001", "VirationIntensity", "medium") == 0);
    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\nenableDeadZone=1\nDeadZoneLeft=13\n\n[CUSA00001]\nVirationIntensity=medium\n\n");
    free(out);
    gp_model_free(m);
}

static void test_round_trip(void)
{
    gp_model_t *a = gp_model_new();
    gp_model_t *b = gp_model_new();
    gp_model_t *c = gp_model_new();
    char *out;

    CHECK(gp_ini_parse(a, REAL_INI) == 0);
    out = gp_ini_serialize(a);
    CHECK(out != NULL);
    CHECK(gp_ini_parse(b, out) == 0);
    CHECK(gp_model_equal(a, b));
    free(out);

    /* equality is sensitive to values, keys and order */
    CHECK(gp_ini_parse(c, REAL_INI) == 0);
    CHECK(gp_model_equal(a, c));
    CHECK(gp_set(c, "default", "DeadZoneLeft", "14") == 0);
    CHECK(!gp_model_equal(a, c));
    CHECK(gp_set(c, "default", "DeadZoneLeft", "13") == 0);
    CHECK(gp_model_equal(a, c));
    CHECK(gp_set(c, "CUSA00002", "x", "1") == 0);
    CHECK(!gp_model_equal(a, c));

    gp_model_free(a);
    gp_model_free(b);
    gp_model_free(c);
}

static void test_clear_and_reuse(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_ini_parse(m, REAL_INI) == 0);
    CHECK(m->count == 5);
    gp_model_clear(m);
    CHECK(m->count == 0);
    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "");
    free(out);

    CHECK(gp_ini_parse(m, "[CUSA00001]\nk=v\n") == 0);
    CHECK(m->count == 1);
    CHECK_STR_EQ(gp_get(m, "CUSA00001", "k"), "v");
    gp_model_clear(m);
    gp_model_clear(m);   /* idempotent */
    CHECK(m->count == 0);
    gp_model_free(m);
    gp_model_free(NULL); /* tolerated */
}

/* Removing the last key of a section created by gp_set drops the section,
 * so a set-then-unset round trip leaves no empty "[CUSAxxxxx]" behind.
 * Sections that came from the file with no keys are still kept. */
static void test_remove_last_key_drops_section(void)
{
    gp_model_t *m = gp_model_new();
    char *out;

    CHECK(gp_ini_parse(m, "[default]\nenableDeadZone=1\n\n[CUSA00002]\n") == 0);
    CHECK(gp_set(m, "CUSA00001", "BUTTON_L3", "BUTTON_CROSS") == 0);
    CHECK(gp_model_find_section(m, "CUSA00001") != NULL);
    CHECK(gp_set(m, "CUSA00001", "BUTTON_L3", NULL) == 0);
    CHECK(gp_model_find_section(m, "CUSA00001") == NULL);
    CHECK(m->count == 2);
    /* the empty section that came from the file is untouched */
    CHECK(gp_model_find_section(m, "CUSA00002") != NULL);
    /* removing from default leaves the section (it still has no other key, but it existed in the file) */
    CHECK(gp_set(m, "default", "enableDeadZone", NULL) == 0);
    CHECK(gp_model_find_section(m, "default") == NULL);
    out = gp_ini_serialize(m);
    CHECK_STR_EQ(out, "[CUSA00002]\n\n");
    free(out);
    gp_model_free(m);
}

int main(void)
{
    RUN(test_empty_and_null);
    RUN(test_real_file);
    RUN(test_set_get_remove);
    RUN(test_unknown_keys_preserved);
    RUN(test_duplicate_key_and_section);
    RUN(test_whitespace_crlf_bom);
    RUN(test_headerless_lines);
    RUN(test_exact_serialization);
    RUN(test_round_trip);
    RUN(test_clear_and_reuse);
    RUN(test_remove_last_key_drops_section);

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
