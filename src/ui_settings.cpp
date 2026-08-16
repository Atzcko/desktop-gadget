/**
 * ui_settings.cpp — on-device settings, opened by a 3-second hold.
 *
 * Built on its own LVGL screen so the clock screen is never disturbed and
 * comes back exactly as it was. Everything is written to NVS on close.
 */
#include "ui_settings.h"
#include "settings.h"
#include "config.h"
#include "net.h"
#include "ui.h"
#include "app.h"
#include "ble.h"

#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define MAX_SCAN 16
#define MAX_GEO  6

static lv_obj_t *scr_set;
static bool      is_open;

/* Wi-Fi tab */
static lv_obj_t *lbl_wifi_state, *list_wifi, *ta_pass, *lbl_pick, *cb_show_pass;
static lv_timer_t *wifi_poll;
static char      scan_ssid[MAX_SCAN][33];
static int       scan_rssi[MAX_SCAN];
static bool      scan_sec[MAX_SCAN];
static int       scan_n;
static char      picked_ssid[33];

/* Place tab */
static lv_obj_t *lbl_place, *ta_city, *list_geo;
static GeoResult geo[MAX_GEO];
static int       geo_n;

/* Time tab */
static lv_obj_t *roller_tz, *sw_24h;

/* Screen tab */
static lv_obj_t *sl_day, *sl_night, *lbl_day, *lbl_night;
static lv_obj_t *roller_ns, *roller_ne, *sw_wx, *sw_hum, *sw_burn;

/* BLE tab */
static lv_obj_t *sw_ble, *sw_hid, *ta_ble_name, *lbl_ble_state;

/* Info tab */
static lv_obj_t *lbl_info_body;

/* ------------------------------------------------------------- helpers -- */

static lv_obj_t *section(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0x7A7A7A), LV_PART_MAIN);
    lv_label_set_text(l, text);
    return l;
}

static lv_obj_t *body_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_label_set_text(l, text);
    return l;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text,
                             lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_height(b, 44);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
}

static void hours_options(char *buf, size_t cap)
{
    buf[0] = '\0';
    for (int h = 0; h < 24; h++) {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), h == 23 ? "%02d" : "%02d\n", h);
        strncat(buf, tmp, cap - strlen(buf) - 1);
    }
}

/* ---------------------------------------------------------- text editor -- */
/*
 * Text entry gets its own full-screen overlay instead of a keyboard that
 * slides over the tab content. Two bugs made that necessary:
 *
 *  1. The keyboard covered the bottom half of the screen, and a textarea
 *     laid out low in a tab ended up BEHIND it — you could not see what you
 *     were typing.
 *  2. It was shown on LV_EVENT_FOCUSED, which fires only on the FIRST tap.
 *     Once the field already had focus, tapping it again did nothing, so a
 *     dismissed keyboard could only be recovered by switching tabs and back.
 *
 * The overlay puts the field at the top, the keyboard at the bottom, and
 * explicit Done/Cancel buttons in between. Opening it is driven by
 * LV_EVENT_CLICKED, which fires on every tap.
 */
static lv_obj_t *editor, *editor_ta;
static lv_obj_t *edit_target;

static void editor_close(bool commit)
{
    if (!editor) return;
    if (commit && edit_target && editor_ta) {
        lv_textarea_set_text(edit_target, lv_textarea_get_text(editor_ta));
    }
    lv_obj_del(editor);
    editor = nullptr; editor_ta = nullptr; edit_target = nullptr;
}

static void editor_kb_event(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY)  editor_close(true);
    if (c == LV_EVENT_CANCEL) editor_close(false);
}

static void editor_done_cb(lv_event_t *)   { editor_close(true); }
static void editor_cancel_cb(lv_event_t *) { editor_close(false); }

static void editor_mask_cb(lv_event_t *e)
{
    bool hide = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    lv_textarea_set_password_mode(editor_ta, hide);
}

static void editor_open(lv_obj_t *target, const char *title, bool is_password)
{
    editor_close(false);
    edit_target = target;

    editor = lv_obj_create(scr_set);
    lv_obj_remove_style_all(editor);
    lv_obj_set_size(editor, 600, 450);
    lv_obj_set_pos(editor, 0, 0);
    lv_obj_set_style_bg_color(editor, lv_color_hex(0x0A0A0A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(editor, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(editor, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(editor);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x9A9A9A), LV_PART_MAIN);
    lv_label_set_text(lbl, title);
    lv_obj_set_pos(lbl, 20, 10);

    editor_ta = lv_textarea_create(editor);
    lv_textarea_set_one_line(editor_ta, true);
    lv_obj_set_style_text_font(editor_ta, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_size(editor_ta, 560, 56);
    lv_obj_set_pos(editor_ta, 20, 34);
    lv_textarea_set_text(editor_ta, lv_textarea_get_text(target));
    /* Visible by default even for a password: the entire point of this
     * overlay is that you can see what you are typing. "Hide" is there for
     * anyone who wants it. */
    lv_textarea_set_password_mode(editor_ta, false);

    if (is_password) {
        lv_obj_t *cb = lv_checkbox_create(editor);
        lv_checkbox_set_text(cb, "Hide");
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_pos(cb, 22, 106);
        lv_obj_add_event_cb(cb, editor_mask_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    }

    lv_obj_t *b_cancel = make_button(editor, LV_SYMBOL_CLOSE "  Cancel",
                                     editor_cancel_cb, nullptr);
    lv_obj_set_size(b_cancel, 150, 46);
    lv_obj_set_pos(b_cancel, 256, 100);

    lv_obj_t *b_done = make_button(editor, LV_SYMBOL_OK "  Done",
                                   editor_done_cb, nullptr);
    lv_obj_set_size(b_done, 150, 46);
    lv_obj_set_pos(b_done, 424, 100);

    /* 450 - 162 = 288 px of keyboard, with the field parked safely above. */
    lv_obj_t *kb = lv_keyboard_create(editor);
    lv_obj_set_size(kb, 600, 288);
    lv_obj_set_pos(kb, 0, 162);
    lv_keyboard_set_textarea(kb, editor_ta);
    lv_obj_add_event_cb(kb, editor_kb_event, LV_EVENT_ALL, nullptr);
}

/* CLICKED, not FOCUSED — fires on every tap, including a re-tap. */
static void ta_open_editor(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    editor_open(ta, (const char *)lv_event_get_user_data(e), ta == ta_pass);
}

/* ------------------------------------------------------------------ Wi-Fi -- */

static void wifi_pick(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= scan_n) return;
    strncpy(picked_ssid, scan_ssid[idx], sizeof(picked_ssid) - 1);
    picked_ssid[sizeof(picked_ssid) - 1] = '\0';

    char buf[64];
    snprintf(buf, sizeof(buf), "Network: %s", picked_ssid);
    lv_label_set_text(lbl_pick, buf);
    lv_textarea_set_text(ta_pass, "");
}

static void wifi_scan_cb(lv_event_t *e)
{
    lv_label_set_text(lbl_wifi_state, "Scanning...");
    lv_refr_now(nullptr);          /* paint the message before we block */

    scan_n = net_scan(scan_ssid, scan_rssi, scan_sec, MAX_SCAN);
    lv_obj_clean(list_wifi);

    for (int i = 0; i < scan_n; i++) {
        char row[64];
        snprintf(row, sizeof(row), "%s   %d dBm%s",
                 scan_ssid[i], scan_rssi[i], scan_sec[i] ? "" : "  (open)");
        lv_obj_t *b = lv_list_add_btn(list_wifi, LV_SYMBOL_WIFI, row);
        lv_obj_set_style_text_font(b, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_add_event_cb(b, wifi_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "%d network%s found", scan_n, scan_n == 1 ? "" : "s");
    lv_label_set_text(lbl_wifi_state, buf);
}

static void show_pass_cb(lv_event_t *e)
{
    bool show = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    lv_textarea_set_password_mode(ta_pass, !show);
}

/*
 * Without this the screen said "Connecting..." forever and a mistyped
 * password was indistinguishable from a slow router. Poll the real state
 * and report the 802.11 reason code in plain words.
 */
static void wifi_poll_cb(lv_timer_t *)
{
    if (!lbl_wifi_state) return;
    NetStatus st = net_status();
    char buf[112];
    if (st.wifi_up) {
        snprintf(buf, sizeof(buf), "Connected to %s   %s   %d dBm",
                 settings_get().wifi_ssid, st.ip, st.rssi);
    } else {
        uint8_t r = net_last_disconnect();
        if (r) snprintf(buf, sizeof(buf), "Failed: %s", net_disconnect_text(r));
        else   snprintf(buf, sizeof(buf), "Not connected");
    }
    lv_label_set_text(lbl_wifi_state, buf);
}

static void wifi_connect_cb(lv_event_t *e)
{
    if (!picked_ssid[0]) {
        lv_label_set_text(lbl_wifi_state, "Pick a network first");
        return;
    }
    Settings &s = settings_get();
    strncpy(s.wifi_ssid, picked_ssid, sizeof(s.wifi_ssid) - 1);
    strncpy(s.wifi_pass, lv_textarea_get_text(ta_pass), sizeof(s.wifi_pass) - 1);
    s.wifi_ssid[sizeof(s.wifi_ssid) - 1] = '\0';
    s.wifi_pass[sizeof(s.wifi_pass) - 1] = '\0';
    settings_save();
    net_apply_wifi(s.wifi_ssid, s.wifi_pass);
    lv_label_set_text(lbl_wifi_state, "Connecting...");
}

/* ------------------------------------------------------------------ Place -- */

static void clear_bonds_cb(lv_event_t *)
{
    ble_clear_bonds();
    lv_label_set_text(lbl_ble_state,
                      "Pairings cleared. Also remove it on the Mac:\n"
                      "System Settings > Bluetooth > (i) > Forget.");
}

static void geo_pick(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= geo_n) return;

    Settings &s = settings_get();
    strncpy(s.city, geo[idx].name, sizeof(s.city) - 1);
    s.city[sizeof(s.city) - 1] = '\0';
    s.latitude  = geo[idx].latitude;
    s.longitude = geo[idx].longitude;
    settings_save();
    net_request_weather_now();

    char buf[96];
    snprintf(buf, sizeof(buf), "%s  (%.3f, %.3f)", s.city, s.latitude, s.longitude);
    lv_label_set_text(lbl_place, buf);
}

static void geo_search_cb(lv_event_t *e)
{
    const char *q = lv_textarea_get_text(ta_city);
    if (!q || !q[0]) return;

    lv_obj_clean(list_geo);
    lv_refr_now(nullptr);

    geo_n = net_geocode(q, geo, MAX_GEO);
    if (geo_n == 0) {
        lv_obj_t *b = lv_list_add_btn(list_geo, nullptr, "No match (Wi-Fi up?)");
        lv_obj_set_style_text_font(b, &lv_font_montserrat_16, LV_PART_MAIN);
        return;
    }
    for (int i = 0; i < geo_n; i++) {
        char row[96];
        snprintf(row, sizeof(row), "%s, %s", geo[i].name, geo[i].country);
        lv_obj_t *b = lv_list_add_btn(list_geo, LV_SYMBOL_GPS, row);
        lv_obj_set_style_text_font(b, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_add_event_cb(b, geo_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* ------------------------------------------------------------------- Time -- */

static void sync_now_cb(lv_event_t *e)
{
    Settings &s = settings_get();
    int idx = lv_roller_get_selected(roller_tz);
    if (idx >= 0 && idx < TIMEZONE_COUNT) {
        strncpy(s.tz_posix, TIMEZONES[idx].posix, sizeof(s.tz_posix) - 1);
        s.tz_posix[sizeof(s.tz_posix) - 1] = '\0';
    }
    settings_save();
    net_apply_timezone(s.tz_posix);
    app_refresh_clock(false);
}

/* ----------------------------------------------------------------- Screen -- */

static void slider_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int v = lv_slider_get_value(sl);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", v);

    Settings &s = settings_get();
    if (sl == sl_day) {
        s.brightness_day = v;
        lv_label_set_text(lbl_day, buf);
        app_apply_brightness(v);          /* live preview */
    } else {
        s.brightness_night = v;
        lv_label_set_text(lbl_night, buf);
    }
}

static void reset_cb(lv_event_t *e)
{
    settings_reset();
    app_apply_brightness(settings_get().brightness_day);
    lv_label_set_text(lbl_info_body, "Defaults restored. Close to apply.");
}

/* ------------------------------------------------------------------ close -- */

static void close_cb(lv_event_t *e)
{
    Settings &s = settings_get();

    int idx = lv_roller_get_selected(roller_tz);
    if (idx >= 0 && idx < TIMEZONE_COUNT) {
        strncpy(s.tz_posix, TIMEZONES[idx].posix, sizeof(s.tz_posix) - 1);
        s.tz_posix[sizeof(s.tz_posix) - 1] = '\0';
    }
    s.use_24h          = lv_obj_has_state(sw_24h,  LV_STATE_CHECKED);
    s.show_weather     = lv_obj_has_state(sw_wx,   LV_STATE_CHECKED);
    s.show_humidity    = lv_obj_has_state(sw_hum,  LV_STATE_CHECKED);
    s.burnin_guard     = lv_obj_has_state(sw_burn, LV_STATE_CHECKED);
    s.brightness_day   = lv_slider_get_value(sl_day);
    s.brightness_night = lv_slider_get_value(sl_night);
    s.night_start_hour = lv_roller_get_selected(roller_ns);
    s.night_end_hour   = lv_roller_get_selected(roller_ne);

    /* Detect a BLE change before saving so we know whether to bounce the
     * stack — restarting NimBLE unnecessarily drops a connected client. */
    bool ble_was_on = s.ble_enabled;
    char old_name[sizeof(s.ble_name)];
    strncpy(old_name, s.ble_name, sizeof(old_name));

    bool hid_was = s.ble_hid;
    s.ble_hid     = lv_obj_has_state(sw_hid, LV_STATE_CHECKED);
    s.ble_enabled = lv_obj_has_state(sw_ble, LV_STATE_CHECKED);
    strncpy(s.ble_name, lv_textarea_get_text(ta_ble_name), sizeof(s.ble_name) - 1);
    s.ble_name[sizeof(s.ble_name) - 1] = '\0';
    if (!s.ble_name[0]) strncpy(s.ble_name, DEFAULT_BLE_NAME, sizeof(s.ble_name) - 1);

    settings_save();
    net_apply_timezone(s.tz_posix);

    if (!s.ble_enabled && ble_was_on)                     ble_stop();
    else if (s.ble_enabled && !ble_was_on)                ble_begin();
    else if (s.ble_enabled && (strcmp(old_name, s.ble_name) != 0 || hid_was != s.ble_hid))
        ble_apply_name(s.ble_name);   /* bounces the stack; picks up both */

    ui_show_weather_block(s.show_weather);
    ui_show_humidity(s.show_humidity);
    app_refresh_clock(false);
    app_apply_brightness(s.brightness_day);

    editor_close(false);
    if (wifi_poll) { lv_timer_del(wifi_poll); wifi_poll = nullptr; }
    lbl_wifi_state = nullptr;

    lv_obj_t *dead = scr_set;
    scr_set = nullptr;
    is_open = false;
    lv_scr_load(ui_screen());
    lv_obj_del(dead);
}

/* ------------------------------------------------------------------- open -- */

bool ui_settings_is_open(void) { return is_open; }

void ui_settings_open(void)
{
    if (is_open) return;
    is_open = true;

    Settings &s = settings_get();

    scr_set = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_set, lv_color_hex(0x0A0A0A), LV_PART_MAIN);

    lv_obj_t *tv = lv_tabview_create(scr_set, LV_DIR_TOP, 46);
    lv_obj_set_size(tv, 600, 396);
    lv_obj_set_pos(tv, 0, 0);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x0A0A0A), LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_tabview_get_tab_btns(tv),
                               &lv_font_montserrat_18, LV_PART_MAIN);

    lv_obj_t *t_wifi   = lv_tabview_add_tab(tv, "Wi-Fi");
    lv_obj_t *t_time   = lv_tabview_add_tab(tv, "Time");
    lv_obj_t *t_place  = lv_tabview_add_tab(tv, "Place");
    lv_obj_t *t_screen = lv_tabview_add_tab(tv, "Screen");
    lv_obj_t *t_ble    = lv_tabview_add_tab(tv, "BLE");
    lv_obj_t *t_info   = lv_tabview_add_tab(tv, "Info");

    /* ---- Wi-Fi ---- */
    lv_obj_set_flex_flow(t_wifi, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t_wifi, 8, LV_PART_MAIN);
    {
        NetStatus st = net_status();
        char buf[80];
        if (st.wifi_up) snprintf(buf, sizeof(buf), "Connected to %s   %s   %d dBm",
                                 s.wifi_ssid, st.ip, st.rssi);
        else            snprintf(buf, sizeof(buf), "Not connected");
        lbl_wifi_state = body_label(t_wifi, buf);
    }
    lbl_pick = section(t_wifi, picked_ssid[0] ? picked_ssid : "No network picked");
    make_button(t_wifi, LV_SYMBOL_REFRESH "  Scan", wifi_scan_cb, nullptr);

    list_wifi = lv_list_create(t_wifi);
    lv_obj_set_size(list_wifi, 560, 140);
    lv_obj_set_style_bg_color(list_wifi, lv_color_hex(0x141414), LV_PART_MAIN);

    ta_pass = lv_textarea_create(t_wifi);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_password_mode(ta_pass, true);
    lv_textarea_set_placeholder_text(ta_pass, "Wi-Fi password");
    lv_obj_set_width(ta_pass, 560);
    lv_obj_add_event_cb(ta_pass, ta_open_editor, LV_EVENT_CLICKED, (void *)"Wi-Fi password");

    cb_show_pass = lv_checkbox_create(t_wifi);
    lv_checkbox_set_text(cb_show_pass, "Show password");
    lv_obj_set_style_text_font(cb_show_pass, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_add_event_cb(cb_show_pass, show_pass_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    make_button(t_wifi, LV_SYMBOL_OK "  Connect", wifi_connect_cb, nullptr);

    wifi_poll = lv_timer_create(wifi_poll_cb, 1000, nullptr);

    /* ---- Time ---- */
    lv_obj_set_flex_flow(t_time, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t_time, 10, LV_PART_MAIN);
    section(t_time, "Time zone");
    {
        String opts;
        for (int i = 0; i < TIMEZONE_COUNT; i++) {
            opts += TIMEZONES[i].label;
            if (i < TIMEZONE_COUNT - 1) opts += "\n";
        }
        roller_tz = lv_roller_create(t_time);
        lv_roller_set_options(roller_tz, opts.c_str(), LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller_tz, 3);
        lv_obj_set_width(roller_tz, 400);
        lv_obj_set_style_text_font(roller_tz, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_roller_set_selected(roller_tz, settings_timezone_index(), LV_ANIM_OFF);
    }
    {
        lv_obj_t *row = lv_obj_create(t_time);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 400, 40);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 16, LV_PART_MAIN);
        body_label(row, "24-hour clock");
        sw_24h = lv_switch_create(row);
        if (s.use_24h) lv_obj_add_state(sw_24h, LV_STATE_CHECKED);
    }
    make_button(t_time, LV_SYMBOL_REFRESH "  Apply & sync NTP now", sync_now_cb, nullptr);

    /* ---- Place ---- */
    lv_obj_set_flex_flow(t_place, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t_place, 8, LV_PART_MAIN);
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "%s  (%.3f, %.3f)", s.city, s.latitude, s.longitude);
        section(t_place, "Current location");
        lbl_place = body_label(t_place, buf);
    }
    ta_city = lv_textarea_create(t_place);
    lv_textarea_set_one_line(ta_city, true);
    lv_textarea_set_placeholder_text(ta_city, "City name");
    lv_obj_set_width(ta_city, 560);
    lv_obj_add_event_cb(ta_city, ta_open_editor, LV_EVENT_CLICKED, (void *)"City name");

    make_button(t_place, LV_SYMBOL_GPS "  Search", geo_search_cb, nullptr);

    list_geo = lv_list_create(t_place);
    lv_obj_set_size(list_geo, 560, 150);
    lv_obj_set_style_bg_color(list_geo, lv_color_hex(0x141414), LV_PART_MAIN);

    /* ---- Screen ---- */
    lv_obj_set_flex_flow(t_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t_screen, 6, LV_PART_MAIN);
    {
        char buf[8];
        section(t_screen, "Day brightness");
        lv_obj_t *r1 = lv_obj_create(t_screen);
        lv_obj_remove_style_all(r1);
        lv_obj_set_size(r1, 540, 36);
        lv_obj_set_flex_flow(r1, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r1, 14, LV_PART_MAIN);
        sl_day = lv_slider_create(r1);
        lv_obj_set_width(sl_day, 440);
        lv_slider_set_range(sl_day, 10, 255);
        lv_slider_set_value(sl_day, s.brightness_day, LV_ANIM_OFF);
        lv_obj_add_event_cb(sl_day, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        snprintf(buf, sizeof(buf), "%d", s.brightness_day);
        lbl_day = body_label(r1, buf);

        section(t_screen, "Night brightness");
        lv_obj_t *r2 = lv_obj_create(t_screen);
        lv_obj_remove_style_all(r2);
        lv_obj_set_size(r2, 540, 36);
        lv_obj_set_flex_flow(r2, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r2, 14, LV_PART_MAIN);
        sl_night = lv_slider_create(r2);
        lv_obj_set_width(sl_night, 440);
        lv_slider_set_range(sl_night, 5, 255);
        lv_slider_set_value(sl_night, s.brightness_night, LV_ANIM_OFF);
        lv_obj_add_event_cb(sl_night, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        snprintf(buf, sizeof(buf), "%d", s.brightness_night);
        lbl_night = body_label(r2, buf);
    }
    {
        static char hopts[128];
        hours_options(hopts, sizeof(hopts));
        lv_obj_t *r3 = lv_obj_create(t_screen);
        lv_obj_remove_style_all(r3);
        lv_obj_set_size(r3, 540, 90);
        lv_obj_set_flex_flow(r3, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r3, 12, LV_PART_MAIN);
        body_label(r3, "Night from");
        roller_ns = lv_roller_create(r3);
        lv_roller_set_options(roller_ns, hopts, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller_ns, 2);
        lv_roller_set_selected(roller_ns, s.night_start_hour, LV_ANIM_OFF);
        body_label(r3, "to");
        roller_ne = lv_roller_create(r3);
        lv_roller_set_options(roller_ne, hopts, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller_ne, 2);
        lv_roller_set_selected(roller_ne, s.night_end_hour, LV_ANIM_OFF);
    }
    {
        lv_obj_t *r4 = lv_obj_create(t_screen);
        lv_obj_remove_style_all(r4);
        lv_obj_set_size(r4, 540, 40);
        lv_obj_set_flex_flow(r4, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r4, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r4, 14, LV_PART_MAIN);
        body_label(r4, "Weather");
        sw_wx = lv_switch_create(r4);
        if (s.show_weather) lv_obj_add_state(sw_wx, LV_STATE_CHECKED);
        body_label(r4, "Humidity");
        sw_hum = lv_switch_create(r4);
        if (s.show_humidity) lv_obj_add_state(sw_hum, LV_STATE_CHECKED);
        body_label(r4, "Burn-in guard");
        sw_burn = lv_switch_create(r4);
        if (s.burnin_guard) lv_obj_add_state(sw_burn, LV_STATE_CHECKED);
    }

    /* ---- BLE ---- */
    lv_obj_set_flex_flow(t_ble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t_ble, 10, LV_PART_MAIN);
    {
        lv_obj_t *row = lv_obj_create(t_ble);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 520, 40);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 16, LV_PART_MAIN);
        body_label(row, "Bluetooth LE");
        sw_ble = lv_switch_create(row);
        if (s.ble_enabled) lv_obj_add_state(sw_ble, LV_STATE_CHECKED);
    }

    section(t_ble, "Device name (as advertised)");
    ta_ble_name = lv_textarea_create(t_ble);
    lv_textarea_set_one_line(ta_ble_name, true);
    lv_textarea_set_max_length(ta_ble_name, sizeof(s.ble_name) - 1);
    lv_textarea_set_text(ta_ble_name, s.ble_name);
    lv_obj_set_width(ta_ble_name, 520);
    lv_obj_add_event_cb(ta_ble_name, ta_open_editor, LV_EVENT_CLICKED, (void *)"Bluetooth device name");

    {
        char buf[96];
        snprintf(buf, sizeof(buf), "Status: %s%s",
                 ble_is_running() ? "advertising" : "off",
                 ble_is_connected() ? ", client connected" : "");
        lbl_ble_state = body_label(t_ble, buf);
    }

    {
        lv_obj_t *row = lv_obj_create(t_ble);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 560, 40);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 16, LV_PART_MAIN);
        body_label(row, "Pairable (appear in Bluetooth settings)");
        sw_hid = lv_switch_create(row);
        if (s.ble_hid) lv_obj_add_state(sw_hid, LV_STATE_CHECKED);
    }

    section(t_ble,
            "Pairable ON: advertises as a HID keyboard, so it shows up in\n"
            "macOS System Settings > Bluetooth and can be connected. It\n"
            "never sends keystrokes. macOS may open Keyboard Setup\n"
            "Assistant on first pair -- just close it.\n"
            "OFF: GATT only. Invisible to Bluetooth settings; still\n"
            "reachable from tools/flipclock.py and any BLE scanner.");

    make_button(t_ble, LV_SYMBOL_TRASH "  Clear pairings", clear_bonds_cb, nullptr);

    /* ---- Info ---- */
    lv_obj_set_flex_flow(t_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(t_info, 8, LV_PART_MAIN);
    {
        NetStatus   st = net_status();
        WeatherData w  = net_weather();
        char buf[420];
        char sync[40];
        if (w.valid && st.time_valid) {
            long age = (long)(time(nullptr) - w.last_sync);
            snprintf(sync, sizeof(sync), "%ld min ago", age / 60);
        } else {
            snprintf(sync, sizeof(sync), "never");
        }
        snprintf(buf, sizeof(buf),
                 "Host      %s.local\n"
                 "BLE       %s (%s)\n"
                 "SSID      %s\n"
                 "IP        %s\n"
                 "RSSI      %d dBm\n"
                 "Time      %s\n"
                 "Weather   %s\n"
                 "Uptime    %lu min\n"
                 "PSRAM     %u KB free\n"
                 "Heap      %u KB free",
                 MDNS_HOSTNAME,
                 s.ble_name, ble_is_running() ? (ble_is_connected() ? "connected" : "advertising") : "off",
                 s.wifi_ssid[0] ? s.wifi_ssid : "(none)",
                 st.wifi_up ? st.ip : "-",
                 st.rssi,
                 st.time_valid ? "NTP synced" : "not synced",
                 sync,
                 (unsigned long)(millis() / 60000UL),
                 (unsigned)(ESP.getFreePsram() / 1024),
                 (unsigned)(ESP.getFreeHeap() / 1024));
        lbl_info_body = lv_label_create(t_info);
        lv_obj_set_style_text_font(lbl_info_body, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_info_body, lv_color_hex(0xC8C8C8), LV_PART_MAIN);
        lv_label_set_text(lbl_info_body, buf);
    }
    make_button(t_info, LV_SYMBOL_TRASH "  Reset to defaults", reset_cb, nullptr);

    /* ---- bottom bar ---- */
    lv_obj_t *bar = lv_obj_create(scr_set);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 600, 54);
    lv_obj_set_pos(bar, 0, 396);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x141414), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *done = make_button(bar, LV_SYMBOL_OK "  Save & close", close_cb, nullptr);
    lv_obj_set_width(done, 240);
    lv_obj_align(done, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(scr_set);
}
