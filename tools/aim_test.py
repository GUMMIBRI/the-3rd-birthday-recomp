"""Free aim check: load a fight (scene 25), turn the camera with the mouse, hold L (aim), move the
mouse left/right and up/down while aiming, fire, release. Prints where the game's view looks at
each step and saves screenshots (scratchpad/aim/<style>_<freeaim>/).

    python tools/aim_test.py [classic|modern|both] [freeaim 0|1]

With free aim the aim starts where the camera looked and the crosshair stays in the middle of
the screen while the mouse turns it; without, the game's own aim (along Aya's facing).
"""

import math
import os
import shutil
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
import scenarios


def run(style, freeaim):
    out = os.path.join(drive.SCRATCH, "aim", f"{style}_{freeaim}")
    shutil.rmtree(out, ignore_errors=True); os.makedirs(out)
    tmp = os.path.join(drive.BUILD, "save_scene")
    shutil.rmtree(tmp, ignore_errors=True); shutil.copytree(drive.SAVES["user"][0], tmp)
    f = os.path.join(tmp, "ULUS10567DATA00", "3RDDATA.BIN")
    d = bytearray(open(f, "rb").read()); struct.pack_into("<I", d, 0xB9D8, 0xFFFF0000 | 25); open(f, "wb").write(d)
    g = drive.Port(out, tmp, extra_env={"SR_TWINSTICK": "1", "SR_CAMSTYLE": "1" if style == "modern" else "0",
                                         "SR_FREEAIM": freeaim})

    def look(tag):   # the rendered view camera's look direction (yaw, pitch)
        p = struct.unpack("<I", g.read(0x08C3C58C, 4))[0]
        v = struct.unpack("<12f", g.read(p + 0x10, 48))
        dx, dy, dz = v[4] - v[0], v[5] - v[1], v[6] - v[2]
        print(f"{style} freeaim={freeaim} {tag}: yaw {math.atan2(dx, dz):+.3f} pitch {math.atan2(dy, math.hypot(dx, dz)):+.3f}", flush=True)

    try:
        scenarios.to_title_menu(g)
        g.press("cross"); g.wait(4); g.press("cross"); g.wait(25)
        for _ in range(10): g._cmd("mouse 40 0"); time.sleep(0.05)       # camera ~60 degrees right
        g.wait(1); g.shot("a_turned"); look("camera turned")
        g._cmd("btn 100 1200"); g.wait(1.5); g.shot("b_aim"); look("aiming")
        for _ in range(30): g._cmd("mouse -12 0"); time.sleep(0.033)    # aim left
        g.wait(0.5); g.shot("c_aim_left"); look("aim moved left")
        for _ in range(20): g._cmd("mouse 0 -10"); time.sleep(0.033)    # aim up
        g.wait(0.5); g.shot("d_aim_up"); look("aim moved up")
        g._cmd("btn 300 60"); g.wait(0.6); g.shot("e_fire"); g.wait(0.6)  # fire (R, L still held)
        g._cmd("btn 100 400")
        g.stick(1, 128, 40); g.wait(1.0); g.shot("f_strafe"); look("strafing")
        g.wait(8); g.shot("g_released"); look("released")
    finally:
        g.close()


which = sys.argv[1] if len(sys.argv) > 1 else "both"
fa = sys.argv[2] if len(sys.argv) > 2 else "1"
for st in (("classic", "modern") if which == "both" else (which,)):
    run(st, fa)
