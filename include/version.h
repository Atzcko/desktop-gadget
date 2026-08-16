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
#define FW_VERSION_MINOR  7
#define FW_VERSION_PATCH  6

#define FW_STR_(x)   #x
#define FW_STR(x)    FW_STR_(x)
#define FW_VERSION   FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)

/*
 * Two stamps, because one is not enough.
 *
 * FW_BUILD is __DATE__/__TIME__, which the compiler bakes in when THIS header's
 * including translation unit is compiled. Change any other file and it goes
 * stale while the binary genuinely changed — the exact failure it existed to
 * catch. Kept because it is still useful when main.cpp is what changed.
 *
 * FW_GIT is injected as a build flag by scripts/version_stamp.py, so it cannot
 * go stale: a changed flag forces a rebuild, and it names the source revision
 * precisely instead of approximately. "+dirty" means uncommitted changes.
 */
#define FW_BUILD     __DATE__ " " __TIME__

#ifndef FW_GIT
#define FW_GIT       "unknown"
#endif
