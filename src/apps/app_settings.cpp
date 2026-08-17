/**
 * app_settings.cpp — Settings, adopted into the app platform.
 *
 * All of the screen already lived in ui_settings.cpp; D026 predicted this would
 * be a thin adapter rather than a rewrite, because that file was already an app
 * in everything but name. This is the whole of it.
 */
#include "app_api.h"
#include "ui_settings.h"
#include "app_host.h"

static void settings_icon(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t co; lv_obj_get_coords(o, &co);
    const int cx = co.x1 + lv_area_get_width(&co) / 2;
    const int cy = co.y1 + lv_area_get_height(&co) / 2;

    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = lv_color_hex(0xE8E8E8);
    a.width = 9;
    a.opa   = LV_OPA_COVER;
    lv_point_t c = { (lv_coord_t)cx, (lv_coord_t)cy };

    /* Six teeth around a ring: a gear at 118 px without any path data. */
    for (int i = 0; i < 6; i++) lv_draw_arc(ctx, &a, &c, 30, i * 60, i * 60 + 34);
    a.width = 5;
    lv_draw_arc(ctx, &a, &c, 14, 0, 360);
}

extern const App app_settings = {
    "Settings", settings_icon, ui_settings_create, ui_settings_destroy,
    nullptr, ui_settings_back
};

/*
 * The 3-second hold still opens Settings directly — it is muscle memory and
 * predates the drawer. It now routes through the host so there is exactly ONE
 * path that creates, loads and destroys a screen, rather than two that have to
 * be kept in agreement.
 */
void ui_settings_open(void) { app_host_launch(&app_settings); }
