> PRELIMINARY: code half only; documents still extracting.

# Graph Report - Desktop gadget  (2026-09-11)

## Corpus Check
- 212 files · ~363,405 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2156 nodes · 7279 edges · 92 communities (63 shown, 7 thin omitted)
- Extraction: 79% EXTRACTED · 21% INFERRED · 0% AMBIGUOUS · INFERRED: 1538 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Community 0
- Community 1
- Community 2
- Community 3
- Community 4
- Community 5
- Community 6
- Community 7
- Community 8
- Community 9
- Community 10
- Community 11
- Community 12
- Community 13
- Community 14
- Community 15
- Community 16
- Community 17
- Community 18
- Community 19
- Community 20
- Community 21
- Community 22
- Community 23
- Community 24
- Community 25
- Community 26
- Community 27
- Community 28
- Community 29
- Community 30
- Community 31
- Community 32
- Community 33
- Community 34
- Community 35
- Community 36
- Community 37
- Community 38
- Community 39
- Community 40
- Community 41
- Community 42
- Community 43
- Community 44
- Community 45
- Community 46
- Community 47
- Community 48
- Community 49
- Community 50
- Community 51
- Community 52
- Community 53
- Community 54
- Community 55
- Community 56
- Community 57
- Community 58
- Community 59
- Community 60
- Community 61
- Community 62
- Community 63
- Community 64
- Community 65
- Community 66
- Community 67
- Community 68
- Community 69

## God Nodes (most connected - your core abstractions)
1. `luaL_error()` - 63 edges
2. `lua_pushinteger()` - 45 edges
3. `lua_pushvalue()` - 44 edges
4. `lua_Integer()` - 43 edges
5. `index2value()` - 38 edges
6. `httpapi_begin()` - 37 edges
7. `lua_pushstring()` - 34 edges
8. `luaL_checkinteger()` - 34 edges
9. `lua_pushnumber()` - 32 edges
10. `lua_setfield()` - 31 edges

## Surprising Connections (you probably didn't know these)
- `l_gpio_write()` --calls--> `lua_toboolean()`  [INFERRED]
  src/script.cpp → lib/lua/src/lapi.c
- `l_ui_hide()` --calls--> `lua_toboolean()`  [INFERRED]
  src/script.cpp → lib/lua/src/lapi.c
- `script_back()` --calls--> `lua_toboolean()`  [INFERRED]
  src/script.cpp → lib/lua/src/lapi.c
- `push_handle()` --calls--> `lua_pushnil()`  [INFERRED]
  src/script.cpp → lib/lua/src/lapi.c
- `l_millis()` --calls--> `lua_pushinteger()`  [INFERRED]
  src/script.cpp → lib/lua/src/lapi.c

## Import Cycles
- None detected.

## Communities (92 total, 7 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.08
Nodes (118): BlockCnt, ConsControl, expkind, Labeldesc, Labellist, l_noret, LexState, luaK_semerror() (+110 more)

### Community 1 - "Community 1"
Cohesion: 0.08
Nodes (114): addk(), binopr2op(), binopr2TM(), boolF(), boolT(), BinOpr, expdesc, FuncState (+106 more)

### Community 2 - "Community 2"
Cohesion: 0.08
Nodes (88): l_mem, lua_gc(), atomic(), atomic2gen(), CClosure, GCObject, global_State, LClosure (+80 more)

### Community 3 - "Community 3"
Cohesion: 0.07
Nodes (85): aux_rawset(), aux_upvalue(), auxgetstr(), auxsetstr(), GCObject, l_sinline, LClosure, lua_Alloc (+77 more)

### Community 4 - "Community 4"
Cohesion: 0.07
Nodes (79): l_uint32, CallInfo, l_noret, l_sinline, LUA_API, lua_KContext, lua_KFunction, lua_State (+71 more)

### Community 5 - "Community 5"
Cohesion: 0.06
Nodes (58): __gb_execute_cb(), gb_get_save_size(), gb_get_save_size_s(), gb_init(), gb_init_lcd(), __gb_read(), gb_reset(), gb_run_frame() (+50 more)

### Community 6 - "Community 6"
Cohesion: 0.06
Nodes (61): EmotionDef, lv_anim_exec_xcb_t, lv_anim_ready_cb_t, lv_color_t, animate(), animate_done(), arrive_big(), arrive_small() (+53 more)

### Community 7 - "Community 7"
Cohesion: 0.08
Nodes (62): lua_error(), lua_gettop(), lua_pushboolean(), lua_pushinteger(), lua_pushnil(), lua_pushvalue(), lua_status(), lua_toboolean() (+54 more)

### Community 8 - "Community 8"
Cohesion: 0.11
Nodes (60): Closure, auxgetinfo(), basicgetobjname(), CallInfo, Instruction, l_noret, LUA_API, lua_Debug (+52 more)

### Community 9 - "Community 9"
Cohesion: 0.10
Nodes (55): lua_absindex(), lua_pushfstring(), lua_setwarnf(), boxgc(), FILE, lua_Debug, lua_Number, lua_State (+47 more)

### Community 10 - "Community 10"
Cohesion: 0.12
Nodes (54): lua_Number, lua_pushnumber(), lua_Unsigned(), luaL_checknumber(), lua_Number, lua_State, LUAMOD_API, I2d() (+46 more)

### Community 11 - "Community 11"
Cohesion: 0.12
Nodes (51): luaL_fileresult(), aux_close(), FILE, lua_State, LUAMOD_API, createmeta(), createstdfile(), f_close() (+43 more)

### Community 12 - "Community 12"
Cohesion: 0.15
Nodes (47): luaO_ceillog2(), arrayindex(), binsearch(), l_sinline, lua_Number, lua_State, Node, StkId (+39 more)

### Community 13 - "Community 13"
Cohesion: 0.08
Nodes (44): lua_sethook(), lua_Hook, app_host_request_back(), button_cb(), call_global(), App, lua_Debug, lua_State (+36 more)

### Community 14 - "Community 14"
Cohesion: 0.13
Nodes (43): lua_setmetatable(), lua_settop(), lua_type(), lua_xmove(), luaL_checkany(), luaL_checkinteger(), luaL_checktype(), luaB_rawget() (+35 more)

### Community 15 - "Community 15"
Cohesion: 0.06
Nodes (17): BaseHTTPRequestHandler, BrowserSession, tools/browser_session.py — the remote-browser half of the companion (D057).…, Write concatenated JPEGs to an HTTP client until it hangs up. Same format the…, One headless Chrome page, driven from its own thread. The Playwright sync API…, stream_to(), Capture, _demo_line() (+9 more)

### Community 16 - "Community 16"
Cohesion: 0.15
Nodes (41): lua_getfield(), lua_pushlstring(), lua_pushstring(), lua_setfield(), luaL_addgsub(), luaL_addstring(), luaL_gsub(), luaL_pushresult() (+33 more)

### Community 17 - "Community 17"
Cohesion: 0.11
Nodes (41): ble_clear_bonds(), net_apply_wifi(), net_request_weather_now(), net_scan(), body_label(), clear_bonds_cb(), close_cb(), confirm_cb() (+33 more)

### Community 18 - "Community 18"
Cohesion: 0.14
Nodes (40): F2Imod, lua_Integer(), intarith(), l_sinline, lua_Number, lua_State, Proto, StkId (+32 more)

### Community 19 - "Community 19"
Cohesion: 0.11
Nodes (36): lv_indev_t, app_host_back(), app_host_home(), app_host_is_open(), app_host_launch(), app_host_open_drawer(), app_host_request_open(), app_host_running() (+28 more)

### Community 20 - "Community 20"
Cohesion: 0.13
Nodes (38): adddigit(), arith(), arith_add(), arith_div(), arith_idiv(), arith_mod(), arith_mul(), arith_pow() (+30 more)

### Community 21 - "Community 21"
Cohesion: 0.08
Nodes (34): BatteryState, app_host_current(), ble_key_subs(), ble_mouse_subs(), emotion_active(), emotion_begin(), emotion_current_state(), emotion_name() (+26 more)

### Community 22 - "Community 22"
Cohesion: 0.16
Nodes (36): luaD_inctop(), l_noret, LClosure, lu_byte, lua_Number, lua_State, Proto, TString (+28 more)

### Community 23 - "Community 23"
Cohesion: 0.12
Nodes (29): back_cb(), build_card(), bump(), clear_busy(), lv_anim_t, lv_coord_t, lv_event_cb_t, lv_event_t (+21 more)

### Community 24 - "Community 24"
Cohesion: 0.12
Nodes (31): lv_event_t, lv_obj_t, decor(), list_build(), ov_kb_cb(), overlay_close(), overlay_open(), player_close() (+23 more)

### Community 25 - "Community 25"
Cohesion: 0.10
Nodes (30): player_open(), age_from_iso(), JDEC, JRECT, String, decode_frame(), decode_thumb(), do_play() (+22 more)

### Community 26 - "Community 26"
Cohesion: 0.14
Nodes (30): Dyndata, CClosure, LClosure, lua_State, Proto, StkId, TValue, UpVal (+22 more)

### Community 27 - "Community 27"
Cohesion: 0.14
Nodes (3): lua_State, LUALIB_API, luaL_openlibs()

### Community 28 - "Community 28"
Cohesion: 0.10
Nodes (27): browser_create(), lv_event_cb_t, lv_obj_t, page_touch_cb(), tool_btn(), BEvent, body, path (+19 more)

### Community 29 - "Community 29"
Cohesion: 0.21
Nodes (29): l_noret, LexState, lua_State, TString, ZIO, check_next1(), check_next2(), esccheck() (+21 more)

### Community 30 - "Community 30"
Cohesion: 0.16
Nodes (26): apply_mode(), build_bus_map(), build_gpio_tab(), build_i2c_tab(), build_uart_tab(), bus_gpio(), lv_event_t, lv_obj_t (+18 more)

### Community 31 - "Community 31"
Cohesion: 0.14
Nodes (25): ble_apply_name(), ble_begin(), ble_is_running(), ble_stop(), app_apply_brightness(), app_refresh_clock(), setup(), msg_begin() (+17 more)

### Community 32 - "Community 32"
Cohesion: 0.20
Nodes (24): BuffFS, addnum2buff(), addstr2buff(), lua_Number, lua_State, StkId, TValue, va_list (+16 more)

### Community 33 - "Community 33"
Cohesion: 0.24
Nodes (23): CallInfo, lua_State, Proto, StkId, Table, TMS, TString, TValue (+15 more)

### Community 34 - "Community 34"
Cohesion: 0.23
Nodes (22): IdxT, lua_geti(), lua_seti(), addfield(), auxsort(), lua_State, luaL_Buffer, LUAMOD_API (+14 more)

### Community 35 - "Community 35"
Cohesion: 0.19
Nodes (22): lua_State, LUAMOD_API, checkoption(), getboolfield(), getfield(), l_checktime(), luaopen_os(), os_clock() (+14 more)

### Community 36 - "Community 36"
Cohesion: 0.17
Nodes (21): MsgEntry, app_host_std_back(), lv_event_cb_t, age_str(), bubbles_build(), chat_row_cb(), lv_event_t, lv_obj_t (+13 more)

### Community 37 - "Community 37"
Cohesion: 0.23
Nodes (21): AsyncWebServerRequest, app_name_param(), String, handle_apps_body(), handle_apps_delete(), handle_apps_list(), handle_apps_post(), handle_crash() (+13 more)

### Community 38 - "Community 38"
Cohesion: 0.29
Nodes (21): DumpState, lua_Number, lua_State, lua_Writer, Proto, TString, dumpBlock(), dumpByte() (+13 more)

### Community 39 - "Community 39"
Cohesion: 0.15
Nodes (19): MsgPeer, msgs_tick(), scan_cb(), contact_learn_locked(), do_scan(), do_send(), history_add_locked(), msg_busy() (+11 more)

### Community 40 - "Community 40"
Cohesion: 0.12
Nodes (16): Accelerate, CMSampleBuffer, CoreMedia, Error, Float, Foundation, NSObject, OpaquePointer (+8 more)

### Community 41 - "Community 41"
Cohesion: 0.23
Nodes (20): luaL_error(), capture_to_close(), check_capture(), classend(), end_capture(), get_onecapture(), gmatch_aux(), match() (+12 more)

### Community 42 - "Community 42"
Cohesion: 0.25
Nodes (19): lua_State, TString, Udata, createstrobj(), growstrtab(), internshrstr(), luaS_createlngstrobj(), luaS_eqlngstr() (+11 more)

### Community 43 - "Community 43"
Cohesion: 0.15
Nodes (18): GeoResult, NetStatus, String, fetch_weather(), https_get(), mark_stale(), net_disconnect_text(), net_geocode() (+10 more)

### Community 44 - "Community 44"
Cohesion: 0.23
Nodes (19): luaL_Buffer, luaL_addlstring(), luaL_addvalue(), luaL_buffinit(), luaL_prepbuffsize(), newbuffsize(), prepbuffsize(), add_s() (+11 more)

### Community 45 - "Community 45"
Cohesion: 0.16
Nodes (15): lv_event_t, lv_obj_t, eq_create(), eq_destroy(), eq_icon(), eq_tick(), eq_levels(), eq_rev() (+7 more)

### Community 46 - "Community 46"
Cohesion: 0.34
Nodes (13): luaL_optinteger(), byteoffset(), lua_State, codepoint(), iter_aux(), iter_auxlax(), iter_auxstrict(), pushutfchar() (+5 more)

### Community 47 - "Community 47"
Cohesion: 0.30
Nodes (12): global_State, l_noret, lua_State, firsttry(), luaM_free_(), luaM_growaux_(), luaM_malloc_(), luaM_realloc_() (+4 more)

### Community 48 - "Community 48"
Cohesion: 0.24
Nodes (11): ascii_to_hid(), lv_event_t, lv_obj_t, kb_cb(), kbd_create(), kbd_icon(), kbd_tick(), tail_push() (+3 more)

### Community 49 - "Community 49"
Cohesion: 0.26
Nodes (10): ble_scan(), ble_send(), _ble_send(), emote(), http_health(), http_post(), main(), _need_bleak() (+2 more)

### Community 50 - "Community 50"
Cohesion: 0.32
Nodes (11): browser_back_nav(), browser_destroy(), browser_icon(), lv_event_t, kb_ov_close(), kb_page_cb(), kb_toggle_cb(), url_edit_cb() (+3 more)

### Community 51 - "Community 51"
Cohesion: 0.24
Nodes (12): browser_tick(), overlay_done(), browser_frame(), browser_frame_rev(), browser_status(), browser_streaming(), httpapi_begin(), yt_set_channels() (+4 more)

### Community 52 - "Community 52"
Cohesion: 0.27
Nodes (10): lv_event_t, lv_obj_t, pad_cb(), state_paint(), tp_create(), tp_destroy(), tp_icon(), tp_tick() (+2 more)

### Community 53 - "Community 53"
Cohesion: 0.27
Nodes (8): EmotionRequest, NimBLECharacteristic, NimBLECharacteristicCallbacks, HidOutCallbacks, RxCallbacks, emotion_from_name(), emotion_parse(), emotion_post()

### Community 54 - "Community 54"
Cohesion: 0.27
Nodes (7): apply_cb(), lv_event_t, lv_obj_t, theme_card(), themes_create(), themes_icon(), theme_count()

### Community 55 - "Community 55"
Cohesion: 0.44
Nodes (9): luaL_buffinitsize(), luaL_checklstring(), luaL_optlstring(), luaL_pushresultsize(), str_char(), str_lower(), str_rep(), str_reverse() (+1 more)

### Community 56 - "Community 56"
Cohesion: 0.39
Nodes (8): lv_font_t, font_by_name(), l_ui_button(), l_ui_card(), l_ui_label(), opt_int(), opt_str(), push_handle()

### Community 57 - "Community 57"
Cohesion: 0.71
Nodes (6): on_create(), on_exit(), on_tick(), paint(), pin(), select_pin()

### Community 58 - "Community 58"
Cohesion: 0.48
Nodes (7): Header, KOption, digit(), getdetails(), getnum(), getnumlimit(), getoption()

### Community 59 - "Community 59"
Cohesion: 0.29
Nodes (6): YtVideo, age, channel, id, thumb, title

### Community 60 - "Community 60"
Cohesion: 0.29
Nodes (7): JpgCtx, data, len, oh, out, ow, pos

### Community 61 - "Community 61"
Cohesion: 0.29
Nodes (7): JpgCtx, data, len, oh, out, ow, pos

### Community 62 - "Community 62"
Cohesion: 0.40
Nodes (4): ble_gap_conn_desc, NimBLEServer, NimBLEServerCallbacks, ServerCallbacks

## Knowledge Gaps
- **47 isolated node(s):** `id`, `title`, `channel`, `age`, `thumb` (+42 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 280 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **7 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `setup()` connect `Community 31` to `Community 24`, `Community 13`, `Community 21`, `Community 6`?**
  _High betweenness centrality (0.095) - this node is a cross-community bridge._
- **Why does `script_begin()` connect `Community 13` to `Community 31`?**
  _High betweenness centrality (0.088) - this node is a cross-community bridge._
- **Why does `yt_begin()` connect `Community 31` to `Community 25`?**
  _High betweenness centrality (0.019) - this node is a cross-community bridge._
- **Are the 53 inferred relationships involving `luaL_error()` (e.g. with `lua_concat()` and `lua_error()`) actually correct?**
  _`luaL_error()` has 53 INFERRED edges - model-reasoned connections that need verification._
- **What connects `id`, `title`, `channel` to the rest of the system?**
  _47 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Community 0` be split into smaller, more focused modules?**
  _Cohesion score 0.07676969092721835 - nodes in this community are weakly interconnected._
- **Should `Community 1` be split into smaller, more focused modules?**
  _Cohesion score 0.07536231884057971 - nodes in this community are weakly interconnected._