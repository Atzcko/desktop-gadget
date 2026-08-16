"""
Inject the git revision as a build flag.

WHY: FW_BUILD uses __DATE__/__TIME__, which the compiler bakes in when the
translation unit containing it is compiled. Change any OTHER file and main.cpp
is not recompiled, so the stamp goes stale while the binary genuinely changes —
exactly the failure the stamp existed to catch.

A git revision does not have that problem: it is a build FLAG, so PlatformIO
rebuilds whenever it changes, and it identifies the source precisely rather than
approximately. "+dirty" marks uncommitted changes, which is the normal state
while iterating.
"""
import subprocess

Import("env")


def _git(*args):
    try:
        return subprocess.check_output(["git", *args],
                                       stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        return ""


rev = _git("rev-parse", "--short", "HEAD") or "nogit"
if _git("status", "--porcelain"):
    rev += "+dirty"

env.Append(CPPDEFINES=[("FW_GIT", env.StringifyMacro(rev))])
print(f"version_stamp: FW_GIT={rev}")
