/**
 * script.cpp — the Lua app runtime. See D037.
 */
#include "script.h"
#include "app_api.h"
#include "app_host.h"

#include <Arduino.h>
#include <setjmp.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

/*
 * <climits> BEFORE lua.h, and it is load-bearing.
 *
 * luaconf.h picks its integer type by testing for LLONG_MAX, which newlib's
 * <limits.h> hides in C++ mode — so from a .cpp the header decides the
 * compiler has no 'long long' and #errors out. libstdc++'s <climits> defines
 * the macro if it is missing, which is exactly the gap.
 */
#include <climits>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
LV_FONT_DECLARE(fliqlo_digits);
LV_FONT_DECLARE(fliqlo_mid);
LV_FONT_DECLARE(fliqlo_corner);
}

#define APPS_DIR      "/apps"
#define MAX_SCRIPTS   12
#define MAX_OBJECTS   64
#define NAME_MAX      23
#define SCRIPT_MAX    16384      /* per file; a UI script that needs more
                                  * wants to be a native app */

/*
 * Instruction budget per entry point. A script with `while true do end` runs
 * on the LVGL task, so without this it freezes the clock and the only way out
 * is the cable — precisely the failure "add apps without restarting" must not
 * have. 400k VM instructions is generous for building a screen and absurd for
 * a tick.
 */
#define INSTR_BUDGET  400000

struct ScriptApp {
    App  app;
    char name[NAME_MAX + 1];
    bool used;
};

static ScriptApp  scripts[MAX_SCRIPTS];
static int        script_n;
static bool       fs_ok;
static volatile bool rescan_pending;

static lua_State *L;                  /* one live app at a time, by contract */
static lv_obj_t  *cur_scr;
static lv_obj_t  *objects[MAX_OBJECTS];
static int        object_n;
static ScriptApp *cur_script;

/* ------------------------------------------------------------ allocator -- */
/*
 * Lua allocates from PSRAM, never the internal heap. There is 8 MB of PSRAM
 * and roughly 60 KB of internal heap in use; a script that leaks must not be
 * able to starve the display driver, which allocates from internal memory.
 */
static void *lua_alloc(void *, void *ptr, size_t, size_t nsize)
{
    if (nsize == 0) { heap_caps_free(ptr); return nullptr; }
    return heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

/*
 * A Lua error raised OUTSIDE a pcall has no handler, and Lua's default
 * behaviour for that is abort() — which reboots the clock. That is exactly
 * what happened when luaL_checkversion() failed inside luaL_requiref: a
 * library mistake took down the whole device, in a runtime whose entire
 * promise was that a broken script fails alone (D037).
 *
 * A panic function must not return; if it does, Lua aborts anyway. So it
 * longjmps back to script_create, which reports the message on screen and
 * closes the state. See D040.
 */
static jmp_buf panic_jmp;
static bool    panic_armed;
static char    panic_msg[160];

static int lua_panic(lua_State *Ls)
{
    const char *m = lua_tostring(Ls, -1);
    snprintf(panic_msg, sizeof(panic_msg), "%s", m ? m : "(no message)");
    Serial.printf("[lua] PANIC: %s\n", panic_msg);
    if (panic_armed) { panic_armed = false; longjmp(panic_jmp, 1); }
    return 0;                    /* unreachable while armed */
}

static void instr_hook(lua_State *Ls, lua_Debug *)
{
    luaL_error(Ls, "script ran too long (over %d instructions) and was stopped",
               INSTR_BUDGET);
}

/* ---------------------------------------------------------- arg helpers -- */

static int opt_int(lua_State *Ls, int idx, const char *k, int def)
{
    lua_getfield(Ls, idx, k);
    int v = lua_isnil(Ls, -1) ? def : (int)lua_tointeger(Ls, -1);
    lua_pop(Ls, 1);
    return v;
}

static void opt_str(lua_State *Ls, int idx, const char *k,
                    char *out, size_t cap, const char *def)
{
    lua_getfield(Ls, idx, k);
    const char *s = lua_isnil(Ls, -1) ? def : lua_tostring(Ls, -1);
    if (!s) s = def;
    snprintf(out, cap, "%s", s ? s : "");
    lua_pop(Ls, 1);           /* copied first: popping may make it collectable */
}

static const lv_font_t *font_by_name(const char *n, int size)
{
    if (!strcmp(n, "digits")) return &fliqlo_digits;
    if (!strcmp(n, "mid"))    return &fliqlo_mid;
    if (!strcmp(n, "corner")) return &fliqlo_corner;
    if (size >= 32) return &lv_font_montserrat_32;
    if (size >= 28) return &lv_font_montserrat_28;
    if (size >= 24) return &lv_font_montserrat_24;
    if (size >= 20) return &lv_font_montserrat_20;
    return &lv_font_montserrat_18;
}

/* Handles are indices, not pointers: a script cannot forge one into memory. */
static int push_handle(lua_State *Ls, lv_obj_t *o)
{
    if (object_n >= MAX_OBJECTS) {
        lua_pushnil(Ls);
        return 1;
    }
    objects[object_n] = o;
    lua_pushinteger(Ls, object_n++);
    return 1;
}

static lv_obj_t *handle_obj(lua_State *Ls, int idx)
{
    int h = (int)luaL_checkinteger(Ls, idx);
    if (h < 0 || h >= object_n || !objects[h])
        luaL_error(Ls, "bad object handle %d", h);
    return objects[h];
}

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

/* ----------------------------------------------------------- ui bindings -- */

static int l_ui_card(lua_State *Ls)
{
    luaL_checktype(Ls, 1, LUA_TTABLE);
    lv_obj_t *c = lv_obj_create(cur_scr);
    decor(c);
    lv_obj_set_size(c, opt_int(Ls, 1, "w", 268), opt_int(Ls, 1, "h", 232));
    lv_obj_set_pos(c, opt_int(Ls, 1, "x", 0), opt_int(Ls, 1, "y", 0));
    lv_obj_set_style_bg_color(c, lv_color_hex(opt_int(Ls, 1, "color", 0x161616)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(c, opt_int(Ls, 1, "radius", 26), LV_PART_MAIN);
    return push_handle(Ls, c);
}

static int l_ui_label(lua_State *Ls)
{
    luaL_checktype(Ls, 1, LUA_TTABLE);
    char text[128], font[16];
    opt_str(Ls, 1, "text", text, sizeof(text), "");
    opt_str(Ls, 1, "font", font, sizeof(font), "");
    int size = opt_int(Ls, 1, "size", 20);

    lv_obj_t *l = lv_label_create(cur_scr);
    lv_obj_set_style_text_font(l, font_by_name(font, size), LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(opt_int(Ls, 1, "color", 0xFFFFFF)),
                                LV_PART_MAIN);
    lv_label_set_text(l, text);
    int w = opt_int(Ls, 1, "w", 0);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
    lv_obj_set_pos(l, opt_int(Ls, 1, "x", 0), opt_int(Ls, 1, "y", 0));
    return push_handle(Ls, l);
}

/* Each button's Lua function lives in the registry; the ref is the user data,
 * so the C callback needs no map. Refs die with the lua_State on exit. */
static void button_cb(lv_event_t *e)
{
    if (!L) return;
    int ref = (int)(intptr_t)lv_event_get_user_data(e);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_sethook(L, instr_hook, LUA_MASKCOUNT, INSTR_BUDGET);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        Serial.printf("[lua] on_click: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static int l_ui_button(lua_State *Ls)
{
    luaL_checktype(Ls, 1, LUA_TTABLE);
    char text[64];
    opt_str(Ls, 1, "text", text, sizeof(text), "");

    lv_obj_t *b = lv_obj_create(cur_scr);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, opt_int(Ls, 1, "w", 200), opt_int(Ls, 1, "h", 60));
    lv_obj_set_pos(b, opt_int(Ls, 1, "x", 0), opt_int(Ls, 1, "y", 0));
    lv_obj_set_style_bg_color(b, lv_color_hex(opt_int(Ls, 1, "color", 0x2A2A2A)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, opt_int(Ls, 1, "radius", 14), LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x3C3C3C), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);

    lua_getfield(Ls, 1, "on_click");
    if (lua_isfunction(Ls, -1)) {
        int ref = luaL_ref(Ls, LUA_REGISTRYINDEX);      /* pops the function */
        lv_obj_add_event_cb(b, button_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ref);
    } else {
        lua_pop(Ls, 1);
    }

    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font_by_name("", opt_int(Ls, 1, "size", 20)),
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xE8E8E8), LV_PART_MAIN);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return push_handle(Ls, b);
}

static int l_ui_set_text(lua_State *Ls)
{
    lv_obj_t *o = handle_obj(Ls, 1);
    const char *s = luaL_checkstring(Ls, 2);
    /* A button's label is its first child; a label is itself. */
    if (lv_obj_check_type(o, &lv_label_class)) lv_label_set_text(o, s);
    else if (lv_obj_get_child_cnt(o) > 0)      lv_label_set_text(lv_obj_get_child(o, 0), s);
    return 0;
}

static int l_ui_set_color(lua_State *Ls)
{
    lv_obj_t *o = handle_obj(Ls, 1);
    lv_color_t c = lv_color_hex((uint32_t)luaL_checkinteger(Ls, 2));
    if (lv_obj_check_type(o, &lv_label_class))
        lv_obj_set_style_text_color(o, c, LV_PART_MAIN);
    else
        lv_obj_set_style_bg_color(o, c, LV_PART_MAIN);
    return 0;
}

static int l_ui_set_pos(lua_State *Ls)
{
    lv_obj_set_pos(handle_obj(Ls, 1), (lv_coord_t)luaL_checkinteger(Ls, 2),
                   (lv_coord_t)luaL_checkinteger(Ls, 3));
    return 0;
}

static int l_ui_hide(lua_State *Ls)
{
    lv_obj_t *o = handle_obj(Ls, 1);
    if (lua_toboolean(Ls, 2)) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else                      lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

/* --------------------------------------------------------- gpio bindings -- */
/*
 * The same whitelist the Lab enforces, for the same reasons — a script may not
 * touch a pin the firmware owns, and GPIO0 is readable but never drivable.
 * See D035 and D036.
 */
static const uint8_t SAFE_PINS[]    = { 0, 21, 38, 39, 40, 41, 42, 47, 48, 43, 44, 1, 2, 3, 4 };
static const uint8_t IN_ONLY_PINS[] = { 0 };

static bool pin_allowed(int p, bool as_output)
{
    bool found = false;
    for (unsigned i = 0; i < sizeof(SAFE_PINS); i++) if (SAFE_PINS[i] == p) found = true;
    if (!found) return false;
    if (as_output)
        for (unsigned i = 0; i < sizeof(IN_ONLY_PINS); i++) if (IN_ONLY_PINS[i] == p) return false;
    return true;
}

static int l_gpio_mode(lua_State *Ls)
{
    int p = (int)luaL_checkinteger(Ls, 1);
    const char *m = luaL_checkstring(Ls, 2);
    bool out = !strcmp(m, "out");
    if (!pin_allowed(p, out))
        return luaL_error(Ls, "GPIO%d is not available to scripts%s", p,
                          out ? " as an output" : "");
    if (out)                        pinMode(p, OUTPUT);
    else if (!strcmp(m, "in_pu"))   pinMode(p, INPUT_PULLUP);
    else if (!strcmp(m, "in_pd"))   pinMode(p, INPUT_PULLDOWN);
    else                            pinMode(p, INPUT);
    return 0;
}

static int l_gpio_write(lua_State *Ls)
{
    int p = (int)luaL_checkinteger(Ls, 1);
    if (!pin_allowed(p, true)) return luaL_error(Ls, "GPIO%d is not drivable", p);
    digitalWrite(p, lua_toboolean(Ls, 2) ? HIGH : LOW);
    return 0;
}

static int l_gpio_read(lua_State *Ls)
{
    int p = (int)luaL_checkinteger(Ls, 1);
    if (!pin_allowed(p, false)) return luaL_error(Ls, "GPIO%d is not readable", p);
    lua_pushboolean(Ls, digitalRead(p) == HIGH);
    return 1;
}

/* -------------------------------------------------------- misc bindings -- */

static int l_millis(lua_State *Ls) { lua_pushinteger(Ls, (lua_Integer)millis()); return 1; }

static int l_log(lua_State *Ls)
{
    Serial.printf("[lua:%s] %s\n", cur_script ? cur_script->name : "?",
                  luaL_checkstring(Ls, 1));
    return 0;
}

static int l_back(lua_State *Ls) { (void)Ls; app_host_back(); return 0; }

static void register_api(lua_State *Ls)
{
    static const luaL_Reg ui[] = {
        { "card", l_ui_card }, { "label", l_ui_label }, { "button", l_ui_button },
        { "set_text", l_ui_set_text }, { "set_color", l_ui_set_color },
        { "set_pos", l_ui_set_pos }, { "hide", l_ui_hide }, { nullptr, nullptr }
    };
    static const luaL_Reg gpio[] = {
        { "mode", l_gpio_mode }, { "write", l_gpio_write }, { "read", l_gpio_read },
        { nullptr, nullptr }
    };
    luaL_newlib(Ls, ui);   lua_setglobal(Ls, "ui");
    luaL_newlib(Ls, gpio); lua_setglobal(Ls, "gpio");
    lua_pushcfunction(Ls, l_millis); lua_setglobal(Ls, "millis");
    lua_pushcfunction(Ls, l_log);    lua_setglobal(Ls, "log");
    lua_pushcfunction(Ls, l_back);   lua_setglobal(Ls, "back");
}

/* ------------------------------------------------------------- lifecycle -- */

static void show_error(const char *what, const char *msg)
{
    Serial.printf("[lua] %s: %s\n", what, msg);
    if (!cur_scr) return;
    lv_obj_t *l = lv_label_create(cur_scr);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xE0483B), LV_PART_MAIN);
    lv_obj_set_width(l, 560);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text_fmt(l, "%s\n\n%s", what, msg);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 20, 20);
}

/* Call a global if it exists. Returns false on error (already reported). */
static bool call_global(const char *fn, int nres)
{
    if (!L) return false;
    lua_getglobal(L, fn);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return false; }
    lua_sethook(L, instr_hook, LUA_MASKCOUNT, INSTR_BUDGET);
    if (lua_pcall(L, 0, nres, 0) != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        show_error(fn, msg ? msg : "(no message)");
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static lv_obj_t *script_create(void)
{
    cur_script = nullptr;
    const App *me = app_host_running();
    for (int i = 0; i < MAX_SCRIPTS; i++)
        if (scripts[i].used && &scripts[i].app == me) { cur_script = &scripts[i]; break; }

    object_n = 0;
    memset(objects, 0, sizeof(objects));

    cur_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(cur_scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_clear_flag(cur_scr, LV_OBJ_FLAG_SCROLLABLE);

    if (!cur_script) { show_error("runtime", "script not found"); return cur_scr; }

    /* A fresh state per launch: nothing survives an exit, so a script cannot
     * leave anything behind for the next one. */
    L = lua_newstate(lua_alloc, nullptr);
    if (!L) { show_error("runtime", "out of PSRAM for the interpreter"); return cur_scr; }
    lua_atpanic(L, lua_panic);

    /* Everything from here to on_create() runs under the panic guard. Lua API
     * calls made outside a pcall — luaL_requiref and luaL_newlib among them —
     * can raise, and unguarded that is a reboot rather than an error. */
    if (setjmp(panic_jmp) != 0) {
        show_error("lua panic", panic_msg);
        if (L) { lua_close(L); L = nullptr; }   /* unusable after a panic */
        return cur_scr;
    }
    panic_armed = true;

    /* Deliberately NOT loaded: io, os, package, debug. A UI script has no
     * business opening files, spawning processes or loading C modules, and
     * every one of those is a way to escape the sandbox this provides. */
    luaL_requiref(L, LUA_GNAME,      luaopen_base,   1); lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table,  1); lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1); lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math,  1); lua_pop(L, 1);
    register_api(L);

    char path[48];
    snprintf(path, sizeof(path), APPS_DIR "/%s.lua", cur_script->name);
    File f = LittleFS.open(path, "r");
    if (!f) { show_error("load", "cannot open the script file"); return cur_scr; }
    size_t len = f.size();
    if (len > SCRIPT_MAX) { f.close(); show_error("load", "script too large"); return cur_scr; }
    char *buf = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { f.close(); show_error("load", "out of PSRAM"); return cur_scr; }
    f.readBytes(buf, len);
    buf[len] = '\0';
    f.close();

    int rc = luaL_loadbuffer(L, buf, len, cur_script->name);
    heap_caps_free(buf);
    if (rc != LUA_OK) {
        show_error("syntax error", lua_tostring(L, -1));
        lua_pop(L, 1);
        return cur_scr;
    }
    lua_sethook(L, instr_hook, LUA_MASKCOUNT, INSTR_BUDGET);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        show_error("load", lua_tostring(L, -1));
        lua_pop(L, 1);
        return cur_scr;
    }
    call_global("on_create", 0);
    panic_armed = false;
    return cur_scr;
}

static void script_destroy(void)
{
    if (L) { call_global("on_exit", 0); lua_close(L); L = nullptr; }
    /* Every pin a script touched goes back to Hi-Z, same rule as the Lab. */
    for (unsigned i = 0; i < sizeof(SAFE_PINS); i++) pinMode(SAFE_PINS[i], INPUT);
    cur_scr = nullptr; cur_script = nullptr;
    object_n = 0;
}

static void script_tick(void) { if (L) call_global("on_tick", 0); }

static bool script_back(void)
{
    if (!L) return false;
    if (!call_global("on_back", 1)) return false;
    bool consumed = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return consumed;
}

/* ------------------------------------------------------------- registry -- */

static bool name_ok(const char *n)
{
    size_t len = strlen(n);
    if (len == 0 || len > NAME_MAX) return false;
    for (const char *p = n; *p; p++) {
        bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == ' ';
        if (!ok) return false;      /* no '/', no '.', no escaping /apps/ */
    }
    return true;
}

int script_rescan(void)
{
    memset(scripts, 0, sizeof(scripts));
    script_n = 0;
    if (!fs_ok) return 0;

    File dir = LittleFS.open(APPS_DIR);
    if (!dir || !dir.isDirectory()) return 0;

    for (File f = dir.openNextFile(); f && script_n < MAX_SCRIPTS; f = dir.openNextFile()) {
        const char *fn = strrchr(f.name(), '/');
        fn = fn ? fn + 1 : f.name();
        const char *dot = strrchr(fn, '.');
        if (!dot || strcmp(dot, ".lua") != 0) { f.close(); continue; }

        ScriptApp &s = scripts[script_n];
        size_t stem = (size_t)(dot - fn);
        if (stem > NAME_MAX) stem = NAME_MAX;
        memcpy(s.name, fn, stem);
        s.name[stem] = '\0';
        s.used = true;
        s.app  = { s.name, nullptr, script_create, script_destroy,
                   script_tick, script_back };
        script_n++;
        f.close();
    }
    dir.close();
    Serial.printf("[lua] %d script app(s)\n", script_n);
    return script_n;
}

void script_begin(void)
{
    fs_ok = LittleFS.begin(true);        /* format on first boot */
    if (!fs_ok) { Serial.println("[lua] LittleFS mount failed"); return; }
    if (!LittleFS.exists(APPS_DIR)) LittleFS.mkdir(APPS_DIR);
    Serial.printf("[lua] LittleFS %u/%u bytes used\n",
                  (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    script_rescan();
}

void script_mark_dirty(void)      { rescan_pending = true; }
bool script_pending(void)         { return rescan_pending; }

void script_rescan_if_pending(void)
{
    if (!rescan_pending) return;
    rescan_pending = false;
    script_rescan();
}

int         script_count(void)     { return script_n; }
const App  *script_at(int i)       { return (i >= 0 && i < script_n) ? &scripts[i].app : nullptr; }

bool script_save(const char *name, const uint8_t *body, size_t len, char *err, size_t errcap)
{
    if (!fs_ok)            { snprintf(err, errcap, "filesystem not mounted"); return false; }
    if (!name_ok(name))    { snprintf(err, errcap, "bad name: letters, digits, space, - and _ only"); return false; }
    if (len == 0)          { snprintf(err, errcap, "empty body"); return false; }
    if (len > SCRIPT_MAX)  { snprintf(err, errcap, "too large (max %d bytes)", SCRIPT_MAX); return false; }
    if (script_n >= MAX_SCRIPTS && !LittleFS.exists(APPS_DIR)) {
        snprintf(err, errcap, "too many scripts (max %d)", MAX_SCRIPTS); return false;
    }

    /*
     * Compile before storing. A script that cannot parse is rejected at upload
     * with the parser's own message, rather than becoming a tile that shows an
     * error when tapped.
     */
    lua_State *T = lua_newstate(lua_alloc, nullptr);
    if (!T) { snprintf(err, errcap, "out of PSRAM"); return false; }
    int rc = luaL_loadbuffer(T, (const char *)body, len, name);
    if (rc != LUA_OK) {
        snprintf(err, errcap, "%s", lua_tostring(T, -1));
        lua_close(T);
        return false;
    }
    lua_close(T);

    char path[48];
    snprintf(path, sizeof(path), APPS_DIR "/%s.lua", name);
    File f = LittleFS.open(path, "w");
    if (!f) { snprintf(err, errcap, "cannot open %s for writing", path); return false; }
    size_t w = f.write(body, len);
    f.close();
    if (w != len) { snprintf(err, errcap, "short write (%u of %u)", (unsigned)w, (unsigned)len); return false; }

    script_mark_dirty();
    return true;
}

bool script_delete(const char *name)
{
    if (!fs_ok || !name_ok(name)) return false;
    char path[48];
    snprintf(path, sizeof(path), APPS_DIR "/%s.lua", name);
    if (!LittleFS.exists(path)) return false;
    bool ok = LittleFS.remove(path);
    script_mark_dirty();
    return ok;
}

void script_list_json(char *out, size_t cap)
{
    size_t o = snprintf(out, cap,
                        "{\"ok\":true,\"fs\":%s,\"pending\":%s,\"used\":%u,\"total\":%u,\"apps\":[",
                        fs_ok ? "true" : "false",
                        rescan_pending ? "true" : "false",
                        fs_ok ? (unsigned)LittleFS.usedBytes() : 0u,
                        fs_ok ? (unsigned)LittleFS.totalBytes() : 0u);
    for (int i = 0; i < script_n && o + 40 < cap; i++)
        o += snprintf(out + o, cap - o, i ? ",\"%s\"" : "\"%s\"", scripts[i].name);
    snprintf(out + o, cap - o, "]}");
}
