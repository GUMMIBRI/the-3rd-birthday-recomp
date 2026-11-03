"""Run the game on a chosen GPU and take screenshots: the title screen, a fight (scene 25) at
60 and 30 fps. For GPU-specific rendering bugs (issue #9: black blocks on AMD).

    python tools/gpu_test.py <gpu name part, e.g. Radeon or NVIDIA> [tag]
Writes scratchpad/gpu/<tag>/*.png and port.err.
"""

import os
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drive
import scenarios

gpu = sys.argv[1]
tag = sys.argv[2] if len(sys.argv) > 2 else gpu


def run(interp):
    out = os.path.join(drive.SCRATCH, "gpu", f"{tag}_interp{interp}")
    shutil.rmtree(out, ignore_errors=True); os.makedirs(out)
    tmp = os.path.join(drive.BUILD, "save_scene")
    shutil.rmtree(tmp, ignore_errors=True); shutil.copytree(drive.SAVES["user"][0], tmp)
    f = os.path.join(tmp, "ULUS10567DATA00", "3RDDATA.BIN")
    d = bytearray(open(f, "rb").read()); struct.pack_into("<I", d, 0xB9D8, 0xFFFF0000 | 25); open(f, "wb").write(d)
    g = drive.Port(out, tmp, extra_env={"SR_GPU_DEVICE": gpu, "SR_INTERP": interp})
    try:
        scenarios.to_title_menu(g)
        g.shot("a_title")
        g.press("cross"); g.wait(4); g.press("cross"); g.wait(25)
        for i in range(4):
            g.shot(f"b_fight{i}"); g.wait(0.7)
        g.stick(255, 128, 60); g.wait(1.0); g.shot("c_walk")
    finally:
        g.close()


for i in (sys.argv[3:] or ["1", "0"]):
    run(i)
