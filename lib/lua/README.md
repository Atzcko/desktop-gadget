# lua — vendored, not fetched

Lua 5.4.7 from https://www.lua.org/ftp/lua-5.4.7.tar.gz, MIT licensed,
copied verbatim except for `lua.c` and `luac.c` — the standalone interpreter
and compiler binaries, both of which define `main()`.

**Vendored rather than pulled from the PlatformIO registry** on purpose. The
registry entries are third-party wrappers of varying maintenance; Lua's own
source is ~15 k lines of dependency-free C89 that has compiled everywhere for
twenty years. Vendoring makes the version a fact of this repo rather than a
resolution result. See [[D037 - Apps become Lua scripts]].

Configuration lives in `luaconf.h` **as shipped** — do not edit it. Anything
this project needs to change is a `-D` in `library.json`, so the next version
bump stays a directory copy.
