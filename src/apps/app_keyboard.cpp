/**
 * app_keyboard.cpp — the clock types on the Mac. See D052.
 *
 * The LVGL keyboard widget is the UI; every key press translates to a HID
 * usage code and goes out as a real keystroke on the keyboard report both
 * identities now carry. There is no textarea and nothing is stored — keys
 * go to the HOST, not to a field on this screen; a dim tail of the last few
 * characters is shown purely as feedback (passwords: it is on screen, so
 * type those with the pad angled away, same as any keyboard in public).
 */
#include "app_api.h"
#include "app_host.h"
#include "ble.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define COL_BG    lv_color_hex(0x000000)
#define COL_DIM   lv_color_hex(0x8A8A8A)
#define COL_ERR   lv_color_hex(0xE0483B)

static lv_obj_t *scr, *kb, *lbl_state, *lbl_tail;
static char      tail[25];

/* ASCII -> {modifier, usage}. US layout, the same table every firmware
 * keyboard ships. 0 usage = unmapped. */
static bool ascii_to_hid(char c, uint8_t *mod, uint8_t *code)
{
    *mod = 0; *code = 0;
    if (c >= 'a' && c <= 'z') { *code = 0x04 + (c - 'a'); return true; }
    if (c >= 'A' && c <= 'Z') { *mod = 0x02; *code = 0x04 + (c - 'A'); return true; }
    if (c >= '1' && c <= '9') { *code = 0x1E + (c - '1'); return true; }
    if (c == '0') { *code = 0x27; return true; }
    switch (c) {
    case ' ':  *code = 0x2C; return true;
    case '\n': *code = 0x28; return true;
    case '\t': *code = 0x2B; return true;
    case '-':  *code = 0x2D; return true;   case '_': *mod = 0x02; *code = 0x2D; return true;
    case '=':  *code = 0x2E; return true;   case '+': *mod = 0x02; *code = 0x2E; return true;
    case '[':  *code = 0x2F; return true;   case '{': *mod = 0x02; *code = 0x2F; return true;
    case ']':  *code = 0x30; return true;   case '}': *mod = 0x02; *code = 0x30; return true;
    case '\\': *code = 0x31; return true;   case '|': *mod = 0x02; *code = 0x31; return true;
    case ';':  *code = 0x33; return true;   case ':': *mod = 0x02; *code = 0x33; return true;
    case '\'': *code = 0x34; return true;   case '"': *mod = 0x02; *code = 0x34; return true;
    case '`':  *code = 0x35; return true;   case '~': *mod = 0x02; *code = 0x35; return true;
    case ',':  *code = 0x36; return true;   case '<': *mod = 0x02; *code = 0x36; return true;
    case '.':  *code = 0x37; return true;   case '>': *mod = 0x02; *code = 0x37; return true;
    case '/':  *code = 0x38; return true;   case '?': *mod = 0x02; *code = 0x38; return true;
    case '!': *mod = 0x02; *code = 0x1E; return true;
    case '@': *mod = 0x02; *code = 0x1F; return true;
    case '#': *mod = 0x02; *code = 0x20; return true;
    case '$': *mod = 0x02; *code = 0x21; return true;
    case '%': *mod = 0x02; *code = 0x22; return true;
    case '^': *mod = 0x02; *code = 0x23; return true;
    case '&': *mod = 0x02; *code = 0x24; return true;
    case '*': *mod = 0x02; *code = 0x25; return true;
    case '(': *mod = 0x02; *code = 0x26; return true;
    case ')': *mod = 0x02; *code = 0x27; return true;
    }
    return false;
}

static void tail_push(const char *s)
{
    char shown = s[1] ? '*' : s[0];      /* multi-char = special key marker */
    size_t n = strlen(tail);
    if (n >= sizeof(tail) - 1) { memmove(tail, tail + 1, n); n--; }
    tail[n] = (shown == '*') ? ' ' : shown;
    tail[n + 1] = '\0';
    lv_label_set_text(lbl_tail, tail);
}

static void kb_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const uint16_t id = lv_btnmatrix_get_selected_btn(obj);
    if (id == LV_BTNMATRIX_BTN_NONE) return;
    const char *txt = lv_btnmatrix_get_btn_text(obj, id);
    if (!txt) return;

    bool sent = false;
    if      (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) sent = ble_key(0, 0x2A);
    else if (strcmp(txt, LV_SYMBOL_NEW_LINE)  == 0) sent = ble_key(0, 0x28);
    else if (strcmp(txt, LV_SYMBOL_OK)        == 0) sent = ble_key(0, 0x28);
    else if (strcmp(txt, LV_SYMBOL_LEFT)      == 0) sent = ble_key(0, 0x50);
    else if (strcmp(txt, LV_SYMBOL_RIGHT)     == 0) sent = ble_key(0, 0x4F);
    else if (strcmp(txt, LV_SYMBOL_UP)        == 0) sent = ble_key(0, 0x52);
    else if (strcmp(txt, LV_SYMBOL_DOWN)      == 0) sent = ble_key(0, 0x51);
    else if (strcmp(txt, LV_SYMBOL_CLOSE) == 0 || strcmp(txt, LV_SYMBOL_KEYBOARD) == 0 ||
             strcmp(txt, "ABC") == 0 || strcmp(txt, "abc") == 0 ||
             strcmp(txt, "1#") == 0) return;      /* mode keys, local only */
    else if (txt[0] && !txt[1]) {
        uint8_t mod, code;
        if (ascii_to_hid(txt[0], &mod, &code)) sent = ble_key(mod, code);
    }
    if (sent) tail_push(txt);
    else if (!ble_is_connected())
        lv_label_set_text(lbl_state, "no host connected - pair in Bluetooth settings");
}

static void kbd_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_TRANSP;
    r.border_color = lv_color_hex(0xE8E8E8);
    r.border_width = 5;
    r.radius = 8;
    lv_area_t a = { (lv_coord_t)(cx - 36), (lv_coord_t)(cy - 22),
                    (lv_coord_t)(cx + 36), (lv_coord_t)(cy + 22) };
    lv_draw_rect(ctx, &r, &a);
    /* key dots */
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(0xE8E8E8);
    d.bg_opa = LV_OPA_COVER;
    d.radius = 2;
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 5; col++) {
            lv_area_t k = { (lv_coord_t)(cx - 26 + col * 12), (lv_coord_t)(cy - 12 + row * 12),
                            (lv_coord_t)(cx - 20 + col * 12), (lv_coord_t)(cy - 6 + row * 12) };
            lv_draw_rect(ctx, &d, &k);
        }
    lv_area_t sp = { (lv_coord_t)(cx - 20), (lv_coord_t)(cy + 8),
                     (lv_coord_t)(cx + 20), (lv_coord_t)(cy + 14) };
    lv_draw_rect(ctx, &d, &sp);
}

static lv_obj_t *kbd_create(void)
{
    const int H = lv_disp_get_ver_res(nullptr);

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lbl_state = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_state,
        ble_is_connected() ? COL_DIM : COL_ERR, LV_PART_MAIN);
    lv_label_set_text(lbl_state, ble_is_connected()
        ? "typing on the connected computer"
        : "no host connected - pair in Bluetooth settings");
    lv_obj_set_pos(lbl_state, 14, 10);

    tail[0] = '\0';
    lbl_tail = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_tail, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_tail, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(lbl_tail, "");
    lv_obj_set_pos(lbl_tail, 14, 36);

    kb = lv_keyboard_create(scr);
    lv_obj_set_size(kb, LV_PCT(100), H - 68 - 76);
    lv_obj_align(kb, LV_ALIGN_TOP_MID, 0, 68);
    lv_keyboard_set_textarea(kb, nullptr);
    lv_obj_add_event_cb(kb, kb_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    app_host_std_back(scr, nullptr);
    return scr;
}

static void kbd_destroy(void)
{
    scr = kb = lbl_state = lbl_tail = nullptr;
}

static void kbd_tick(void)
{
    if (!scr) return;
    static uint32_t last = 0;
    if (millis() - last > 1500) {
        last = millis();
        lv_obj_set_style_text_color(lbl_state,
            ble_is_connected() ? COL_DIM : COL_ERR, LV_PART_MAIN);
        if (ble_is_connected())
            lv_label_set_text(lbl_state, "typing on the connected computer");
    }
}

extern const App app_keyboard = { "Keyboard", kbd_icon, kbd_create, kbd_destroy,
                                  kbd_tick, nullptr, /*portrait_ok=*/true };
