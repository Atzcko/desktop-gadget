#include "emotion.h"
#include "ui.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

static QueueHandle_t q;
static uint32_t      revert_at_ms;
static bool          active;

static const char *NAMES[] = {
    "none", "thinking", "working", "success", "error", "celebrate", "sleepy"
};

const char *emotion_name(uint8_t state)
{
    return (state < sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[state] : "none";
}

int emotion_from_name(const char *name)
{
    if (!name) return -1;
    for (size_t i = 1; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        if (strcasecmp(name, NAMES[i]) == 0) return (int)i;
    }
    return -1;
}

bool emotion_parse(const char *json, EmotionRequest *out, char *err, size_t errcap)
{
    memset(out, 0, sizeof(*out));

    StaticJsonDocument<256> doc;
    DeserializationError e = deserializeJson(doc, json);
    if (e) {
        snprintf(err, errcap, "malformed JSON: %s", e.c_str());
        return false;
    }

    const char *state = doc["state"] | (const char *)nullptr;
    if (!state) {
        snprintf(err, errcap, "missing \"state\"");
        return false;
    }

    int st = emotion_from_name(state);
    if (st < 0) {
        /* Single quotes, not double: this string is interpolated straight
         * into a JSON error body, and a raw " would make the response
         * unparseable by the very client trying to read the error. */
        snprintf(err, errcap,
                 "unknown state '%s' (thinking|working|success|error|celebrate|sleepy)",
                 state);
        return false;
    }
    out->state = (uint8_t)st;

    /* Absent duration is not an error — 5 s is a sensible default for a
     * transient expression. */
    int dur = doc["duration_s"] | 5;
    if (dur < 1)                   dur = 1;
    if (dur > EMOTION_MAX_SECONDS) dur = EMOTION_MAX_SECONDS;
    out->duration_s = (uint16_t)dur;

    const char *msg = doc["message"] | "";
    if (strlen(msg) > EMOTION_MSG_MAX) {
        snprintf(err, errcap, "message longer than %d characters", EMOTION_MSG_MAX);
        return false;
    }
    strncpy(out->message, msg, EMOTION_MSG_MAX);
    out->message[EMOTION_MSG_MAX] = '\0';

    return true;
}

bool emotion_post(const EmotionRequest &req)
{
    if (!q) return false;
    /* Non-blocking: a wedged UI must never stall the BLE or HTTP task. */
    return xQueueSend(q, &req, 0) == pdPASS;
}

void emotion_begin(void)
{
    q = xQueueCreate(4, sizeof(EmotionRequest));
}

bool emotion_active(void) { return active; }

void emotion_tick(void)
{
    EmotionRequest req;
    if (q && xQueueReceive(q, &req, 0) == pdPASS) {
        Serial.printf("[emotion] %s for %us%s%s\n",
                      emotion_name(req.state), req.duration_s,
                      req.message[0] ? " — " : "", req.message);
        ui_emotion_show(req.state, req.message);
        revert_at_ms = millis() + (uint32_t)req.duration_s * 1000u;
        active = true;
    }

    if (active && (int32_t)(millis() - revert_at_ms) >= 0) {
        ui_emotion_clear();
        active = false;
    }
}
