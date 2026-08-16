/**
 * Desktop gadget — LilyGO T4-S3 Fliqlo flip clock
 *
 * Stage 1: static digits. No Wi-Fi, no NTP, no animation.
 */
#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>

#include "config.h"
#include "ui.h"

LilyGo_Class amoled;

void setup()
{
    Serial.begin(115200);

    /* Native USB CDC needs a moment to enumerate before the host attaches.
     * Bounded so a headless boot never stalls here. */
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1500) {
        delay(10);
    }

    Serial.println();
    Serial.println("=== Desktop gadget — Stage 1 (static digits) ===");

    /* Explicit T4-S3 entry point rather than begin()'s I2C auto-probe.
     * SD is skipped: the slot is unused and SD.begin() costs boot time
     * we need for the <10 s cold-boot target. See D007. */
    if (!amoled.beginAMOLED_241(/*disable_sd=*/true, /*disable_state_led=*/false)) {
        Serial.println("FATAL: beginAMOLED_241() failed");
        while (true) delay(1000);
    }

    Serial.printf("Board      : %s\n", amoled.getName());
    Serial.printf("Panel      : %u x %u\n", amoled.width(), amoled.height());
    Serial.printf("Touch      : %s\n", amoled.hasTouch() ? "online" : "OFFLINE");
    Serial.printf("PSRAM free : %u bytes\n", (unsigned)ESP.getFreePsram());
    Serial.printf("Heap free  : %u bytes\n", (unsigned)ESP.getFreeHeap());

    /* Allocates one full-screen LVGL buffer with ps_malloc — 600*450*2 =
     * 527 KB in PSRAM. See D009. */
    beginLvglHelper(amoled);

    Serial.printf("PSRAM after LVGL : %u bytes\n", (unsigned)ESP.getFreePsram());

    ui_init(amoled.width(), amoled.height());

    /* Stage 1 is deliberately static — real time arrives in Stage 2. */
    ui_set_time(12, 34);
    ui_set_weather(21.0f, 14.0f, 26.0f, true);

    amoled.setBrightness(BRIGHTNESS_DAY);

    Serial.printf("Ready in %lu ms\n", (unsigned long)millis());
}

void loop()
{
    lv_timer_handler();
    delay(2);
}
