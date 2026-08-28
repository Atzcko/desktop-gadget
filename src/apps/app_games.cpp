/**
 * app_games.cpp — the arcade. See D055.
 *
 * One app, a chooser, and games written in the house style: theme colors,
 * the digit font for scores, true black behind everything. Snake plays on a
 * canvas grid; Breakout on plain LVGL objects. Both are silent like the
 * rest of the device and both lay out for either orientation.
 *
 * In-app stack under the host's back: game -> chooser -> host pops.
 */
#include "app_api.h"
#include "app_host.h"
#include "theme.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" { LV_FONT_DECLARE(fliqlo_mid); }

#define COL_BG   lv_color_hex(0x000000)
#define COL_TEXT lv_color_hex(0xE8E8E8)
#define COL_DIM  lv_color_hex(0x8A8A8A)

enum GView { V_MENU, V_SNAKE, V_BREAKOUT };
static GView     view;
static lv_obj_t *scr, *game_view;
static Preferences gprefs;

/* =============================================================== SNAKE == */

#define CELL      15
static int  gw, gh;                       /* grid size, from the display   */
static lv_obj_t  *sn_canvas, *sn_score_lbl;
static lv_color_t *sn_buf;
static int8_t   *sn_body;                 /* x,y pairs, head first         */
static int       sn_len, sn_dir, sn_pending;   /* dir: 0=R 1=D 2=L 3=U    */
static int8_t    food_x, food_y;
static uint32_t  sn_last_step, sn_step_ms;
static bool      sn_dead;
static int       sn_score, sn_hi;
static lv_point_t sw_start;

static void sn_place_food(void)
{
    for (;;) {
        food_x = (int8_t)(esp_random() % gw);
        food_y = (int8_t)(esp_random() % gh);
        bool on = false;
        for (int i = 0; i < sn_len; i++)
            if (sn_body[i*2] == food_x && sn_body[i*2+1] == food_y) { on = true; break; }
        if (!on) return;
    }
}

static void sn_draw(void)
{
    const Theme &t = theme_get();
    lv_canvas_fill_bg(sn_canvas, COL_BG, LV_OPA_COVER);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = 3;

    d.bg_color = lv_color_hex(t.chip);
    lv_canvas_draw_rect(sn_canvas, food_x * CELL + 2, food_y * CELL + 2,
                        CELL - 4, CELL - 4, &d);

    d.bg_color = sn_dead ? lv_color_hex(0xE0483B) : lv_color_hex(t.digit);
    for (int i = 0; i < sn_len; i++)
        lv_canvas_draw_rect(sn_canvas, sn_body[i*2] * CELL + 1,
                            sn_body[i*2+1] * CELL + 1, CELL - 2, CELL - 2, &d);
}

static void sn_reset(void)
{
    sn_len = 4; sn_dir = 0; sn_pending = 0;
    sn_dead = false; sn_score = 0; sn_step_ms = 150;
    for (int i = 0; i < sn_len; i++) { sn_body[i*2] = gw/2 - i; sn_body[i*2+1] = gh/2; }
    sn_place_food();
    sn_last_step = millis();
    char b[24]; snprintf(b, sizeof(b), "%d", sn_score);
    lv_label_set_text(sn_score_lbl, b);
    sn_draw();
}

static void sn_pad_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p; lv_indev_get_point(indev, &p);
    if (c == LV_EVENT_PRESSED) { sw_start = p; return; }
    if (c != LV_EVENT_RELEASED) return;
    if (sn_dead) { sn_reset(); return; }

    const int dx = p.x - sw_start.x, dy = p.y - sw_start.y;
    if (abs(dx) < 20 && abs(dy) < 20) return;
    int nd = (abs(dx) > abs(dy)) ? (dx > 0 ? 0 : 2) : (dy > 0 ? 1 : 3);
    if ((nd ^ 2) != sn_dir) sn_pending = nd;      /* no instant reversal */
}

static void sn_tick(void)
{
    if (sn_dead || millis() - sn_last_step < sn_step_ms) return;
    sn_last_step = millis();
    sn_dir = sn_pending;

    int nx = sn_body[0] + (sn_dir == 0) - (sn_dir == 2);
    int ny = sn_body[1] + (sn_dir == 1) - (sn_dir == 3);
    bool hit = nx < 0 || ny < 0 || nx >= gw || ny >= gh;
    for (int i = 0; !hit && i < sn_len - 1; i++)
        if (sn_body[i*2] == nx && sn_body[i*2+1] == ny) hit = true;
    if (hit) {
        sn_dead = true;
        if (sn_score > sn_hi) { sn_hi = sn_score; gprefs.putInt("snake_hi", sn_hi); }
        char b[40]; snprintf(b, sizeof(b), "%d   best %d   tap to retry", sn_score, sn_hi);
        lv_label_set_text(sn_score_lbl, b);
        sn_draw();
        return;
    }

    const bool ate = (nx == food_x && ny == food_y);
    const int keep = ate ? sn_len : sn_len - 1;
    for (int i = keep; i > 0; i--) {
        sn_body[i*2]   = sn_body[(i-1)*2];
        sn_body[i*2+1] = sn_body[(i-1)*2+1];
    }
    sn_body[0] = (int8_t)nx; sn_body[1] = (int8_t)ny;
    if (ate) {
        sn_len++;
        sn_score += 10;
        if (sn_step_ms > 70) sn_step_ms -= 3;
        sn_place_food();
        char b[24]; snprintf(b, sizeof(b), "%d", sn_score);
        lv_label_set_text(sn_score_lbl, b);
    }
    sn_draw();
}

static void snake_open(void)
{
    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);
    gw = (W - 20) / CELL;
    gh = (H - 20 - 46 - 66) / CELL;       /* score row + strip spare */

    game_view = lv_obj_create(scr);
    lv_obj_remove_style_all(game_view);
    lv_obj_set_size(game_view, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(game_view, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(game_view, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(game_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(game_view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(game_view, sn_pad_cb, LV_EVENT_ALL, nullptr);

    sn_score_lbl = lv_label_create(game_view);
    lv_obj_set_style_text_font(sn_score_lbl, &fliqlo_mid, LV_PART_MAIN);
    lv_obj_set_style_text_color(sn_score_lbl, COL_DIM, LV_PART_MAIN);
    lv_obj_align(sn_score_lbl, LV_ALIGN_TOP_MID, 0, 4);

    sn_buf = (lv_color_t *)heap_caps_malloc(
        LV_CANVAS_BUF_SIZE_TRUE_COLOR(gw * CELL, gh * CELL),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    sn_body = (int8_t *)heap_caps_malloc(gw * gh * 2, MALLOC_CAP_SPIRAM);
    sn_canvas = lv_canvas_create(game_view);
    lv_canvas_set_buffer(sn_canvas, sn_buf, gw * CELL, gh * CELL, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(sn_canvas, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_clear_flag(sn_canvas, LV_OBJ_FLAG_CLICKABLE);

    sn_hi = gprefs.getInt("snake_hi", 0);
    sn_reset();
}

/* ============================================================ BREAKOUT == */

#define BR_COLS 8
#define BR_ROWS 5
static lv_obj_t *br_paddle, *br_ball, *br_bricks[BR_COLS * BR_ROWS], *br_lbl;
static float bx, by, bvx, bvy;
static int   br_left, br_score, br_lives, br_hi;
static bool  br_running, br_over;
static int   pw, ph, paddle_w;
static uint32_t br_last;

static void br_status(void)
{
    char b[48];
    if (br_over) snprintf(b, sizeof(b), "%d   best %d   tap to retry", br_score, br_hi);
    else snprintf(b, sizeof(b), "%d    %s", br_score,
                  br_lives == 3 ? "\xE2\x97\x8F\xE2\x97\x8F\xE2\x97\x8F" :
                  br_lives == 2 ? "\xE2\x97\x8F\xE2\x97\x8F" :
                  br_lives == 1 ? "\xE2\x97\x8F" : "");
    lv_label_set_text(br_lbl, b);
}

static void br_serve(void)
{
    bx = pw / 2.0f; by = ph - 80.0f;
    bvx = 2.4f * ((esp_random() & 1) ? 1 : -1);
    bvy = -3.2f;
    br_running = false;                    /* first tap launches */
    lv_obj_set_pos(br_ball, (int)bx - 7, (int)by - 7);
}

static void br_reset(void)
{
    const Theme &t = theme_get();
    br_score = 0; br_lives = 3; br_over = false; br_left = BR_COLS * BR_ROWS;
    for (int i = 0; i < BR_COLS * BR_ROWS; i++) {
        lv_obj_clear_flag(br_bricks[i], LV_OBJ_FLAG_HIDDEN);
        const uint32_t rowc[5] = { t.card2, t.tile[0], t.tile[1], t.tile[2], t.chip };
        lv_obj_set_style_bg_color(br_bricks[i], lv_color_hex(rowc[i / BR_COLS]),
                                  LV_PART_MAIN);
    }
    br_serve();
    br_status();
}

static void br_pad_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p; lv_indev_get_point(indev, &p);
    if (c == LV_EVENT_PRESSING || c == LV_EVENT_PRESSED) {
        int x = p.x - paddle_w / 2;
        if (x < 0) x = 0;
        if (x > pw - paddle_w) x = pw - paddle_w;
        lv_obj_set_x(br_paddle, x);
    }
    if (c == LV_EVENT_RELEASED) {
        if (br_over) br_reset();
        else br_running = true;
    }
}

static void br_tick(void)
{
    if (!br_running || br_over) return;
    const uint32_t now = millis();
    float dt = (now - br_last) / 16.0f;    /* ~60fps units */
    br_last = now;
    if (dt > 4) dt = 4;

    bx += bvx * dt; by += bvy * dt;
    if (bx < 7)        { bx = 7;        bvx = -bvx; }
    if (bx > pw - 7)   { bx = pw - 7;   bvx = -bvx; }
    if (by < 46 + 7)   { by = 46 + 7;   bvy = -bvy; }

    /* paddle */
    const int px = lv_obj_get_x(br_paddle), py = ph - 46;
    if (by > py - 7 && by < py + 10 && bx > px - 7 && bx < px + paddle_w + 7 && bvy > 0) {
        bvy = -bvy;
        bvx += ((bx - (px + paddle_w / 2.0f)) / paddle_w) * 3.0f;   /* english */
        if (bvx > 4.5f) bvx = 4.5f;
        if (bvx < -4.5f) bvx = -4.5f;
    }

    /* bricks */
    for (int i = 0; i < BR_COLS * BR_ROWS; i++) {
        if (lv_obj_has_flag(br_bricks[i], LV_OBJ_FLAG_HIDDEN)) continue;
        const int rx = lv_obj_get_x(br_bricks[i]), ry = lv_obj_get_y(br_bricks[i]);
        const int rw = lv_obj_get_width(br_bricks[i]), rh = lv_obj_get_height(br_bricks[i]);
        if (bx > rx - 7 && bx < rx + rw + 7 && by > ry - 7 && by < ry + rh + 7) {
            lv_obj_add_flag(br_bricks[i], LV_OBJ_FLAG_HIDDEN);
            br_score += 5; br_left--;
            const float cx = rx + rw / 2.0f, cy = ry + rh / 2.0f;
            if (fabsf(bx - cx) / rw > fabsf(by - cy) / rh) bvx = -bvx; else bvy = -bvy;
            br_status();
            if (br_left == 0) {
                br_over = true;
                if (br_score > br_hi) { br_hi = br_score; gprefs.putInt("brk_hi", br_hi); }
                br_status();
            }
            break;
        }
    }

    if (by > ph - 8) {                     /* lost the ball */
        br_lives--;
        if (br_lives <= 0) {
            br_over = true;
            if (br_score > br_hi) { br_hi = br_score; gprefs.putInt("brk_hi", br_hi); }
        }
        br_status();
        if (!br_over) br_serve();
    }
    lv_obj_set_pos(br_ball, (int)bx - 7, (int)by - 7);
}

static void breakout_open(void)
{
    const Theme &t = theme_get();
    pw = lv_disp_get_hor_res(nullptr);
    ph = lv_disp_get_ver_res(nullptr) - 66;     /* room above the strip */
    paddle_w = pw / 5;

    game_view = lv_obj_create(scr);
    lv_obj_remove_style_all(game_view);
    lv_obj_set_size(game_view, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(game_view, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(game_view, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(game_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(game_view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(game_view, br_pad_cb, LV_EVENT_ALL, nullptr);

    br_lbl = lv_label_create(game_view);
    lv_obj_set_style_text_font(br_lbl, &fliqlo_mid, LV_PART_MAIN);
    lv_obj_set_style_text_color(br_lbl, COL_DIM, LV_PART_MAIN);
    lv_obj_align(br_lbl, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_clear_flag(br_lbl, LV_OBJ_FLAG_CLICKABLE);

    const int bw = (pw - 20 - (BR_COLS - 1) * 6) / BR_COLS;
    for (int r = 0; r < BR_ROWS; r++)
        for (int c = 0; c < BR_COLS; c++) {
            lv_obj_t *b = lv_obj_create(game_view);
            lv_obj_remove_style_all(b);
            lv_obj_set_size(b, bw, 18);
            lv_obj_set_pos(b, 10 + c * (bw + 6), 52 + r * 26);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(b, 5, LV_PART_MAIN);
            lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
            br_bricks[r * BR_COLS + c] = b;
        }

    br_paddle = lv_obj_create(game_view);
    lv_obj_remove_style_all(br_paddle);
    lv_obj_set_size(br_paddle, paddle_w, 12);
    lv_obj_set_pos(br_paddle, (pw - paddle_w) / 2, ph - 46);
    lv_obj_set_style_bg_color(br_paddle, lv_color_hex(t.digit), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(br_paddle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(br_paddle, 6, LV_PART_MAIN);
    lv_obj_clear_flag(br_paddle, LV_OBJ_FLAG_CLICKABLE);

    br_ball = lv_obj_create(game_view);
    lv_obj_remove_style_all(br_ball);
    lv_obj_set_size(br_ball, 14, 14);
    lv_obj_set_style_bg_color(br_ball, lv_color_hex(t.colon), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(br_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(br_ball, 7, LV_PART_MAIN);
    lv_obj_clear_flag(br_ball, LV_OBJ_FLAG_CLICKABLE);

    br_hi = gprefs.getInt("brk_hi", 0);
    br_last = millis();
    br_reset();
}

/* ============================================================== chooser == */

static void game_close(void);
static void menu_build(void);

static void menu_pick_cb(lv_event_t *e)
{
    const int which = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *dead = lv_obj_get_parent(lv_event_get_target(e));
    (void)dead;
    /* clear the menu, open the game */
    lv_obj_clean(scr);
    game_view = nullptr;
    app_host_std_back(scr, nullptr);
    if (which == 0) { view = V_SNAKE;    snake_open(); }
    if (which == 1) { view = V_BREAKOUT; breakout_open(); }
    if (which == 2) { view = V_MENU;     menu_build();  }   /* placeholder */
    if (game_view) lv_obj_move_foreground(game_view);
    /* the strip chip must stay on top of the game surface */
    app_host_std_back(scr, nullptr);
}

static void menu_build(void)
{
    view = V_MENU;
    lv_obj_clean(scr);
    game_view = nullptr;

    const int W = lv_disp_get_hor_res(nullptr);
    const int H = lv_disp_get_ver_res(nullptr);
    const bool portrait = H > W;

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, COL_DIM, LV_PART_MAIN);
    lv_label_set_text(t, "Games   ·   swipe to steer, tap to launch");
    lv_obj_set_pos(t, 14, 10);

    static const char *names[3] = { "Snake", "Breakout", "Game Boy" };
    static const char *subs[3]  = { "swipe to turn", "drag the paddle",
                                    "coming - needs its core (ask Claude)" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *card = lv_btn_create(scr);
        if (portrait) {
            lv_obj_set_size(card, W - 28, (H - 44 - 76 - 2 * 12 - 10) / 3);
            lv_obj_set_pos(card, 14, 44 + i * ((H - 44 - 76 - 2 * 12 - 10) / 3 + 12));
        } else {
            lv_obj_set_size(card, (W - 28 - 2 * 12) / 3, H - 44 - 76 - 10);
            lv_obj_set_pos(card, 14 + i * ((W - 28 - 2 * 12) / 3 + 12), 44);
        }
        lv_obj_set_style_bg_color(card, lv_color_hex(0x161616), LV_PART_MAIN);
        lv_obj_set_style_radius(card, 18 + theme_get().radius_add, LV_PART_MAIN);
        if (i == 2) lv_obj_set_style_bg_color(card, lv_color_hex(0x0E0E0E), LV_PART_MAIN);
        lv_obj_add_event_cb(card, menu_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *n = lv_label_create(card);
        lv_obj_set_style_text_font(n, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(n, i == 2 ? COL_DIM : COL_TEXT, LV_PART_MAIN);
        lv_label_set_text(n, names[i]);
        lv_obj_align(n, LV_ALIGN_CENTER, 0, -14);

        lv_obj_t *s = lv_label_create(card);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(s, COL_DIM, LV_PART_MAIN);
        lv_label_set_text(s, subs[i]);
        lv_obj_align(s, LV_ALIGN_CENTER, 0, 18);
    }
    app_host_std_back(scr, nullptr);
}

static void game_close(void)
{
    if (view == V_SNAKE) {
        if (sn_buf)  { heap_caps_free(sn_buf);  sn_buf = nullptr; }
        if (sn_body) { heap_caps_free(sn_body); sn_body = nullptr; }
        sn_canvas = nullptr; sn_score_lbl = nullptr;
    }
    if (view == V_BREAKOUT) {
        br_paddle = br_ball = br_lbl = nullptr;
        for (int i = 0; i < BR_COLS * BR_ROWS; i++) br_bricks[i] = nullptr;
    }
    menu_build();
}

/* ================================================================== app == */

static void games_icon(lv_event_t *e)
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
    r.radius = 20;
    lv_area_t body = { (lv_coord_t)(cx - 38), (lv_coord_t)(cy - 20),
                       (lv_coord_t)(cx + 38), (lv_coord_t)(cy + 20) };
    lv_draw_rect(ctx, &r, &body);

    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(0xE8E8E8);
    d.bg_opa = LV_OPA_COVER;
    d.radius = 1;
    lv_area_t h1 = { (lv_coord_t)(cx - 28), (lv_coord_t)(cy - 3),
                     (lv_coord_t)(cx - 12), (lv_coord_t)(cy + 3) };
    lv_area_t v1 = { (lv_coord_t)(cx - 23), (lv_coord_t)(cy - 8),
                     (lv_coord_t)(cx - 17), (lv_coord_t)(cy + 8) };
    lv_draw_rect(ctx, &d, &h1);
    lv_draw_rect(ctx, &d, &v1);
    d.radius = 4;
    lv_area_t b1 = { (lv_coord_t)(cx + 14), (lv_coord_t)(cy - 8),
                     (lv_coord_t)(cx + 22), (lv_coord_t)(cy) };
    lv_area_t b2 = { (lv_coord_t)(cx + 24), (lv_coord_t)(cy + 1),
                     (lv_coord_t)(cx + 32), (lv_coord_t)(cy + 9) };
    lv_draw_rect(ctx, &d, &b1);
    lv_draw_rect(ctx, &d, &b2);
}

static lv_obj_t *games_create(void)
{
    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    gprefs.begin("games", false);
    menu_build();
    return scr;
}

static void games_destroy(void)
{
    if (view == V_SNAKE) {
        if (sn_buf)  { heap_caps_free(sn_buf);  sn_buf  = nullptr; }
        if (sn_body) { heap_caps_free(sn_body); sn_body = nullptr; }
    }
    scr = game_view = nullptr;
    sn_canvas = nullptr; sn_score_lbl = nullptr;
    br_paddle = br_ball = br_lbl = nullptr;
    view = V_MENU;
}

static void games_tick(void)
{
    if (!scr) return;
    if (view == V_SNAKE)    sn_tick();
    if (view == V_BREAKOUT) br_tick();
}

static bool games_back(void)
{
    if (view != V_MENU) { game_close(); return true; }
    return false;
}

extern const App app_games = { "Games", games_icon, games_create, games_destroy,
                               games_tick, games_back, /*portrait_ok=*/true };
