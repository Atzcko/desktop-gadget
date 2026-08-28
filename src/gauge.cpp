#include "gauge.h"
#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

/*
 * Discharge model: base draw plus panel draw scaled by brightness.
 * Numbers from the measured-component estimate in the 2026-08-23 battery-life
 * assessment (90-170 mA across the brightness range). The model does not need
 * to be exact — the voltage correction bounds its error — it needs to have
 * the right SHAPE, so the percentage falls faster at day brightness than at
 * night, which is what a voltage-only gauge cannot show.
 */
#define BASE_MA          88.0f     /* S3 + Wi-Fi modem-sleep + BLE + PMU  */
#define PANEL_MA_FULL    82.0f     /* AMOLED at brightness 255            */
#define CHG_EFFICIENCY   0.98f     /* li-ion coulombic efficiency         */
#define VOLT_TC_S        3000.0f   /* pull toward the voltage curve, ~50 min */

static Preferences prefs;
static float soc = -1.0f;          /* 0..100, authoritative state          */
static float mah_used = -1.0f;     /* since last full; -1 = never anchored */
static float last_saved_soc = -1.0f;
static uint32_t last_save_ms;

static int volt_pct(uint16_t mv)
{
    static const struct { uint16_t mv; uint8_t pct; } C[] = {
        {4200,100},{4060,90},{3980,80},{3920,70},{3870,60},
        {3820,50},{3790,40},{3770,30},{3740,20},{3680,10},{3450,0},
    };
    const unsigned N = sizeof(C) / sizeof(C[0]);
    if (mv >= C[0].mv) return 100;
    for (unsigned i = 1; i < N; i++) {
        if (mv >= C[i].mv) {
            const int span_mv  = C[i-1].mv - C[i].mv;
            const int span_pct = C[i-1].pct - C[i].pct;
            return C[i].pct + (int)(mv - C[i].mv) * span_pct / span_mv;
        }
    }
    return 0;
}

static int last_ma;   /* signed, for reporting only */

void gauge_begin(uint16_t mv_now)
{
    prefs.begin("gauge", false);
    soc      = prefs.getFloat("soc", -1.0f);
    mah_used = prefs.getFloat("used", -1.0f);

    const int vp = volt_pct(mv_now);
    /* A stored state far from the voltage curve means the battery changed
     * while we were off (swapped, or charged elsewhere). The curve wins. */
    if (soc < 0.0f || fabsf(soc - (float)vp) > 25.0f) {
        soc = (float)vp;
        mah_used = -1.0f;
    }
    last_saved_soc = soc;
    Serial.printf("[gauge] start soc=%.1f%% (voltage says %d%%)\n", soc, vp);
}

static void maybe_persist(void)
{
    const uint32_t now = millis();
    if (fabsf(soc - last_saved_soc) < 1.0f && now - last_save_ms < 600000UL) return;
    if (now - last_save_ms < 60000UL) return;      /* NVS wear floor: 1/min */
    prefs.putFloat("soc", soc);
    prefs.putFloat("used", mah_used);
    last_saved_soc = soc;
    last_save_ms   = now;
}

void gauge_update(uint16_t mv, bool present, bool charging, bool charge_done,
                  uint16_t charge_ma, uint8_t brightness, uint32_t dt_ms)
{
    if (!present) { last_ma = 0; return; }
    if (soc < 0.0f) soc = (float)volt_pct(mv);

    const float cap_mah = (float)settings_get().batt_mah;
    const float dt_s    = (float)dt_ms / 1000.0f;
    const float pct_per_mah = 100.0f / cap_mah;

    if (charge_done) {
        /* The one absolute calibration point this hardware offers. */
        soc = 100.0f;
        mah_used = 0.0f;
        last_ma = 0;
    } else if (charging && charge_ma > 0) {
        /* MEASURED current in: this half really is a coulomb counter. */
        const float mah_in = (float)charge_ma * CHG_EFFICIENCY * dt_s / 3600.0f;
        soc += mah_in * pct_per_mah;
        if (mah_used >= 0.0f) mah_used -= mah_in;
        if (mah_used < 0.0f && mah_used != -1.0f) mah_used = 0.0f;
        last_ma = (int)charge_ma;
    } else {
        /* MODELLED current out, with the voltage curve as a slow tether so
         * model error cannot accumulate past what the curve allows. */
        const float ma = BASE_MA + PANEL_MA_FULL * (float)brightness / 255.0f;
        const float mah_out = ma * dt_s / 3600.0f;
        soc -= mah_out * pct_per_mah;
        if (mah_used >= 0.0f) mah_used += mah_out;
        soc += ((float)volt_pct(mv) - soc) * (dt_s / VOLT_TC_S);
        last_ma = -(int)ma;
    }

    if (soc > 100.0f) soc = 100.0f;
    if (soc < 0.0f)   soc = 0.0f;
    maybe_persist();
}

int gauge_pct(void)      { return soc < 0.0f ? -1 : (int)(soc + 0.5f); }
int gauge_ma(void)       { return last_ma; }
int gauge_mah_used(void) { return mah_used < 0.0f ? -1 : (int)(mah_used + 0.5f); }
