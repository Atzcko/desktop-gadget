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

/*
 * Emotions are points in a CONTINUOUS space, not a fixed list.
 *
 * Grounded in the literature rather than invented:
 *   Ekman (1992)              6 basic emotions
 *   Plutchik                  8 primaries in 4 opposing pairs
 *   Cowen & Keltner (2017)    27 categories, explicitly "bridged by
 *                             continuous gradients" rather than discrete
 *   Russell (1980) circumplex any emotion = a point on two axes,
 *                             VALENCE (unpleasant..pleasant) and
 *                             AROUSAL  (calm..activated)
 *
 * The circumplex is what makes a single line viable as a display:
 *
 *   arousal  -> how agitated the line is  (amplitude, frequency, speed)
 *   valence  -> its hue                   (red/magenta .. cyan/green)
 *
 * So the renderer implements the SPACE, and the table below is just a set of
 * named points in it. Adding an emotion is one row — never new animation
 * code — and blends between emotions are meaningful because the space is
 * continuous.
 */

/* How the line carries itself, layered on top of valence/arousal. */
enum EmotionCharacter : uint8_t {
    CH_SMOOTH = 0,   /* clean sine                                  */
    CH_JAGGED,       /* squared-off alternating spikes              */
    CH_TREMOR,       /* fast shallow judder over a slow carrier     */
    CH_SCAN,         /* a bright swell travelling along the line    */
    CH_DROOP,        /* asymmetric — sags below the axis            */
};

struct EmotionDef {
    const char *name;
    float       valence;    /* -1 unpleasant .. +1 pleasant */
    float       arousal;    /*  0 calm       ..  1 activated */
    uint8_t     character;
    bool        spectrum;   /* run the full hue wheel (celebration only) */
};

/* Index 0 is "none". Everything else is a live emotion. */
extern const EmotionDef EMOTIONS[];
extern const int        EMOTION_COUNT;

/* Was 20 (the brief's figure). Raised because 20 characters is enough for a
 * filename but not for context — "ui.cpp" does not say what is being done to
 * it. The caption now prepends the state, and wraps to two lines. */
#define EMOTION_MSG_MAX     48
#define EMOTION_MAX_SECONDS 300     /* clamp: a typo must not hide the clock */

struct EmotionRequest {
    uint8_t  state;
    uint16_t duration_s;
    char     message[EMOTION_MSG_MAX + 1];
};

const char       *emotion_name(uint8_t state);
int               emotion_from_name(const char *name);   /* -1 when unknown */
const EmotionDef &emotion_def(uint8_t state);

/* Parse a JSON body. Returns false and fills `err` on bad input — callers
 * turn that into a 400 rather than a reboot. */
bool emotion_parse(const char *json, EmotionRequest *out, char *err, size_t errcap);

/* Callable from any task. Returns false if the queue is full. */
bool emotion_post(const EmotionRequest &req);

/* LVGL task only. */
void emotion_begin(void);
void emotion_tick(void);
bool emotion_active(void);

/* Current expression and time left, for /health. Makes "does it auto-revert?"
 * an observable fact rather than something only a human staring at the panel
 * can confirm. */
uint8_t  emotion_current_state(void);
uint32_t emotion_remaining_s(void);
