# Graph Report - Desktop gadget  (2026-09-11)

## Corpus Check
- 212 files · ~363,405 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2779 nodes · 9791 edges · 92 communities
- Extraction: 79% EXTRACTED · 21% INFERRED · 0% AMBIGUOUS · INFERRED: 2052 edges (avg confidence: 0.86)
- Token cost: 937,183 input · 0 output

## Community Hubs (Navigation)
- Lua Garbage Collector
- Lua String Library
- Lua C API
- Games and Game Boy
- Lua Parser
- Lua Code Generator
- YouTube App
- Project Log Lessons
- Lua Debug and Errors
- Gesture and Navigation Decisions
- Remote Browser
- App Host and Drawer
- Lua Math Library
- Releases and HTTP API
- Clock UI and Line Mode
- Lua Script Runtime
- Lua Auxiliary Library
- Vault Index and Emotion API
- Lua I/O Library
- Lua Function Calls
- Messages App
- Orientation, Chrome and Themes
- Lua Debug Library
- Lua Tables
- Board and Display Library
- Agent Instructions and Constraints
- Scripting and App Contract
- Lua Virtual Machine
- Lua Base Library
- Lua Expression Codegen
- Lua Package Loader
- Lua Expression Parsing
- Settings Screen
- Timer App
- Lua Core Headers
- Mac Companion Decisions
- Lua Bytecode Loader
- Lua State Lifecycle
- BLE HID Decisions
- Lua Lexer
- Lab Pin Safety
- Lab App
- HTTP API Handlers
- Lua Object Utilities
- Equalizer App
- Crash Readback and Rebuilds
- Lua Metamethods
- Emotion Engine and Health
- Lua Table Library
- Lua UTF-8 Library
- Lua OS Library
- Audio Capture Helper
- Main Loop and Battery Gauge
- Wi-Fi and Weather
- Lua Bytecode Dumper
- Equalizer Companion Relay
- Line-Mode Mini Cards
- Lua String Interning
- Digits and Card Geometry
- Boot and Live Settings Apply
- Lua Coroutines
- Module Map and Radio Order
- flipclock.py Client
- Buffers, Fold and Burn-in
- Keyboard App
- Lua Memory Allocation
- Trackpad App
- Flip Card Rendering
- Settings Storage (NVS)
- Settings Rows and Navigation
- ytserve HTTP Handler
- YouTube Video Struct
- JPEG Context (YouTube)
- JPEG Context (Browser)
- BLE Server Callbacks
- YouTube Fetched Item
- Lua Operator Parsing
- TJpgDec Callbacks

## God Nodes (most connected - your core abstractions)
1. `Project log` - 169 edges
2. `Desktop gadget - Fliqlo flip clock (index)` - 80 edges
3. `Releases` - 79 edges
4. `luaL_error()` - 63 edges
5. `lua_pushinteger()` - 45 edges
6. `lua_pushvalue()` - 44 edges
7. `lua_Integer()` - 43 edges
8. `D037 - Apps become Lua scripts` - 41 edges
9. `Module map` - 41 edges
10. `index2value()` - 38 edges

## Surprising Connections (you probably didn't know these)
- `POSIX TZ string <+04>-4 — two things that look like typos and are not` --references--> `net_begin()`  [INFERRED]
  docs/decisions/D015 - Abu Dhabi locale.md → src/net.cpp
- `Weather glyphs drawn with primitives; a moon after dark — v1.7.0 / v1.8.0` --references--> `icon`  [INFERRED]
  docs/RELEASES.md → src/ui.cpp
- `lua_panic()` --semantically_similar_to--> `panic()`  [INFERRED] [semantically similar]
  src/script.cpp → lib/lua/src/lauxlib.c
- `Rule 3: decor() every decorative object` --references--> `decor()`  [INFERRED]
  docs/decisions/D026 - Apps are a platform, not a special case.md → src/app_host.cpp
- `D029 - Back is a system gesture, not a widget event` --references--> `app_host_home()`  [INFERRED]
  docs/decisions/D029 - Back is a system gesture, not a widget event.md → src/app_host.cpp

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Routes assessed for adding an app without reflashing the image** — docs_the_os_direction_ota_full_images_route, docs_the_os_direction_dynamically_loaded_native_elf_route, docs_the_os_direction_embedded_scripting_runtime_route, docs_decisions_d034_updates_ship_over_the_air, docs_decisions_d037_apps_become_lua_scripts [EXTRACTED 1.00]
- **The decor() hit-testing rule and everything that inherits it** — docs_decisions_d014_touch_hit_testing_decor_helper, docs_decisions_d014_touch_hit_testing_screen_object_scroll_chain_bug, src_ui_decor, src_app_host_decor, claude_native_app_contract, docs_releases_drawer_scroll_chain_fix [INFERRED 0.85]
- **The desk-clock narration protocol (what Claude shows while it works)** — claude_activity_states, claude_message_is_an_object_never_a_verb, claude_prefix_the_work_command, claude_short_durations_refreshed, claude_hush_as_the_last_action, claude_keep_it_lit_while_you_work, claude_flipclock_global_command, tools_say, tools_hush [EXTRACTED 1.00]
- **Every emotion transport lands in the same emotion_post() queue** — docs_decisions_d018_emotion_api_one_engine_two_transports_emotion_queue, docs_decisions_d018_emotion_api_one_engine_two_transports_nordic_uart_service, docs_decisions_d022_custom_hid_identity_not_a_keyboard_hid_output_report_emotion_protocol, docs_decisions_d034_updates_ship_over_the_air_device_narrates_its_own_update, src_emotion_emotion_post, src_httpapi, src_ble [EXTRACTED 1.00]
- **Nothing stays resident: create on entry, destroy on exit** — docs_decisions_d026_apps_are_a_platform_not_a_special_case_create_on_entry_destroy_on_exit, docs_decisions_d033_back_goes_one_level_not_home_drawer_rebuilt_on_every_visit, docs_decisions_d037_apps_become_lua_scripts_fresh_lua_state_per_launch, docs_decisions_d035_the_lab_may_only_touch_pins_the_firmware_does_not_own_leave_no_trace, src_ui_settings [EXTRACTED 1.00]
- **Lab pin safety: whitelist, hazard table, GPIO0 input-only, script GPIO bindings** — docs_decisions_d035_the_lab_may_only_touch_pins_the_firmware_does_not_own_pin_whitelist, docs_decisions_d035_the_lab_may_only_touch_pins_the_firmware_does_not_own_owned_pins_hazard_table, docs_decisions_d036_gpio0_is_readable_never_drivable_gpio0_input_only, docs_decisions_d036_gpio0_is_readable_never_drivable_bus_map_not_ordering_convention, docs_decisions_d037_apps_become_lua_scripts_library_whitelist_sandbox, src_apps_app_lab [EXTRACTED 1.00]
- **The Mac companion pattern: every feature the Mac carries for the clock** — docs_decisions_d050_youtube_is_a_dashboard_and_a_remote_not_a_player_companion_ytserve, docs_decisions_d053_the_clock_plays_video_after_all_through_the_mac_mac_transcodes_mjpeg, docs_decisions_d054_home_comes_from_the_owner_s_own_session_home_from_cookies, docs_decisions_d057_the_clock_is_a_browser_through_the_mac_thin_client_browser, docs_decisions_d058_the_equalizer_listens_through_the_mac_mac_captures_own_output, tools_ytserve [EXTRACTED 1.00]
- **Lifecycle changes deferred to the LVGL loop via app_host_request_* and serve_pending()** — docs_decisions_d039_the_device_must_be_drivable_without_a_finger_post_launch_endpoint, docs_decisions_d041_nothing_may_unwind_through_a_live_interpreter_lifecycle_request_rule, docs_decisions_d047_messages_are_conversations_and_the_clock_wears_the_badge_unread_badge, src_app_host_serve_pending, src_app_host_app_host_tick, src_app_host_app_host_request_back, src_app_host_app_host_request_open, src_script_l_back, src_httpapi_handle_launch_body [EXTRACTED 1.00]
- **Remote debugging pair: read the crash (/crash) and reach the trigger (/launch)** — docs_decisions_d038_crashes_must_be_readable_without_a_cable_get_crash_endpoint, docs_decisions_d038_crashes_must_be_readable_without_a_cable_same_build_check, docs_decisions_d039_the_device_must_be_drivable_without_a_finger_post_launch_endpoint, tools_crash, docs_decisions_d040_a_script_must_not_be_able_to_reboot_the_clock_lua_atpanic_guard [EXTRACTED 1.00]
- **Boot order: settings -> panel -> rotation -> LVGL -> UI -> emotion -> BLE -> Wi-Fi** — src_main_setup, src_settings_settings_load, docs_reference_lilygo_amoled_library_beginamoled_241, docs_reference_lilygo_amoled_library_setrotation, docs_reference_lilygo_amoled_library_beginlvglhelper, src_ui_ui_init, src_emotion_emotion_begin, src_ble_ble_begin, src_net_net_begin [EXTRACTED 1.00]
- **Tasks and workers that never touch LVGL: validate, enqueue or flag; the LVGL loop draws** — docs_reference_module_map_only_the_lvgl_task_draws, docs_stages_stage_3___weather_weather_task, docs_stages_stage_4___emotion_api_async_handler_queue, src_httpapi, src_ble, src_msg, src_yt, src_browser, src_eq, src_script [EXTRACTED 1.00]
- **Native apps and the Lua adapter implementing the App contract** — include_app_api_app, src_apps_app_timer, src_apps_app_settings, src_apps_app_lab, src_apps_app_messages, src_apps_app_themes, src_apps_app_youtube, src_apps_app_browser, src_apps_app_equalizer, src_apps_app_trackpad, src_apps_app_keyboard, src_apps_app_games, src_script [INFERRED 0.95]

## Communities (92 total, 0 thin omitted)

### Community 0 - "Lua Garbage Collector"
Cohesion: 0.05
Nodes (118): Dyndata, l_mem, lua_gc(), closepaux(), CClosure, LClosure, lua_State, Proto (+110 more)

### Community 1 - "Lua String Library"
Cohesion: 0.07
Nodes (88): Header, KOption, lua_tolstring(), luaL_addvalue(), luaL_buffinitsize(), luaL_checklstring(), luaL_error(), luaL_optinteger() (+80 more)

### Community 2 - "Lua C API"
Cohesion: 0.08
Nodes (84): aux_rawset(), aux_upvalue(), auxgetstr(), auxsetstr(), GCObject, l_sinline, LClosure, lua_Alloc (+76 more)

### Community 3 - "Games and Game Boy"
Cohesion: 0.05
Nodes (74): Thumbnails decode on the worker task via the ROM's TJpgDec, Silent, and said so: no DAC, amp or speaker, Canvas path (Snake) and object path (Breakout) as engine templates, One Games app: a chooser, then the game, with the in-app back stack, Touch grammar per game, no buttons on the field, D056 - A real Game Boy lives in the arcade, Cartridges are files: POST /rom, the /roms shelf, per-cart battery saves, Screen doubled to 320x288 on a canvas in DMG green, pre-swapped for LV_COLOR_16_SWAP (+66 more)

### Community 4 - "Lua Parser"
Cohesion: 0.11
Nodes (79): BlockCnt, Labeldesc, Labellist, l_noret, LexState, luaK_semerror(), luaX_next(), luaX_syntaxerror() (+71 more)

### Community 5 - "Lua Code Generator"
Cohesion: 0.10
Nodes (69): addk(), boolF(), boolT(), FuncState, Instruction, lua_Number, Proto, TString (+61 more)

### Community 6 - "YouTube App"
Cohesion: 0.08
Nodes (62): The companion: tools/ytserve on the Mac (:8999), Official Data API v3 only; the key is provisioned like Wi-Fi credentials, Tap plays on the clock; 'On Mac' is demoted to an option, lv_event_t, lv_obj_t, decor(), list_build(), ov_kb_cb() (+54 more)

### Community 7 - "Project Log Lessons"
Cohesion: 0.06
Nodes (64): GET /health, D005 - PlatformIO runs on Python 3.12, PlatformIO in its own Python 3.12 venv, D006 - Keep build artifacts out of iCloud, build_dir redirected out of iCloud, Ripple: the split-flap fold with unchanged digits, D023 - Emotions must change the mode, not decorate it, Minimalist is not the same as faint (+56 more)

### Community 8 - "Lua Debug and Errors"
Cohesion: 0.10
Nodes (63): Closure, auxgetinfo(), basicgetobjname(), CallInfo, Instruction, l_noret, LUA_API, lua_Debug (+55 more)

### Community 9 - "Gesture and Navigation Decisions"
Cohesion: 0.07
Nodes (62): The clock is not an app, D014 - Touch hit-testing, Dedicated full-screen text editor overlay, D026 - Apps are a platform, not a special case, A drawer rather than double-tap, The clock is not an app: it is the resting state, D027 - The gesture budget, Inside apps there is no budget problem (+54 more)

### Community 10 - "Remote Browser"
Cohesion: 0.06
Nodes (50): GET /browse/stream (pixels down) and POST /browse/input (input up), Browser app: toolbar, page view, touch layer, keyboard overlays, browser_back_nav(), browser_create(), browser_destroy(), browser_icon(), browser_tick(), lv_event_cb_t (+42 more)

### Community 11 - "App Host and Drawer"
Cohesion: 0.08
Nodes (55): Native App contract (app_api.h), Never delete a screen from inside an event on that screen, The layout is the model: lv_obj_move_to_index reorders without rebuilding, The drawer is rebuilt on every visit, not kept resident, One door: everything routes through app_host_back(), The registry stopped being an array: app_count()/app_at(), Drawer wraps and scrolls vertically, POST /launch endpoint (+47 more)

### Community 12 - "Lua Math Library"
Cohesion: 0.11
Nodes (59): lua_Number, lua_isinteger(), lua_pushinteger(), lua_pushnumber(), luaL_checknumber(), lua_Number, lua_State, LUAMOD_API (+51 more)

### Community 13 - "Releases and HTTP API"
Cohesion: 0.07
Nodes (56): BatteryState, Only the LVGL task touches LVGL objects, OTA flash workflow (tools/ota), D018 - Emotion API - one engine, two transports, D034 - Updates ship over the air, The second OTA slot was always there, POST /update writes a raw firmware image to the inactive OTA slot, Raw body, not multipart (+48 more)

### Community 14 - "Clock UI and Line Mode"
Cohesion: 0.06
Nodes (52): Emotion mode layout: the clock steps aside, Entrance/exit driven by a gain ramp inside the wave timer, not lv_anim, D025 - Scaling text LVGL cannot scale, Render the clock into an lv_canvas and zoom that, If ps_malloc cannot provide the canvas, fall back to the cross-fade, LVGL 8 cannot transform text, lv_snapshot is blocked by the library's lv_conf.h, Verify which config a library actually sees (+44 more)

### Community 15 - "Lua Script Runtime"
Cohesion: 0.08
Nodes (55): Lua app script contract, Compile at upload, not at launch, lua_atpanic longjmp guard around script init, -DLUA_USE_C89=0 turned C89 mode on, Launching a Lua app rebooted the device — v1.20.1, Lua 5.4 runtime beside the native apps — v1.19.0, Route: embedded scripting runtime, Scripting runtime candidates: Berry, Lua, MicroPython (+47 more)

### Community 16 - "Lua Auxiliary Library"
Cohesion: 0.10
Nodes (53): lua_absindex(), lua_pushfstring(), lua_setwarnf(), boxgc(), FILE, lua_Debug, lua_Number, lua_State (+45 more)

### Community 17 - "Vault Index and Emotion API"
Cohesion: 0.07
Nodes (54): CLAUDE.md, D008 - Brightness scale, Brightness defaults: day 90 (≈35 %), night 25 (≈10 %), night 22:00–07:00, D010 - Night dimming is a schedule, not a sensor, The T4-S3 has no ambient light sensor, D020 - Humidity recedes by contrast, not size, Contrast is the free axis once type sizes are spent, Humidity toggle (NVS key hum) and /health humidity_pct (+46 more)

### Community 18 - "Lua I/O Library"
Cohesion: 0.12
Nodes (52): luaL_fileresult(), aux_close(), aux_lines(), FILE, lua_State, LUAMOD_API, createmeta(), createstdfile() (+44 more)

### Community 19 - "Lua Function Calls"
Cohesion: 0.12
Nodes (50): l_uint32, CallInfo, l_noret, l_sinline, LUA_API, lua_KContext, lua_KFunction, lua_State (+42 more)

### Community 20 - "Messages App"
Cohesion: 0.09
Nodes (47): Identity = the device name; _gadget-msg._tcp with name=<identity> in TXT, Messaging worker task with a command queue; the web task only calls msg_store(), Contact book learns name->address three ways, Conversations, not an inbox, Unread badge pill on the clock, tappable, MsgEntry, MsgPeer, age_str() (+39 more)

### Community 21 - "Orientation, Chrome and Themes"
Cohesion: 0.07
Nodes (45): Device gestures, Flip 180 is rotation 2, the other landscape (v1.9.0 addendum), Rotation 0 is natively 600×450 landscape, decor() — strip CLICKABLE, SCROLLABLE and SCROLL_CHAIN from anything purely visual, The clock could be dragged around: the screen object is on the scroll chain, D043 - Orientation is the clock's job; apps borrow landscape, Apps borrow landscape, Three orientation tiers (+37 more)

### Community 22 - "Lua Debug Library"
Cohesion: 0.11
Nodes (48): lua_createtable(), lua_pushthread(), lua_pushvalue(), lua_setfield(), lua_xmove(), luaL_checkinteger(), luaL_getsubtable(), luaL_requiref() (+40 more)

### Community 23 - "Lua Tables"
Cohesion: 0.15
Nodes (47): luaO_ceillog2(), arrayindex(), binsearch(), l_sinline, lua_Number, lua_State, Node, StkId (+39 more)

### Community 24 - "Board and Display Library"
Cohesion: 0.09
Nodes (46): D004 - Rotation 0 is already landscape, D007 - Skip SD, keep charge LED default, The SY6970 status LED reports a fault, not power (reversal), beginAMOLED_241 with disable_sd = true, BOARD_AMOLED_241 board definition (LILYGO_AMOLED_241), CST226SE capacitive touch (I2C, SDA 6 / SCL 7, IRQ 8, RST 17), ESP32-S3R8 (16 MB QIO flash, 8 MB OPI PSRAM, default_16MB.csv), Internal I2C bus (SDA 6 / SCL 7): PMU + touch (+38 more)

### Community 25 - "Agent Instructions and Constraints"
Cohesion: 0.10
Nodes (36): Activity states, Emotion API, flipclock global command, Hard constraints, tools/hush as the last action of every turn, Keep it lit while you work, message is an object, never a verb, Prefix the work command with tools/say (+28 more)

### Community 26 - "Scripting and App Contract"
Cohesion: 0.09
Nodes (41): on_create(), on_exit(), on_tick(), paint(), pin(), select_pin(), Never call LVGL from a non-LVGL task, The App contract: name, icon, create, destroy, tick (+33 more)

### Community 27 - "Lua Virtual Machine"
Cohesion: 0.14
Nodes (42): F2Imod, lua_Integer(), intarith(), l_sinline, lua_Number, lua_State, Proto, StkId (+34 more)

### Community 28 - "Lua Base Library"
Cohesion: 0.14
Nodes (41): lua_gettop(), lua_pushnil(), lua_settop(), lua_type(), luaL_checkany(), luaL_checktype(), b_str2int(), lua_KContext (+33 more)

### Community 29 - "Lua Expression Codegen"
Cohesion: 0.15
Nodes (42): binopr2op(), binopr2TM(), BinOpr, expdesc, l_sinline, TMS, TValue, UnOpr (+34 more)

### Community 30 - "Lua Package Loader"
Cohesion: 0.16
Nodes (39): lua_getfield(), lua_pushlstring(), lua_pushstring(), luaL_Buffer, luaL_addgsub(), luaL_addlstring(), luaL_addstring(), luaL_buffinit() (+31 more)

### Community 31 - "Lua Expression Parsing"
Cohesion: 0.13
Nodes (37): ConsControl, expkind, isKstr(), luaK_exp2anyregup(), luaK_indexed(), allocupvalue(), expdesc, FuncState (+29 more)

### Community 32 - "Settings Screen"
Cohesion: 0.12
Nodes (32): A Save button must save everything on the screen, Save and Close are separate, with a guard — v1.2.0, Settings on its own LVGL screen (lv_obj_create(NULL)), deleted on close, net_apply_wifi(), net_scan(), clear_bonds_cb(), close_cb(), confirm_cb() (+24 more)

### Community 33 - "Timer App"
Cohesion: 0.12
Nodes (29): back_cb(), build_card(), bump(), clear_busy(), lv_anim_t, lv_coord_t, lv_event_cb_t, lv_event_t (+21 more)

### Community 34 - "Lua Core Headers"
Cohesion: 0.15
Nodes (7): Lua 5.4.7 vendored verbatim into lib/lua/, lua_Reader, lua_State, ZIO, luaZ_fill(), luaZ_init(), luaZ_read()

### Community 35 - "Mac Companion Decisions"
Cohesion: 0.17
Nodes (28): App drawer, D050 - YouTube is a dashboard and a remote, not a player, Popular and Search modes (v1.31.0), On-device playback walled off three ways, D053 - The clock plays video after all, through the Mac, Companion pattern: the Mac does what the device cannot; the device shows honest pixels, The Mac transcodes to MJPEG; the clock decodes the latest complete frame, D054 - Home comes from the owner's own session (+20 more)

### Community 36 - "Lua Bytecode Loader"
Cohesion: 0.21
Nodes (30): luaD_inctop(), l_noret, LClosure, lu_byte, lua_Number, lua_State, Proto, TString (+22 more)

### Community 37 - "Lua State Lifecycle"
Cohesion: 0.16
Nodes (28): CallInfo, global_State, lua_Alloc, LUA_API, lua_State, close_state(), f_luaopen(), freeCI() (+20 more)

### Community 38 - "BLE HID Decisions"
Cohesion: 0.15
Nodes (28): Nordic UART Service (NUS) as the BLE transport, D021 - BLE HID, for discoverability not typing, 31-byte advertising budget, A keyboard that never types: HID purely for listing, Mandatory pairing, Just Works, no passkey, Settings > BLE > Pairable and Clear pairings, D022 - Custom HID identity, not a keyboard, PnP identity: USB-IF source, Espressif VID 0x303A, PID 0x4001 (+20 more)

### Community 39 - "Lua Lexer"
Cohesion: 0.22
Nodes (28): l_noret, LexState, lua_State, TString, ZIO, check_next1(), check_next2(), esccheck() (+20 more)

### Community 40 - "Lab Pin Safety"
Cohesion: 0.17
Nodes (28): Deliberately absent: authentication, rollback, resume, D035 - The Lab may only touch pins the firmware does not own, Pins the firmware owns and what writing to them does, The Lab pin whitelist, UART is Serial1, never Serial, D036 - GPIO0 is readable, never drivable, bus_map[] instead of an ordering convention on PINS[], A pin with a button hard-wired to ground must never be an output (+20 more)

### Community 41 - "Lab App"
Cohesion: 0.17
Nodes (25): build_bus_map(), build_gpio_tab(), build_i2c_tab(), build_uart_tab(), bus_gpio(), lv_event_t, lv_obj_t, decor() (+17 more)

### Community 42 - "HTTP API Handlers"
Cohesion: 0.18
Nodes (25): AsyncWebServerRequest, POST /msg, GET /messages, POST /send, HTTP into the existing async server + mDNS-SD discovery, The unknown-state error interpolated the value in double quotes, producing invalid JSON; single quotes plus a defensive json_escape() in send_err(), app_name_param(), String, handle_apps_body(), handle_apps_delete() (+17 more)

### Community 43 - "Lua Object Utilities"
Cohesion: 0.20
Nodes (25): BuffFS, addnum2buff(), addstr2buff(), lua_Number, lua_State, StkId, TValue, va_list (+17 more)

### Community 44 - "Equalizer App"
Cohesion: 0.15
Nodes (23): D058 - The equalizer listens through the Mac, eq.cpp + Equalizer app: 32 levels, smoothing, gravity peak caps, themed, GET /eq/stream (companion): hex band lines relayed while a client listens; ?demo=1 / EQ_FORCE_DEMO=1; "!" line = status for the clock, GET /eq (streaming, frames, status), An equalizer for whatever the Mac is playing — v1.39.0, Equalizer (v1.39.0): the Mac captures its own output with ScreenCaptureKit, FFT in Accelerate, 32 band levels at 30 fps as hex lines (2 KB/s); the clock draws theme bars with gravity peak caps, The SCStream was a local in the startup Task and was released when the task ended; a released SCStream stops silently, lv_event_t (+15 more)

### Community 45 - "Crash Readback and Rebuilds"
Cohesion: 0.14
Nodes (23): Crash readout without a cable (tools/crash), LV_EVENT_FOCUSED is an edge, not a level, D031 - The layout is the model, LV_EVENT_CLICKED fires after a long press too, Tap handlers bind LV_EVENT_SHORT_CLICKED, D038 - Crashes must be readable without a cable, Drawer disqualifies moved presses, GET /crash and DELETE /crash endpoints (+15 more)

### Community 46 - "Lua Metamethods"
Cohesion: 0.24
Nodes (23): CallInfo, lua_State, Proto, StkId, Table, TMS, TString, TValue (+15 more)

### Community 47 - "Emotion Engine and Health"
Cohesion: 0.13
Nodes (20): Emotion queue: parse on any task, render on the LVGL task, Emotion input validation: clean 400, never a reboot, 32-byte HID output report wired to the emotion engine, The device narrates its own update through the emotion queue, GET /health reports the current screen, EmotionRequest, NimBLECharacteristic, NimBLECharacteristicCallbacks (+12 more)

### Community 48 - "Lua Table Library"
Cohesion: 0.23
Nodes (22): IdxT, lua_geti(), lua_seti(), addfield(), auxsort(), lua_State, luaL_Buffer, LUAMOD_API (+14 more)

### Community 49 - "Lua UTF-8 Library"
Cohesion: 0.17
Nodes (19): lua_toboolean(), lua_State, LUALIB_API, luaL_openlibs(), byteoffset(), lua_State, LUAMOD_API, codepoint() (+11 more)

### Community 50 - "Lua OS Library"
Cohesion: 0.19
Nodes (22): lua_State, LUAMOD_API, checkoption(), getboolfield(), getfield(), l_checktime(), luaopen_os(), os_clock() (+14 more)

### Community 51 - "Audio Capture Helper"
Cohesion: 0.13
Nodes (18): Accelerate, CMSampleBuffer, CoreMedia, tools/eq_capture (Swift): ScreenCaptureKit capture + Accelerate FFT, A released SCStream stops silently: retain gStream, Error, Float, Foundation (+10 more)

### Community 52 - "Main Loop and Battery Gauge"
Cohesion: 0.16
Nodes (19): Piecewise-linear resting LiPo voltage-to-percent curve, BOOT cycles orientation; settings.rotation; POST /rotate, gauge_update() is the seam for a real fuel-gauge IC, Hybrid gauge (gauge.cpp): coulombs while charging, model while discharging, lv_timer_handler() (LVGL), gauge_begin(), gauge_ma(), gauge_mah_used() (+11 more)

### Community 53 - "Wi-Fi and Weather"
Cohesion: 0.12
Nodes (21): 'weather 496370 h ago' — v1.10.1, Criterion 4: Wi-Fi drop degrades gracefully and reconnects (PASS, field evidence), Non-blocking Wi-Fi: net_begin() returns immediately; GOT_IP kicks configTzTime(); STA_DISCONNECTED reconnects, GeoResult, Wi-Fi reason-code decoding + scheduled backoff exposed 15 4WAY_HANDSHAKE_TIMEOUT then 202 AUTH_FAIL: the password was wrong, NetStatus, String, https_get() (+13 more)

### Community 54 - "Lua Bytecode Dumper"
Cohesion: 0.29
Nodes (21): DumpState, lua_Number, lua_State, lua_Writer, Proto, TString, dumpBlock(), dumpByte() (+13 more)

### Community 55 - "Equalizer Companion Relay"
Cohesion: 0.13
Nodes (9): GET /eq/stream relay on the companion, with a demo seam, Capture, _demo_line(), ensure_built(), tools/eq_session.py — system-audio spectrum for the Equalizer app (D058). The…, Two wandering resonances, a 120 bpm kick in the lows, a little noise., Compile the helper when it is missing or older than its source., Release the helper (and the mic-style capture) a few seconds after the last… (+1 more)

### Community 56 - "Line-Mode Mini Cards"
Cohesion: 0.15
Nodes (19): Enclosure, 3D print STL exports (DG_Case, DG_Buttons, DG_Switch), Case constraints inherited from the firmware, Fusion project 'Desktop Gadget' / ESPRT01 Case, Fusion document search silently caps at 15 results, The temperature card vanished — a silently failed scripted edit — v1.7.5, Line mode strip: clock and weather as five 46 px mini cards in one row; fliqlo_small (38 px), fliqlo_mid (44 px), fliqlo_corner (50 px) compiled faces, lv_color_t (+11 more)

### Community 57 - "Lua String Interning"
Cohesion: 0.27
Nodes (18): lua_State, TString, Udata, createstrobj(), growstrtab(), internshrstr(), luaS_createlngstrobj(), luaS_eqlngstr() (+10 more)

### Community 58 - "Digits and Card Geometry"
Cohesion: 0.18
Nodes (15): lv_rounder_cb snaps flush areas to even coordinates, D011 - Generate the digit font, do not scale Montserrat, LV_FONT_DECLARE must be wrapped in extern "C", fliqlo_digits — a 210 px, ten-glyph LVGL font generated with lv_font_conv, Tabular figures, for free, D012 - Card geometry, Card geometry constants (CARD_W 268, CARD_H 232, GAP 36, RADIUS 26, SEAM_H 3, COLON_DOT 14), Every value wears the same card — v1.1.0 (+7 more)

### Community 59 - "Boot and Live Settings Apply"
Cohesion: 0.18
Nodes (18): ble_apply_name(), ble_stop(), app_apply_brightness(), app_refresh_clock(), setup(), net_apply_timezone(), net_request_weather_now(), cycle_brightness() (+10 more)

### Community 60 - "Lua Coroutines"
Cohesion: 0.29
Nodes (16): lua_pushboolean(), auxresume(), auxstatus(), lua_State, LUAMOD_API, getco(), luaB_auxwrap(), luaB_close() (+8 more)

### Community 61 - "Module Map and Radio Order"
Cohesion: 0.23
Nodes (16): D017 - BLE and Wi-Fi coexistence, Decode the backtrace, do not guess, BLE must start before Wi-Fi (ble_begin before net_begin), NimBLE over Bluedroid, ESP32-S3 shares one 2.4 GHz radio between Wi-Fi and BLE, Wi-Fi modem sleep is mandatory once Bluetooth is enabled, Add the diagnostic before rewriting, BLE before Wi-Fi ordering (+8 more)

### Community 62 - "flipclock.py Client"
Cohesion: 0.25
Nodes (11): BLE Nordic UART Service transport, ble_scan(), ble_send(), _ble_send(), emote(), http_health(), http_post(), main() (+3 more)

### Community 63 - "Buffers, Fold and Burn-in"
Cohesion: 0.22
Nodes (13): Every build flag is load-bearing, D009 - LVGL buffer strategy, Single full-screen framebuffer in PSRAM, D015 - Abu Dhabi locale, POSIX TZ string <+04>-4 — two things that look like typos and are not, Flip driven by "rendered digit differs from target", per card, First paint after NTP does not animate (animate = last_min >= 0), configTzTime(TZ_POSIX, "pool.ntp.org") with daily resync (+5 more)

### Community 64 - "Keyboard App"
Cohesion: 0.27
Nodes (11): Stock LVGL keyboard with no textarea: keys go to the host, ascii_to_hid(), lv_event_t, lv_obj_t, kb_cb(), kbd_create(), kbd_icon(), kbd_tick() (+3 more)

### Community 65 - "Lua Memory Allocation"
Cohesion: 0.33
Nodes (12): global_State, l_noret, lua_State, firsttry(), luaM_free_(), luaM_growaux_(), luaM_malloc_(), luaM_realloc_() (+4 more)

### Community 66 - "Trackpad App"
Cohesion: 0.26
Nodes (11): app_touch_count() reads the CST226's raw point count, Laptop-pad gesture set, lv_event_t, lv_obj_t, pad_cb(), state_paint(), tp_create(), tp_destroy() (+3 more)

### Community 67 - "Flip Card Rendering"
Cohesion: 0.24
Nodes (11): clip_corner set on the card for the fold, The seam is scale-dependent, not all-or-nothing, Seam rule becomes scale-dependent — v1.4.1 / v1.4.2 / v1.4.3, Card, label, root, seam, text (+3 more)

### Community 68 - "Settings Storage (NVS)"
Cohesion: 0.38
Nodes (10): First-boot sentinel key silences Preferences' NOT_FOUND noise, NVS reads guarded by isKey() with a one-shot backfill; first-boot sentinel seeding, apply_defaults(), copy_str(), have(), settings_dump(), settings_load(), settings_reset() (+2 more)

### Community 69 - "Settings Rows and Navigation"
Cohesion: 0.49
Nodes (10): Settings row rules: setting_row / row_control / slider_row / section, Settings navigates like Apple's and nothing in it scrolls sideways — v1.26.0 / v1.27.1–v1.27.3, body_label(), lv_obj_t, hours_options(), row_control(), section(), setting_row() (+2 more)

### Community 70 - "ytserve HTTP Handler"
Cohesion: 0.39
Nodes (3): BaseHTTPRequestHandler, get_browser(), H

### Community 71 - "YouTube Video Struct"
Cohesion: 0.29
Nodes (6): YtVideo, age, channel, id, thumb, title

### Community 72 - "JPEG Context (YouTube)"
Cohesion: 0.29
Nodes (7): JpgCtx, data, len, oh, out, ow, pos

### Community 73 - "JPEG Context (Browser)"
Cohesion: 0.29
Nodes (7): JpgCtx, data, len, oh, out, ow, pos

### Community 74 - "BLE Server Callbacks"
Cohesion: 0.40
Nodes (4): ble_gap_conn_desc, NimBLEServer, NimBLEServerCallbacks, ServerCallbacks

### Community 75 - "YouTube Fetched Item"
Cohesion: 0.33
Nodes (6): Fetched, chan, id, iso, thumb_url, title

### Community 76 - "Lua Operator Parsing"
Cohesion: 0.60
Nodes (5): BinOpr, UnOpr, getbinopr(), getunopr(), subexpr()

### Community 77 - "TJpgDec Callbacks"
Cohesion: 0.50
Nodes (4): JDEC, JRECT, jpg_in(), jpg_out()

## Ambiguous Edges - Review These
- `D034 - Updates ship over the air` → `HTTPS with setInsecure()`  [AMBIGUOUS]
  docs/stages/Stage 3 - Weather.md · relation: conceptually_related_to

## Knowledge Gaps
- **55 isolated node(s):** `id`, `title`, `channel`, `age`, `thumb` (+50 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 287 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **What is the exact relationship between `D034 - Updates ship over the air` and `HTTPS with setInsecure()`?**
  _Edge tagged AMBIGUOUS (relation: conceptually_related_to) - confidence is low._
- **Why does `Module map` connect `Module Map and Radio Order` to `YouTube App`, `Project Log Lessons`, `Gesture and Navigation Decisions`, `Remote Browser`, `App Host and Drawer`, `Releases and HTTP API`, `Clock UI and Line Mode`, `Lua Script Runtime`, `Vault Index and Emotion API`, `Messages App`, `Orientation, Chrome and Themes`, `Scripting and App Contract`, `Settings Screen`, `Timer App`, `BLE HID Decisions`, `Lab Pin Safety`, `Lab App`, `HTTP API Handlers`, `Equalizer App`, `Emotion Engine and Health`, `Main Loop and Battery Gauge`, `Wi-Fi and Weather`, `Keyboard App`, `Trackpad App`, `Settings Storage (NVS)`?**
  _High betweenness centrality (0.196) - this node is a cross-community bridge._
- **Why does `Lua 5.4 runtime beside the native apps — v1.19.0` connect `Lua Script Runtime` to `Lua Core Headers`, `Gesture and Navigation Decisions`, `App Host and Drawer`, `Releases and HTTP API`, `Scripting and App Contract`?**
  _High betweenness centrality (0.120) - this node is a cross-community bridge._
- **Why does `Releases` connect `Releases and HTTP API` to `Games and Game Boy`, `Project Log Lessons`, `Gesture and Navigation Decisions`, `App Host and Drawer`, `Clock UI and Line Mode`, `Lua Script Runtime`, `Orientation, Chrome and Themes`, `Board and Display Library`, `Agent Instructions and Constraints`, `Scripting and App Contract`, `Settings Screen`, `Mac Companion Decisions`, `BLE HID Decisions`, `Lab Pin Safety`, `Equalizer App`, `Crash Readback and Rebuilds`, `Wi-Fi and Weather`, `Line-Mode Mini Cards`, `Digits and Card Geometry`, `Flip Card Rendering`, `Settings Rows and Navigation`?**
  _High betweenness centrality (0.115) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `Project log` (e.g. with `D034 - Updates ship over the air` and `D044 - Coulombs where measurable, model where not`) actually correct?**
  _`Project log` has 5 INFERRED edges - model-reasoned connections that need verification._
- **What connects `id`, `title`, `channel` to the rest of the system?**
  _55 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Lua Garbage Collector` be split into smaller, more focused modules?**
  _Cohesion score 0.05284147557328016 - nodes in this community are weakly interconnected._