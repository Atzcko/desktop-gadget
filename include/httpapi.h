/**
 * httpapi.h — ESPAsyncWebServer transport for the emotion API + /health.
 *
 * Handlers run on the async TCP task. They validate, enqueue and return.
 * They never touch LVGL.
 */
#pragma once
void httpapi_begin(void);
