"""Widescreen check (issue #7): load the user's save (the CTI hub), widescreen on/off in a given
window size, screenshots of Aya. Writes scratchpad/ws/<tag>/*.png.

    python tools/ws_test.py [WxH ...]      default 1918x1027 2520x1080
"""

import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
import scenarios


def run(size, ws):
    tag = f"{size}_ws{ws}"
    if os.environ.get("OLD_CHEAT_BUG"): tag += "_oldcheats"
    if os.environ.get("RESIZE_TO"): tag += "_resized"
    out = os.path.join(drive.SCRATCH, "ws", tag)
    shutil.rmtree(out, ignore_errors=True); os.makedirs(out)
    if os.environ.get("OLD_CHEAT_BUG"):   # what 0.9.1/0.9.2's Infinite Liberation + Max BP wrote every frame
        os.makedirs(os.path.join(out, "patches"))
        open(os.path.join(out, "patches", "oldcheats.txt"), "w").write(
            "08CAAB0C u32 0x447A0000 always\n08CAB904 u32 999999 always\n")
    resize = os.environ.get("RESIZE_TO")   # start at this size, then resize like maximizing (issue #7)
    env = {"SR_WIDESCREEN": ws}
    if not resize: env["SR_WINSIZE"] = size
    g = drive.Port(out, drive.SAVES["user"][0], extra_env=env)
    try:
        scenarios.to_title_menu(g)
        g.press("cross"); g.wait(4); g.press("cross"); g.wait(20)
        for _ in range(4):                                      # skip the movie / talk into the hub
            g.press("start"); g.wait(4); g.press("cross"); g.wait(4)
        for _ in range(3): g.press("circle"); g.wait(1.5)      # close the status menu Start opened
        if resize:
            import ctypes
            from controls_test import hwnd_of
            w, h = (int(v) for v in size.split("x"))
            ctypes.windll.user32.SetWindowPos(hwnd_of(g.p.pid), None, 0, 0, w, h, 0x0002 | 0x0004 | 0x0010)  # NOMOVE NOZORDER NOACTIVATE
            g.wait(3)
        g.shot("a")
        if ws == "1": g._cmd("drawlog"); g.wait(1)
        g.stick(1, 128, 20); g.wait(1.5); g.shot("b")          # turn a little
        g.stick(255, 128, 40); g.wait(1.5); g.shot("c")
    finally:
        g.close()


for s in (sys.argv[1:] or ["1918x1027", "2520x1080"]):
    for ws in (os.environ.get("WS_ONLY") or "01"):
        run(s, ws)
