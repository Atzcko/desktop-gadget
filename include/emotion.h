/**
 * emotion.h — the expression engine.
 *
 * One implementation, two transports. BLE writes and HTTP POSTs both land
 * in emotion_post(), which is thread-safe; the LVGL task drains the queue
 * in emotion_tick(). Nothing outside emotion_tick() ever touches an LVGL
 * object — that is the same rule the weather task follows.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum EmotionState : uint8_t {
    EMO_NONE = 0,
    EMO_THINKING,
    EMO_WORKING,
    EMO_SUCCESS,
    EMO_ERROR,
    EMO_CELEBRATE,
    EMO_SLEEPY,
};

#define EMOTION_MSG_MAX     20      /* per the brief */
#define EMOTION_MAX_SECONDS 300     /* clamp: a typo must not hide the clock */

struct EmotionRequest {
    uint8_t  state;
    uint16_t duration_s;
    char     message[EMOTION_MSG_MAX + 1];
};

const char *emotion_name(uint8_t state);
int         emotion_from_name(const char *name);   /* -1 when unknown */

/* Parse a JSON body. Returns false and fills `err` on bad input — callers
 * turn that into a 400 rather than a reboot. */
bool emotion_parse(const char *json, EmotionRequest *out, char *err, size_t errcap);

/* Callable from any task. Returns false if the queue is full. */
bool emotion_post(const EmotionRequest &req);

/* LVGL task only. */
void emotion_begin(void);
void emotion_tick(void);
bool emotion_active(void);
