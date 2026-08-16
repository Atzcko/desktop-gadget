/**
 * version.h — the single source of truth for the firmware version.
 *
 * Semantic versioning:
 *   MAJOR  a breaking change to the HTTP/BLE API or the settings schema —
 *          something an existing client or a provisioned device would notice
 *   MINOR  a new capability (a transport, a screen, a feature)
 *   PATCH  fixes and tuning that change nothing anyone integrates against
 *
 * Bump this in the SAME commit as the change, then tag the release:
 *
 *     git tag -a v1.0.0 -m "..."   &&   git tag -l
 *
 * The version is reported in three places so it can always be checked against
 * what is actually running on the device rather than what is in the tree:
 * the boot log, GET /health, and Settings > Info.
 */
#pragma once

#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  1
#define FW_VERSION_PATCH  0

#define FW_STR_(x)   #x
#define FW_STR(x)    FW_STR_(x)
#define FW_VERSION   FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)

/* Compiler-stamped, so a stale flash is obvious even at the same version. */
#define FW_BUILD     __DATE__ " " __TIME__
