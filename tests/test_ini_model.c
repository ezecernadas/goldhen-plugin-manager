/*
 * Host-side tests for the pure plugins.ini / plugin_manager.json model.
 * Build and run with `make test` (no PS4 toolchain required).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ini_model.h"

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

static const char *SAMPLE_INI =
    "[default]\n"
    "/data/GoldHEN/plugins/game_patch.prx\n"
    "/data/GoldHEN/plugins/system_language_override.prx ; es_ES\n"
    "\n"
    "[CUSA00001]\n"
    "/data/GoldHEN/plugins/afr.prx\n"
    "/data/GoldHEN/plugins/no_share_watermark.prx\n"
    "\n"
    "[CUSA99999]\n"
    "/data/GoldHEN/plugins/gone.prx ; a ; b\n";

static const char *SAMPLE_INI_CRLF =
    "[default]\r\n"
    "/data/GoldHEN/plugins/game_patch.prx\r\n"
    "/data/GoldHEN/plugins/system_language_override.prx ; es_ES\r\n"
    "\r\n"
    "[CUSA00001]\r\n"
    "/data/GoldHEN/plugins/afr.prx\r\n"
    "/data/GoldHEN/plugins/no_share_watermark.prx\r\n"
    "\r\n"
    "[CUSA99999]\r\n"
    "/data/GoldHEN/plugins/gone.prx ; a ; b";

static void test_empty_and_null(void)
{
    pm_model_t *m = pm_model_new();
    char *out;

    CHECK(m != NULL);
    CHECK(pm_ini_parse(m, NULL) == 0);
    CHECK(m->count == 0);
    CHECK(pm_ini_parse(m, "") == 0);
    CHECK(m->count == 0);

    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "");
    free(out);
    pm_model_free(m);
}

static void test_default_only(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;
    char *out;

    CHECK(pm_ini_parse(m, "[default]\n/data/GoldHEN/plugins/game_patch.prx\n") == 0);
    CHECK(m->count == 1);
    t = pm_model_find_target(m, "default");
    CHECK(t != NULL);
    if (t) {
        CHECK(t->count == 1);
        CHECK_STR_EQ(t->plugins[0].path, "/data/GoldHEN/plugins/game_patch.prx");
        CHECK(t->plugins[0].args == NULL);
        CHECK(t->plugins[0].enabled == true);
    }

    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\n/data/GoldHEN/plugins/game_patch.prx\n\n");
    free(out);
    pm_model_free(m);
}

static void test_args_survive_toggle(void)
{
    const char *path = "/data/GoldHEN/plugins/system_language_override.prx";
    pm_model_t *m = pm_model_new();
    pm_plugin_t *p;
    char *out;

    CHECK(pm_ini_parse(m, "[default]\n/data/GoldHEN/plugins/system_language_override.prx ; es_ES\n") == 0);
    CHECK(pm_model_is_enabled(m, "default", path) == true);

    CHECK(pm_model_set_enabled(m, "default", path, false) == 0);
    CHECK(pm_model_is_enabled(m, "default", path) == false);
    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\n\n");
    free(out);

    CHECK(pm_model_set_enabled(m, "default", path, true) == 0);
    CHECK(pm_model_is_enabled(m, "default", path) == true);
    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\n/data/GoldHEN/plugins/system_language_override.prx ; es_ES\n\n");
    free(out);

    p = pm_target_find_plugin(pm_model_find_target(m, "default"), path);
    CHECK(p != NULL);
    if (p)
        CHECK_STR_EQ(p->args, "es_ES");

    pm_model_free(m);
}

static void test_uninstalled_sections_preserved(void)
{
    pm_model_t *m = pm_model_new();
    char *out;

    CHECK(pm_ini_parse(m, "[default]\na.prx\n\n[CUSA99999]\nb.prx ; x\n") == 0);
    CHECK(m->count == 2);

    out = pm_ini_serialize(m);
    CHECK(out != NULL && strstr(out, "[CUSA99999]\nb.prx ; x\n") != NULL);
    free(out);

    /* A section whose plugins are all disabled still emits its header. */
    CHECK(pm_model_set_enabled(m, "CUSA99999", "b.prx", false) == 0);
    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\na.prx\n\n[CUSA99999]\n\n");
    free(out);

    pm_model_free(m);
}

static void test_whitespace_crlf_no_final_newline(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;

    CHECK(pm_ini_parse(m,
        "  [default]  \r\n"
        "\t/data/GoldHEN/plugins/a.prx  \r\n"
        "\r\n"
        "  /data/GoldHEN/plugins/b.prx ;  arg one  ") == 0);
    CHECK(m->count == 1);
    t = pm_model_find_target(m, "default");
    CHECK(t != NULL);
    if (t) {
        CHECK(t->count == 2);
        if (t->count == 2) {
            CHECK_STR_EQ(t->plugins[0].path, "/data/GoldHEN/plugins/a.prx");
            CHECK(t->plugins[0].args == NULL);
            CHECK_STR_EQ(t->plugins[1].path, "/data/GoldHEN/plugins/b.prx");
            CHECK_STR_EQ(t->plugins[1].args, "arg one");
        }
    }
    pm_model_free(m);
}

static void test_comments_and_empty_path_lines(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;

    CHECK(pm_ini_parse(m, "[default]\n# comment\n; /data/GoldHEN/plugins/c.prx\n;\n") == 0);
    CHECK(m->count == 1);
    t = pm_model_find_target(m, "default");
    CHECK(t != NULL);
    if (t)
        CHECK(t->count == 0);
    pm_model_free(m);
}

static void test_duplicate_sections_and_plugins(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;

    CHECK(pm_ini_parse(m,
        "[CUSA00001]\n"
        "a.prx\n"
        "[default]\n"
        "x.prx\n"
        "[CUSA00001]\n"
        "a.prx ; adopted\n"
        "b.prx ; first\n"
        "b.prx ; ignored\n") == 0);
    CHECK(m->count == 2);
    t = pm_model_find_target(m, "CUSA00001");
    CHECK(t != NULL);
    if (t) {
        CHECK(t->count == 2);
        if (t->count == 2) {
            CHECK_STR_EQ(t->plugins[0].path, "a.prx");
            CHECK_STR_EQ(t->plugins[0].args, "adopted");
            CHECK_STR_EQ(t->plugins[1].path, "b.prx");
            CHECK_STR_EQ(t->plugins[1].args, "first");
        }
    }
    pm_model_free(m);
}

static void test_lines_before_header(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;
    char *out;

    CHECK(pm_ini_parse(m, "orphan.prx ; o\n[default]\na.prx\n") == 0);
    CHECK(m->count == 2);
    t = pm_model_find_target(m, "");
    CHECK(t != NULL);
    if (t) {
        CHECK(t->count == 1);
        CHECK_STR_EQ(t->plugins[0].path, "orphan.prx");
    }
    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "orphan.prx ; o\n\n[default]\na.prx\n\n");
    free(out);
    pm_model_free(m);
}

static void test_round_trip_ini(void)
{
    pm_model_t *a = pm_model_new();
    pm_model_t *b = pm_model_new();
    pm_model_t *c = pm_model_new();
    char *out;

    CHECK(pm_ini_parse(a, SAMPLE_INI) == 0);
    CHECK(a->count == 3);

    out = pm_ini_serialize(a);
    CHECK(out != NULL);
    CHECK(pm_ini_parse(b, out) == 0);
    CHECK(pm_model_equal(a, b));
    free(out);

    CHECK(pm_ini_parse(c, SAMPLE_INI_CRLF) == 0);
    CHECK(pm_model_equal(a, c));

    /* Sanity: equality is not trivially true. */
    CHECK(pm_model_set_enabled(c, "default", "/data/GoldHEN/plugins/game_patch.prx", false) == 0);
    CHECK(!pm_model_equal(a, c));

    pm_model_free(a);
    pm_model_free(b);
    pm_model_free(c);
}

static void test_round_trip_json(void)
{
    pm_model_t *a = pm_model_new();
    pm_model_t *b = pm_model_new();
    pm_model_t *bad;
    pm_target_t *t;
    pm_plugin_t *p;
    char *json;

    CHECK(pm_ini_parse(a, SAMPLE_INI) == 0);
    CHECK(pm_model_set_enabled(a, "CUSA00001", "/data/GoldHEN/plugins/afr.prx", false) == 0);
    CHECK(pm_model_set_enabled(a, "", "/data/GoldHEN/plugins/orphan.prx", true) == 0);

    /* Inject awkward characters into args. */
    t = pm_model_find_target(a, "CUSA99999");
    CHECK(t != NULL);
    p = t ? pm_target_find_plugin(t, "/data/GoldHEN/plugins/gone.prx") : NULL;
    CHECK(p != NULL);
    if (p) {
        free(p->args);
        p->args = malloc(32);
        CHECK(p->args != NULL);
        if (p->args)
            strcpy(p->args, "q\"uote\\back\tslash\x01");
    }

    json = pm_json_serialize(a);
    CHECK(json != NULL);
    CHECK(json != NULL && strstr(json, "\"version\":1") != NULL);
    CHECK(json != NULL && strstr(json, "\"args\":null") != NULL);
    CHECK(json != NULL && strstr(json, "\\\"uote\\\\back\\tslash\\u0001") != NULL);
    CHECK(json != NULL && json[strlen(json) - 1] == '\n');

    CHECK(pm_json_parse(b, json) == 0);
    CHECK(pm_model_equal(a, b));
    free(json);

    /* Whitespace and key order tolerance. */
    pm_model_free(b);
    b = pm_model_new();
    CHECK(pm_json_parse(b,
        " { \"targets\" : [ { \"plugins\" : [ { \"enabled\" : false , \"args\" : \"\\u0065s_ES\" , "
        "\"path\" : \"/x.prx\" } ] , \"id\" : \"default\" } ] , \"version\" : 1 } ") == 0);
    t = pm_model_find_target(b, "default");
    CHECK(t != NULL && t->count == 1);
    if (t && t->count == 1) {
        CHECK_STR_EQ(t->plugins[0].path, "/x.prx");
        CHECK_STR_EQ(t->plugins[0].args, "es_ES");
        CHECK(t->plugins[0].enabled == false);
    }

    /* Rejections. */
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{}") == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{\"version\":2,\"targets\":[]}") == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{\"version\":1,\"targets\":[{\"id\":\"d\",\"plugins\":[") == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{\"version\":1,\"targets\":[],\"extra\":1}") == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "") == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, NULL) == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{\"version\":1,\"targets\":[]} x") == -2); pm_model_free(bad);
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{\"version\":1,\"targets\":[{\"id\":\"d\",\"plugins\":[{\"path\":\"p\",\"args\":null,\"enabled\":true}]}]}") == 0); pm_model_free(bad);
    /* Missing required key inside plugin. */
    bad = pm_model_new(); CHECK(pm_json_parse(bad, "{\"version\":1,\"targets\":[{\"id\":\"d\",\"plugins\":[{\"args\":null,\"enabled\":true}]}]}") == -2); pm_model_free(bad);

    pm_model_free(a);
    pm_model_free(b);
}

static void test_set_enabled_creates(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;

    CHECK(pm_model_is_enabled(m, "CUSA00002", "/p.prx") == false);
    CHECK(pm_model_set_enabled(m, "CUSA00002", "/p.prx", true) == 0);
    CHECK(m->count == 1);
    t = pm_model_find_target(m, "CUSA00002");
    CHECK(t != NULL && t->count == 1);
    if (t && t->count == 1) {
        CHECK(t->plugins[0].args == NULL);
        CHECK(t->plugins[0].enabled == true);
    }
    CHECK(pm_model_is_enabled(m, "CUSA00002", "/p.prx") == true);
    CHECK(pm_model_is_enabled(m, "CUSA00002", "/other.prx") == false);
    CHECK(pm_model_is_enabled(m, "nope", "/p.prx") == false);

    /* Disabling an unknown plugin creates a disabled entry, nothing else. */
    CHECK(pm_model_set_enabled(m, "CUSA00002", "/q.prx", false) == 0);
    CHECK(t->count == 2);
    CHECK(pm_model_is_enabled(m, "CUSA00002", "/q.prx") == false);

    CHECK(pm_model_ensure_target(m, "CUSA00002") == t);
    CHECK(pm_model_ensure_target(m, "new") != NULL);
    CHECK(m->count == 2);

    pm_model_free(m);
}

static void test_display_name(void)
{
    char buf[64];

    pm_plugin_display_name("no_share_watermark.prx", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "No Share Watermark");

    pm_plugin_display_name("/data/GoldHEN/plugins/afr.prx", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Afr");

    pm_plugin_display_name("Weird_NAME.PRX", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Weird NAME");

    pm_plugin_display_name("/data/GoldHEN/plugins/noext", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "Noext");

    pm_plugin_display_name("", buf, sizeof(buf));
    CHECK_STR_EQ(buf, "");

    pm_plugin_display_name(NULL, buf, sizeof(buf));
    CHECK_STR_EQ(buf, "");

    memset(buf, 'X', sizeof(buf));
    pm_plugin_display_name("no_share_watermark.prx", buf, 4);
    CHECK_STR_EQ(buf, "No ");
    CHECK(buf[3] == '\0');

    pm_plugin_display_name("x.prx", buf, 0);   /* must not write */
    CHECK(buf[0] == 'N');
}

static void test_malformed_header_lenient(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;

    CHECK(pm_ini_parse(m, "[CUSA00001\na.prx\n[ spaced ]\nb.prx\n") == 0);
    CHECK(m->count == 2);
    t = pm_model_find_target(m, "CUSA00001");
    CHECK(t != NULL && t->count == 1);
    t = pm_model_find_target(m, "spaced");
    CHECK(t != NULL && t->count == 1);
    pm_model_free(m);
}

static void test_bom_and_path_normalization(void)
{
    pm_model_t *m = pm_model_new();
    pm_target_t *t;
    pm_plugin_t *p;
    char *out;

    /* UTF-8 BOM before the first header must not turn it into a plugin line;
     * consecutive '/' collapse so hand-written spellings match the disk listing. */
    CHECK(pm_ini_parse(m, "\xEF\xBB\xBF[default]\n"
                          "/data/GoldHEN/plugins//x.prx\n"
                          "/data/GoldHEN/plugins/x.prx ; a\n") == 0);
    CHECK(m->count == 1);
    t = pm_model_find_target(m, "default");
    CHECK(t != NULL);
    CHECK(t != NULL && t->count == 1);
    p = pm_target_find_plugin(t, "/data/GoldHEN/plugins/x.prx");
    CHECK(p != NULL);
    if (p) {
        CHECK_STR_EQ(p->path, "/data/GoldHEN/plugins/x.prx");
        CHECK_STR_EQ(p->args, "a");
        CHECK(p->enabled);
    }
    CHECK(pm_target_find_plugin(t, "/data/GoldHEN/plugins//x.prx") == p);
    CHECK(pm_target_find_plugin(t, "//data//GoldHEN/plugins/x.prx") == p);
    CHECK(pm_model_is_enabled(m, "default", "/data/GoldHEN/plugins//x.prx"));

    /* Toggling through a non-normalized spelling hits the same entry. */
    CHECK(pm_model_set_enabled(m, "default", "/data/GoldHEN/plugins//x.prx", false) == 0);
    CHECK(t->count == 1);
    CHECK(!pm_model_is_enabled(m, "default", "/data/GoldHEN/plugins/x.prx"));
    CHECK(pm_model_set_enabled(m, "default", "///data/GoldHEN/plugins/x.prx", true) == 0);
    CHECK(t->count == 1);

    /* A brand-new entry created through set_enabled is stored normalized. */
    CHECK(pm_model_set_enabled(m, "CUSA00001", "/data/GoldHEN/plugins//new.prx", true) == 0);
    t = pm_model_find_target(m, "CUSA00001");
    CHECK(t != NULL && t->count == 1);
    if (t && t->count == 1)
        CHECK_STR_EQ(t->plugins[0].path, "/data/GoldHEN/plugins/new.prx");

    out = pm_ini_serialize(m);
    CHECK_STR_EQ(out, "[default]\n/data/GoldHEN/plugins/x.prx ; a\n\n"
                      "[CUSA00001]\n/data/GoldHEN/plugins/new.prx\n\n");
    free(out);

    /* Case is preserved: the PS4 filesystem is case-sensitive. */
    CHECK(pm_target_find_plugin(pm_model_find_target(m, "default"), "/data/GoldHEN/plugins/X.prx") == NULL);

    /* A BOM alone or a BOM followed by a plugin line (no header). */
    pm_model_free(m);
    m = pm_model_new();
    CHECK(pm_ini_parse(m, "\xEF\xBB\xBF") == 0);
    CHECK(m->count == 0);
    CHECK(pm_ini_parse(m, "\xEF\xBB\xBF" "a.prx\n") == 0);
    t = pm_model_find_target(m, "");
    CHECK(t != NULL && t->count == 1);
    pm_model_free(m);
}

static void test_model_clear(void)
{
    pm_model_t *m = pm_model_new();
    pm_model_t *ref = pm_model_new();

    CHECK(pm_ini_parse(m, SAMPLE_INI) == 0);
    CHECK(m->count == 3);

    pm_model_clear(m);
    CHECK(m->count == 0);
    CHECK(pm_model_find_target(m, "default") == NULL);

    /* The struct stays usable after a clear. */
    CHECK(pm_ini_parse(m, SAMPLE_INI) == 0);
    CHECK(pm_ini_parse(ref, SAMPLE_INI) == 0);
    CHECK(pm_model_equal(m, ref));

    pm_model_clear(m);
    pm_model_clear(m);      /* idempotent */
    pm_model_clear(NULL);   /* safe */
    CHECK(m->count == 0);

    pm_model_free(m);
    pm_model_free(ref);
}

static void test_plugin_args_get_set(void)
{
    pm_model_t *m = pm_model_new();
    pm_model_t *back;
    pm_plugin_t *p;
    char *out;
    char *json;
    const char *path = "/data/GoldHEN/plugins/system_language_override.prx";

    CHECK(pm_ini_parse(m, "[default]\n/data/GoldHEN/plugins/afr.prx\n") == 0);

    /* absent target / plugin / args */
    CHECK(pm_model_get_args(m, "CUSA00001", path) == NULL);
    CHECK(pm_model_get_args(m, "default", path) == NULL);
    CHECK(pm_model_get_args(m, "default", "/data/GoldHEN/plugins/afr.prx") == NULL);

    /* set on a missing plugin creates a DISABLED entry with the args */
    CHECK(pm_model_set_args(m, "CUSA00001", path, "es_ES") == 0);
    CHECK_STR_EQ(pm_model_get_args(m, "CUSA00001", path), "es_ES");
    CHECK(pm_model_is_enabled(m, "CUSA00001", path) == false);
    p = pm_target_find_plugin(pm_model_find_target(m, "CUSA00001"), path);
    CHECK(p != NULL && p->enabled == false);
    out = pm_ini_serialize(m);
    CHECK(out != NULL && strstr(out, "system_language_override") == NULL);
    CHECK(out != NULL && strstr(out, "[CUSA00001]\n") != NULL);
    free(out);

    /* the JSON sidecar keeps the args of the disabled entry */
    json = pm_json_serialize(m);
    back = pm_model_new();
    CHECK(pm_json_parse(back, json) == 0);
    CHECK_STR_EQ(pm_model_get_args(back, "CUSA00001", path), "es_ES");
    CHECK(pm_model_equal(m, back));
    pm_model_free(back);
    free(json);

    /* set on an enabled plugin changes the emitted line */
    CHECK(pm_model_set_enabled(m, "CUSA00001", path, true) == 0);
    CHECK(pm_model_set_args(m, "CUSA00001", path, "pt_BR") == 0);
    CHECK(pm_model_is_enabled(m, "CUSA00001", path) == true);
    out = pm_ini_serialize(m);
    CHECK(out != NULL && strstr(out, "/data/GoldHEN/plugins/system_language_override.prx ; pt_BR\n") != NULL);
    free(out);

    /* NULL clears, keeps enabled */
    CHECK(pm_model_set_args(m, "CUSA00001", path, NULL) == 0);
    CHECK(pm_model_get_args(m, "CUSA00001", path) == NULL);
    p = pm_target_find_plugin(pm_model_find_target(m, "CUSA00001"), path);
    CHECK(p != NULL && p->args == NULL && p->enabled == true);
    out = pm_ini_serialize(m);
    CHECK(out != NULL && strstr(out, "/data/GoldHEN/plugins/system_language_override.prx\n") != NULL);
    CHECK(out != NULL && strstr(out, ";") == NULL);
    free(out);

    /* "" clears too */
    CHECK(pm_model_set_args(m, "CUSA00001", path, "ja") == 0);
    CHECK(pm_model_set_args(m, "CUSA00001", path, "") == 0);
    CHECK(pm_model_get_args(m, "CUSA00001", path) == NULL);
    CHECK(pm_model_is_enabled(m, "CUSA00001", path) == true);

    /* path normalization applies to both get and set */
    CHECK(pm_model_set_args(m, "CUSA00001", "/data/GoldHEN//plugins/system_language_override.prx", "uk") == 0);
    CHECK_STR_EQ(pm_model_get_args(m, "CUSA00001", path), "uk");
    CHECK_STR_EQ(pm_model_get_args(m, "CUSA00001", "/data//GoldHEN/plugins//system_language_override.prx"), "uk");
    CHECK(pm_model_find_target(m, "CUSA00001")->count == 1);

    /* set on an existing target that lacks the plugin: created disabled */
    CHECK(pm_model_set_args(m, "default", path, "de") == 0);
    CHECK(pm_model_is_enabled(m, "default", path) == false);
    CHECK_STR_EQ(pm_model_get_args(m, "default", path), "de");
    CHECK(pm_model_is_enabled(m, "default", "/data/GoldHEN/plugins/afr.prx") == true);

    pm_model_free(m);
}

int main(void)
{
    RUN(test_empty_and_null);
    RUN(test_default_only);
    RUN(test_args_survive_toggle);
    RUN(test_uninstalled_sections_preserved);
    RUN(test_whitespace_crlf_no_final_newline);
    RUN(test_comments_and_empty_path_lines);
    RUN(test_duplicate_sections_and_plugins);
    RUN(test_lines_before_header);
    RUN(test_round_trip_ini);
    RUN(test_round_trip_json);
    RUN(test_set_enabled_creates);
    RUN(test_display_name);
    RUN(test_malformed_header_lenient);
    RUN(test_bom_and_path_normalization);
    RUN(test_model_clear);
    RUN(test_plugin_args_get_set);

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
