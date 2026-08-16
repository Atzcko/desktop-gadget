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
static uint8_t       cur_state;

/*
 * Named points in the circumplex. Coverage is deliberate: Plutchik's eight
 * primaries, Ekman's six, and the work states this device actually needs,
 * spread across all four quadrants so the line has real range.
 *
 *                        high arousal
 *                             |
 *      anger, fear      ...  surprise, excited, celebrate
 *   negative ------------+------------ positive valence
 *      sad, bored       ...  calm, content, trust
 *                             |
 *                        low arousal
 */
const EmotionDef EMOTIONS[] = {
    /*  name            valence  arousal  character    spectrum */
    {  "none",            0.00f,   0.00f, CH_SMOOTH,  false },

    /* --- working states: what this device says most of the time --- */
    {  "thinking",        0.10f,   0.30f, CH_SMOOTH,  false },
    {  "working",         0.20f,   0.52f, CH_SMOOTH,  false },
    {  "searching",       0.05f,   0.46f, CH_SCAN,    false },
    {  "focused",         0.30f,   0.45f, CH_SMOOTH,  false },
    {  "waiting",         0.00f,   0.10f, CH_SCAN,    false },

    /* --- what I am actually doing: the primary use of this device --- */
    {  "reading",         0.10f,   0.22f, CH_SCAN,    false },
    {  "editing",         0.25f,   0.45f, CH_SMOOTH,  false },
    {  "building",        0.20f,   0.68f, CH_SMOOTH,  false },
    {  "testing",         0.15f,   0.56f, CH_SCAN,    false },
    {  "flashing",        0.30f,   0.86f, CH_SMOOTH,  false },
    {  "debugging",      -0.15f,   0.62f, CH_TREMOR,  false },

    /* --- positive --- */
    {  "success",         0.80f,   0.60f, CH_SMOOTH,  false },
    {  "celebrate",       1.00f,   0.95f, CH_SMOOTH,  true  },
    {  "joy",             0.90f,   0.80f, CH_SMOOTH,  false },
    {  "excited",         0.75f,   0.90f, CH_SMOOTH,  false },
    {  "proud",           0.70f,   0.50f, CH_SMOOTH,  false },
    {  "trust",           0.50f,   0.30f, CH_SMOOTH,  false },
    {  "content",         0.55f,   0.20f, CH_SMOOTH,  false },
    {  "calm",            0.40f,   0.12f, CH_SMOOTH,  false },
    {  "relief",          0.45f,   0.25f, CH_DROOP,   false },

    /* --- low arousal --- */
    {  "sleepy",          0.10f,   0.05f, CH_SMOOTH,  false },
    {  "bored",          -0.25f,   0.10f, CH_SMOOTH,  false },

    /* --- negative --- */
    {  "sad",            -0.60f,   0.20f, CH_DROOP,   false },
    {  "disappointed",   -0.50f,   0.30f, CH_DROOP,   false },
    {  "confused",       -0.10f,   0.50f, CH_TREMOR,  false },
    {  "surprise",        0.15f,   0.90f, CH_JAGGED,  false },
    {  "fear",           -0.70f,   0.85f, CH_TREMOR,  false },
    {  "frustrated",     -0.60f,   0.70f, CH_JAGGED,  false },
    {  "anger",          -0.85f,   0.90f, CH_JAGGED,  false },
    {  "error",          -0.90f,   0.80f, CH_JAGGED,  false },
    {  "disgust",        -0.70f,   0.40f, CH_TREMOR,  false },
    {  "anticipation",   0.35f,    0.60f, CH_SCAN,    false },
};

const int EMOTION_COUNT = (int)(sizeof(EMOTIONS) / sizeof(EMOTIONS[0]));

const EmotionDef &emotion_def(uint8_t state)
{
    if (state >= EMOTION_COUNT) state = 0;
    return EMOTIONS[state];
}

const char *emotion_name(uint8_t state)
{
    return emotion_def(state).name;
}

int emotion_from_name(const char *name)
{
    if (!name) return -1;
    /* Explicit stop. State 0 is "none", which the search loop below skips on
     * purpose — you should not be able to reach it by accident. */
    if (strcasecmp(name, "none")  == 0 || strcasecmp(name, "clear") == 0 ||
        strcasecmp(name, "idle")  == 0 || strcasecmp(name, "stop")  == 0) return 0;
    for (int i = 1; i < EMOTION_COUNT; i++) {
        if (strcasecmp(name, EMOTIONS[i].name) == 0) return i;
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
    if (st == 0) {                       /* stop now, back to the clock */
        out->state      = 0;
        out->duration_s = 0;
        return true;
    }
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

uint8_t emotion_current_state(void) { return cur_state; }

uint32_t emotion_remaining_s(void)
{
    if (!active) return 0;
    int32_t left = (int32_t)(revert_at_ms - millis());
    return left <= 0 ? 0 : (uint32_t)((left + 999) / 1000);
}

void emotion_tick(void)
{
    EmotionRequest req;
    if (q && xQueueReceive(q, &req, 0) == pdPASS) {
        if (req.state == 0) {
            Serial.println("[emotion] stop — back to the clock");
            ui_emotion_clear();
            active    = false;
            cur_state = 0;
        } else {
            Serial.printf("[emotion] %s for %us%s%s\n",
                          emotion_name(req.state), req.duration_s,
                          req.message[0] ? " — " : "", req.message);
            ui_emotion_show(req.state, req.message);
            revert_at_ms = millis() + (uint32_t)req.duration_s * 1000u;
            active    = true;
            cur_state = req.state;
        }
    }

    if (active && (int32_t)(millis() - revert_at_ms) >= 0) {
        ui_emotion_clear();
        active    = false;
        cur_state = 0;
    }
}
