/**
 * app_lab.cpp — the device as a test instrument: GPIO, I2C scan, UART monitor.
 *
 * THE WHITELIST IS THE DESIGN (D035). Every pin this app offers was
 * cross-checked against the T4-S3 schematic (what reaches the 2x15 header)
 * and the library board config (what the firmware owns). Nothing else is
 * touchable: a wrong write to a QSPI pin corrupts the panel, to GPIO9 kills
 * the PMIC enable, to 33-37 crashes OPI PSRAM. GPIO18 is ON the header but
 * belongs to the display (TE), so it is not here.
 *
 * Internal I2C (6/7, PMU + touch + the P4 plug) is scannable — address
 * probes are reads — but never offered as GPIO.
 *
 * Leave no trace: destroy() returns every pin to Hi-Z input, closes the UART
 * and releases the external I2C controller.
 */
#include "app_api.h"
#include "app_host.h"

#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

#define COL_BG     lv_color_hex(0x000000)
#define COL_CARD   lv_color_hex(0x161616)
#define COL_TEXT   lv_color_hex(0xE8E8E8)
#define COL_DIM    lv_color_hex(0x8A8A8A)
#define COL_HI     lv_color_hex(0x2FBF71)
#define COL_LO     lv_color_hex(0x3A3A3A)

struct LabPin { uint8_t gpio; const char *note; bool in_only; };

/* Header pins the firmware does not own. 1-4 double as the SD socket —
 * usable while no card is inserted, and labelled so.
 *
 * GPIO0 is INPUT ONLY, and that is a hardware fact rather than caution:
 * the BOOT button is hard-wired from this pin to ground, so an output
 * driving high is one button press away from shorting the pad to GND
 * through nothing but the button. It is also the flash rescue path, which
 * must survive an OTA image that crash-loops. Reading it is free and useful
 * — that is the "extra button" the pin can honestly provide. See D036. */
static const LabPin PINS[] = {
    {  0, "BOOT (in only)", true },
    { 21, ""       }, { 38, ""       }, { 39, ""       }, { 40, ""       },
    { 41, ""       }, { 42, ""       }, { 47, ""       }, { 48, ""       },
    { 43, "TX0"    }, { 44, "RX0"    },
    {  1, "SD CS"  }, {  2, "SD MOSI"}, {  3, "SD SCK" }, {  4, "SD MISO"},
};
#define NPINS ((int)(sizeof(PINS) / sizeof(PINS[0])))

enum PinMode8 { PM_HIZ = 0, PM_IN_PU, PM_IN_PD, PM_OUT };

static lv_obj_t *scr;
static lv_obj_t *rows_gpio[NPINS];       /* state buttons, indexed like PINS */
static lv_obj_t *row_lbls[NPINS];
static uint8_t   pin_mode8[NPINS];
static uint8_t   pin_out[NPINS];

static lv_obj_t *dd_bus, *dd_sda, *dd_scl, *dd_freq, *lbl_i2c;
static lv_obj_t *dd_tx, *dd_rx, *dd_baud, *btn_uart_lbl, *lbl_mon, *mon_box;
static bool      uart_open;
static char      mon[513];
static size_t    mon_len;
static uint32_t  last_poll_ms;

static void decor(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLL_CHAIN);
}

/*
 * Dropdown position -> PINS index, for the I2C and UART tabs.
 *
 * An input-only pin cannot drive SCL or a TX line, so it is simply absent
 * from those lists. Without this map its presence in PINS[] would silently
 * shift every selection index by one — the kind of off-by-one that produces
 * a scan on the wrong pins and no error at all.
 */
static uint8_t bus_map[NPINS];
static int     bus_n;

static void build_bus_map(void)
{
    bus_n = 0;
    for (int i = 0; i < NPINS; i++)
        if (!PINS[i].in_only) bus_map[bus_n++] = (uint8_t)i;
}

static uint8_t bus_gpio(lv_obj_t *dd)
{
    int sel = (int)lv_dropdown_get_selected(dd);
    if (sel < 0 || sel >= bus_n) sel = 0;
    return PINS[bus_map[sel]].gpio;
}

/* Build "21\n38\n39..." once for the pin dropdowns. */
static const char *pin_options(void)
{
    static char opts[NPINS * 4 + 1];
    if (!opts[0]) {
        char *p = opts;
        for (int i = 0; i < bus_n; i++)
            p += sprintf(p, i ? "\n%u" : "%u", PINS[bus_map[i]].gpio);
    }
    return opts;
}

/* ----------------------------------------------------------------- GPIO -- */

static void apply_mode(int i)
{
    switch (pin_mode8[i]) {
    case PM_HIZ:   pinMode(PINS[i].gpio, INPUT);          break;
    case PM_IN_PU: pinMode(PINS[i].gpio, INPUT_PULLUP);   break;
    case PM_IN_PD: pinMode(PINS[i].gpio, INPUT_PULLDOWN); break;
    case PM_OUT:
        /* Unreachable from the UI — the dropdown omits Out for these — but
         * the pad short is bad enough to check twice. */
        if (PINS[i].in_only) { pinMode(PINS[i].gpio, INPUT); break; }
        pinMode(PINS[i].gpio, OUTPUT);
        digitalWrite(PINS[i].gpio, pin_out[i]);
        break;
    }
}

static void paint_state(int i)
{
    if (!rows_gpio[i]) return;
    bool level = (pin_mode8[i] == PM_OUT) ? pin_out[i]
                                          : digitalRead(PINS[i].gpio);
    lv_obj_t *lbl = lv_obj_get_child(rows_gpio[i], 0);
    lv_label_set_text(lbl, level ? "H" : "L");
    lv_obj_set_style_bg_color(rows_gpio[i], level ? COL_HI : COL_LO, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, level ? lv_color_hex(0x000000) : COL_TEXT,
                                LV_PART_MAIN);
}

static void mode_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    pin_mode8[i] = (uint8_t)lv_dropdown_get_selected(lv_event_get_target(e));
    apply_mode(i);
    paint_state(i);
}

static void state_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (pin_mode8[i] != PM_OUT) return;      /* inputs: display only */
    pin_out[i] ^= 1;
    digitalWrite(PINS[i].gpio, pin_out[i]);
    paint_state(i);
}

static void build_gpio_tab(lv_obj_t *tab)
{
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tab, 10, LV_PART_MAIN);

    for (int i = 0; i < NPINS; i++) {
        lv_obj_t *row = lv_obj_create(tab);
        decor(row);
        lv_obj_set_size(row, LV_PCT(100), 52);

        char name[24];
        snprintf(name, sizeof(name), PINS[i].note[0] ? "IO%u  ·  %s" : "IO%u",
                 PINS[i].gpio, PINS[i].note);
        row_lbls[i] = lv_label_create(row);
        lv_obj_set_style_text_font(row_lbls[i], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(row_lbls[i], COL_TEXT, LV_PART_MAIN);
        lv_label_set_text(row_lbls[i], name);
        lv_obj_align(row_lbls[i], LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t *dd = lv_dropdown_create(row);
        lv_dropdown_set_options_static(dd, PINS[i].in_only ? "Hi-Z\nIn PU\nIn PD"
                                                           : "Hi-Z\nIn PU\nIn PD\nOut");
        lv_obj_set_style_text_font(dd, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_font(lv_dropdown_get_list(dd),
                                   &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_size(dd, 150, 44);
        lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -110, 0);
        lv_dropdown_set_selected(dd, pin_mode8[i]);
        lv_obj_add_event_cb(dd, mode_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);

        lv_obj_t *b = lv_btn_create(row);
        lv_obj_set_size(b, 84, 44);
        lv_obj_align(b, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_event_cb(b, state_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *bl = lv_label_create(b);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(bl);
        rows_gpio[i] = b;
        paint_state(i);
    }
}

/* ------------------------------------------------------------------ I2C -- */

static void scan_cb(lv_event_t *)
{
    const bool internal = lv_dropdown_get_selected(dd_bus) == 1;
    uint8_t sda = bus_gpio(dd_sda);
    uint8_t scl = bus_gpio(dd_scl);
    uint32_t hz = lv_dropdown_get_selected(dd_freq) == 0 ? 100000 : 400000;

    if (!internal && sda == scl) {
        lv_label_set_text(lbl_i2c, "SDA and SCL are the same pin.");
        return;
    }

    TwoWire *w = &Wire;               /* internal 6/7: already running */
    if (!internal) {
        Wire1.end();
        if (!Wire1.begin((int)sda, (int)scl, hz)) {
            lv_label_set_text(lbl_i2c, "Wire1.begin failed.");
            return;
        }
        w = &Wire1;
    }

    char out[256];
    int  n = 0, o = 0;
    o += snprintf(out + o, sizeof(out) - o, internal
                  ? "internal 6/7 @ 100k:\n" : "IO%u/IO%u @ %uk:\n",
                  internal ? 6 : sda, internal ? 7 : scl, (unsigned)(hz / 1000));
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        w->beginTransmission(a);
        if (w->endTransmission() == 0) {
            n++;
            if (o < (int)sizeof(out) - 8)
                o += snprintf(out + o, sizeof(out) - o, "0x%02X  ", a);
        }
    }
    snprintf(out + o, sizeof(out) - o, n ? "\n%d device%s" : "no devices",
             n, n == 1 ? "" : "s");
    lv_label_set_text(lbl_i2c, out);

    if (!internal) Wire1.end();       /* leave the pins free again */
}

static void mk_dd(lv_obj_t *parent, lv_obj_t **out, const char *opts,
                  int x, int y, int w, uint16_t sel)
{
    lv_obj_t *dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, opts);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd),
                               &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_size(dd, w, 44);
    lv_obj_set_pos(dd, x, y);
    lv_dropdown_set_selected(dd, sel);
    *out = dd;
}

static void mk_lbl(lv_obj_t *parent, const char *txt, int x, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(l, txt);
    lv_obj_set_pos(l, x, y + 12);
}

static void build_i2c_tab(lv_obj_t *tab)
{
    lv_obj_set_style_pad_all(tab, 10, LV_PART_MAIN);

    mk_lbl(tab, "bus",  0,   0);
    mk_dd(tab, &dd_bus, "external\ninternal 6/7", 46, 0, 170, 0);
    mk_lbl(tab, "kHz",  230, 0);
    mk_dd(tab, &dd_freq, "100\n400", 276, 0, 100, 0);

    mk_lbl(tab, "SDA", 0, 56);
    mk_dd(tab, &dd_sda, pin_options(), 46, 56, 100, 6);   /* IO47 */
    mk_lbl(tab, "SCL", 160, 56);
    mk_dd(tab, &dd_scl, pin_options(), 206, 56, 100, 7);  /* IO48 */

    lv_obj_t *b = lv_btn_create(tab);
    lv_obj_set_size(b, 140, 44);
    lv_obj_set_pos(b, 396, 56);
    lv_obj_add_event_cb(b, scan_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(b);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(bl, LV_SYMBOL_REFRESH "  Scan");
    lv_obj_center(bl);

    lbl_i2c = lv_label_create(tab);
    lv_obj_set_style_text_font(lbl_i2c, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_i2c, COL_TEXT, LV_PART_MAIN);
    lv_obj_set_width(lbl_i2c, LV_PCT(100));
    lv_label_set_long_mode(lbl_i2c, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_i2c, "Pick pins, tap Scan.\nAddress probe only - safe on a live bus.");
    lv_obj_set_pos(lbl_i2c, 0, 120);
}

/* ----------------------------------------------------------------- UART -- */

static void mon_append(const char *s, size_t len)
{
    for (size_t k = 0; k < len; k++) {
        char c = s[k];
        /* Label text is UTF-8; a bare high byte would corrupt the whole
         * string's decoding. Plain ASCII stand-in for anything unprintable. */
        if (c != '\n' && c != '\r' && (c < 0x20 || c > 0x7E)) c = '.';
        if (c == '\r') continue;
        if (mon_len >= sizeof(mon) - 1) {            /* keep the tail */
            memmove(mon, mon + 128, mon_len - 128);
            mon_len -= 128;
        }
        mon[mon_len++] = c;
    }
    mon[mon_len] = '\0';
    lv_label_set_text(lbl_mon, mon);
    lv_obj_scroll_to_y(mon_box, LV_COORD_MAX, LV_ANIM_OFF);
}

static void uart_toggle_cb(lv_event_t *)
{
    if (uart_open) {
        Serial1.end();
        uart_open = false;
        lv_label_set_text(btn_uart_lbl, LV_SYMBOL_PLAY "  Open");
        return;
    }
    static const uint32_t BAUD[] = { 9600, 19200, 38400, 57600, 115200, 230400 };
    uint8_t tx = bus_gpio(dd_tx);
    uint8_t rx = bus_gpio(dd_rx);
    if (tx == rx) { mon_append("[tx = rx, pick two pins]\n", 25); return; }
    Serial1.begin(BAUD[lv_dropdown_get_selected(dd_baud)], SERIAL_8N1, rx, tx);
    uart_open = true;
    lv_label_set_text(btn_uart_lbl, LV_SYMBOL_STOP "  Close");
}

static void send_cb(lv_event_t *e)
{
    if (!uart_open) return;
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    switch (which) {
    case 0: Serial1.print("AT\r\n");            mon_append("> AT\n", 5);    break;
    case 1: Serial1.print("hello\r\n");         mon_append("> hello\n", 8); break;
    case 2: for (int k = 0; k < 8; k++) Serial1.write(0x55);
            mon_append("> 0x55 x8\n", 10);                                  break;
    }
}

static void build_uart_tab(lv_obj_t *tab)
{
    lv_obj_set_style_pad_all(tab, 10, LV_PART_MAIN);

    mk_lbl(tab, "TX", 0, 0);
    mk_dd(tab, &dd_tx, pin_options(), 36, 0, 100, 8);     /* IO43 */
    mk_lbl(tab, "RX", 150, 0);
    mk_dd(tab, &dd_rx, pin_options(), 186, 0, 100, 9);    /* IO44 */
    mk_dd(tab, &dd_baud, "9600\n19200\n38400\n57600\n115200\n230400", 300, 0, 128, 4);

    lv_obj_t *b = lv_btn_create(tab);
    lv_obj_set_size(b, 108, 44);
    lv_obj_set_pos(b, 442, 0);
    lv_obj_add_event_cb(b, uart_toggle_cb, LV_EVENT_CLICKED, nullptr);
    btn_uart_lbl = lv_label_create(b);
    lv_obj_set_style_text_font(btn_uart_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_label_set_text(btn_uart_lbl, LV_SYMBOL_PLAY "  Open");
    lv_obj_center(btn_uart_lbl);

    static const char *sends[] = { "AT", "hello", "0x55" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *sb = lv_btn_create(tab);
        lv_obj_set_size(sb, 100, 40);
        lv_obj_set_pos(sb, i * 112, 54);
        lv_obj_add_event_cb(sb, send_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *sl = lv_label_create(sb);
        lv_obj_set_style_text_font(sl, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_label_set_text(sl, sends[i]);
        lv_obj_center(sl);
    }

    mon_box = lv_obj_create(tab);
    lv_obj_remove_style_all(mon_box);
    lv_obj_set_size(mon_box, LV_PCT(100), 190);
    lv_obj_set_pos(mon_box, 0, 106);
    lv_obj_set_style_bg_color(mon_box, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mon_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(mon_box, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mon_box, 10, LV_PART_MAIN);

    lbl_mon = lv_label_create(mon_box);
    lv_obj_set_style_text_font(lbl_mon, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_mon, COL_TEXT, LV_PART_MAIN);
    lv_obj_set_width(lbl_mon, LV_PCT(100));
    lv_label_set_long_mode(lbl_mon, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_mon, "");
    mon_len = 0; mon[0] = '\0';
}

/* ------------------------------------------------------------------ app -- */

static void lab_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    /* A chip: body plus four legs a side. */
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_TRANSP;
    r.border_color = lv_color_hex(0xE8E8E8);
    r.border_width = 5;
    r.radius = 8;
    lv_area_t body = { (lv_coord_t)(cx - 22), (lv_coord_t)(cy - 22),
                       (lv_coord_t)(cx + 22), (lv_coord_t)(cy + 22) };
    lv_draw_rect(ctx, &r, &body);

    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(0xE8E8E8);
    l.width = 4; l.opa = LV_OPA_COVER;
    for (int k = 0; k < 4; k++) {
        lv_coord_t y = (lv_coord_t)(cy - 15 + k * 10);
        lv_point_t a = { (lv_coord_t)(cx - 32), y }, b2 = { (lv_coord_t)(cx - 22), y };
        lv_draw_line(ctx, &l, &a, &b2);
        lv_point_t c = { (lv_coord_t)(cx + 22), y }, d = { (lv_coord_t)(cx + 32), y };
        lv_draw_line(ctx, &l, &c, &d);
    }
}

static lv_obj_t *lab_create(void)
{
    build_bus_map();                 /* before any pin_options() call */
    memset(pin_mode8, 0, sizeof(pin_mode8));
    memset(pin_out, 0, sizeof(pin_out));
    for (int i = 0; i < NPINS; i++) apply_mode(i);   /* everything Hi-Z */
    uart_open = false;

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);

    lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_TOP, 46);
    lv_obj_set_style_bg_color(tv, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_tabview_get_tab_btns(tv),
                               &lv_font_montserrat_20, LV_PART_MAIN);

    build_gpio_tab(lv_tabview_add_tab(tv, "GPIO"));
    build_i2c_tab(lv_tabview_add_tab(tv, "I2C"));
    build_uart_tab(lv_tabview_add_tab(tv, "UART"));

    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 96, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -8, 3);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_add_event_cb(back, [](lv_event_t *) { app_host_back(); },
                        LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(back);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_label_set_text(bl, LV_SYMBOL_LEFT "  Back");
    lv_obj_center(bl);

    return scr;
}

static void lab_destroy(void)
{
    /* Leave no trace: pins to Hi-Z, peripherals released. */
    if (uart_open) { Serial1.end(); uart_open = false; }
    Wire1.end();
    for (int i = 0; i < NPINS; i++) pinMode(PINS[i].gpio, INPUT);

    scr = nullptr; lbl_i2c = nullptr; lbl_mon = nullptr; mon_box = nullptr;
    dd_bus = dd_sda = dd_scl = dd_freq = dd_tx = dd_rx = dd_baud = nullptr;
    btn_uart_lbl = nullptr;
    memset(rows_gpio, 0, sizeof(rows_gpio));
    memset(row_lbls, 0, sizeof(row_lbls));
}

static void lab_tick(void)
{
    if (!scr) return;
    uint32_t now = millis();

    if (uart_open && lbl_mon) {
        char buf[64];
        size_t n = 0;
        while (Serial1.available() && n < sizeof(buf)) buf[n++] = (char)Serial1.read();
        if (n) mon_append(buf, n);
    }

    if (now - last_poll_ms < 150) return;
    last_poll_ms = now;
    for (int i = 0; i < NPINS; i++)
        if (pin_mode8[i] != PM_OUT) paint_state(i);
}

extern const App app_lab = { "Lab", lab_icon, lab_create, lab_destroy,
                             lab_tick, nullptr };
