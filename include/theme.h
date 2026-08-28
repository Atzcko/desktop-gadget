/**
 * theme.h — the device's look, as data. See D049.
 *
 * A theme is a small table of colors every screen reads AT BUILD TIME —
 * and every screen on this device is rebuilt on entry (D026), so applying a
 * theme is just: save the choice, rebuild what is visible. The rotation
 * machinery (D043) already knows how to do exactly that.
 *
 * Theme 0 "Fliqlo" is the original: charcoal cards, white digits, black.
 * Theme 1 "Pop" is the bento-widget look the owner showed: vivid blue /
 * orange / red / yellow cards on the same black, chunky corners.
 * The background stays black in every theme — this is an AMOLED, black is
 * free, and the burn-in budget is calibrated to it.
 */
#pragma once
#include <lvgl.h>

struct Theme {
    const char *name;
    uint32_t card;        /* hour card, default card everywhere       */
    uint32_t card2;       /* minute card — Pop makes it differ        */
    uint32_t digit;       /* digits on cards                          */
    uint32_t colon;       /* the two dots                             */
    uint32_t wx_temp;     /* temperature card bg                      */
    uint32_t wx_hum;      /* humidity card bg                         */
    uint32_t tile[3];     /* drawer tiles, cycled by index            */
    uint32_t chip;        /* std back chip + strip keys               */
    uint32_t timer2;      /* timer seconds card                       */
    int      radius_add;  /* Pop is chunkier                          */
};

const Theme &theme_get(void);
int          theme_count(void);
const Theme &theme_at(int i);
