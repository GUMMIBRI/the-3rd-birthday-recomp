"""Prepare private static PS5 game input. Never download or publish game data.

The caller provides the supported decrypted US EBOOT. Output must be a new
directory under this repository's ignored build/ tree. No game code is executed.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def run(*args):
    subprocess.run([sys.executable, *map(str, args)], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to((ROOT / "build").resolve())
    if output == (ROOT / "build").resolve() or output.exists():
        raise ValueError("Choose a fresh private subdirectory under build/")
    source = args.elf.read_bytes()
    version = tomllib.loads((ROOT / "versions/ULUS10567.toml").read_text(encoding="utf-8"))
    expected = version["eboot"]["decrypted_elf_sha256"]
    if sha(source) != expected:
        raise ValueError("Decrypted EBOOT does not match supported ULUS10567 profile")
    if source[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", source, 16)[0] != 0xFFA0:
        raise ValueError("Expected supported ELF32 little-endian PSP PRX")
    output.mkdir(parents=True)
    generator_paths = ["tools/prepare_ps5_game.py", "tools/split_recomp.py", "tools/toml_addrs.py",
                       "versions/ULUS10567.toml", "runtime/tools/prxload.py", "runtime/tools/imports.py",
                       "runtime/tools/analyze.py", "runtime/tools/codegen.py", "runtime/src/rt/recomp.h",
                       "runtime/src/rt/platform/game_profile.h"]
    generator_hashes = {p: sha((ROOT / p).read_bytes()) for p in generator_paths}
    frozen = output / "input.elf"
    frozen.write_bytes(source)
    spec = importlib.util.spec_from_file_location("native_prxload", ROOT / "runtime/tools/prxload.py")
    loader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loader)
    base = 0x08804000
    prx = loader.Prx(str(frozen), base)
    prx.relocate()
    # PSP PRX convention: p_paddr of first program header holds module-info
    # file offset. Read GP from that structure AFTER relocation into image RAM.
    module_file = struct.unpack_from("<I", source, prx.phoff + 12)[0] & 0x7FFFFFFF
    containing = [s for s in prx.segments if s["type"] == 1 and
                  s["off"] <= module_file and module_file + 52 <= s["off"] + s["filesz"]]
    if len(containing) != 1:
        raise ValueError("Module info is not in exactly one loaded segment")
    segment = containing[0]
    module_address = base + segment["vaddr"] + module_file - segment["off"]
    gp = prx.r32(module_address + 32)
    entry = base + prx.entry
    image = bytes(prx.mem)
    if not (base <= entry < base + len(image) <= 0x0C000000) or entry & 3:
        raise ValueError("Image/entry outside supported guest RAM")
    if gp and not 0x08000000 <= gp < 0x0C000000:
        raise ValueError("Relocated module GP outside guest RAM")
    (output / "image.bin").write_bytes(image)
    hooks = subprocess.check_output([sys.executable, "tools/toml_addrs.py", "--hooks",
                                     "versions/ULUS10567.toml"], cwd=ROOT, text=True).strip()
    run(ROOT / "runtime/tools/imports.py", frozen, hex(base), f"--toml={output / 'imports.toml'}")
    run(ROOT / "runtime/tools/analyze.py", frozen, f"--base={hex(base)}",
        f"--toml={output / 'functions.toml'}", "--quiet")
    run(ROOT / "runtime/tools/codegen.py", frozen, output / "recomp.c", f"--base={hex(base)}",
        f"--toml={output / 'functions.toml'}", f"--hooks={hooks}")
    run(ROOT / "tools/split_recomp.py", output / "recomp.c", output / "gen")
    # Embed bytes and profile together. There is no separately swappable image.bin
    # in the PS5 application, unlike the Windows developer launch path.
    with (output / "game_image.c").open("w", encoding="ascii") as f:
        f.write('#include "platform/game_profile.h"\nstatic const unsigned char image[] = {\n')
        for at in range(0, len(image), 32):
            f.write(",".join(f"0x{v:02x}" for v in image[at:at+32]) + ",\n")
        f.write("};\nconst SrNativeGameProfile sr_native_game = {\n")
        f.write(f'0x{base:x},0x{entry:x},0x{gp:x},{len(image)},image,"{sha(source)}","{sha(image)}"\n}};\n')
    files = sorted(p for p in output.rglob("*") if p.is_file())
    if sha(frozen.read_bytes()) != sha(source):
        raise ValueError("Frozen EBOOT changed during generation; output cohort is invalid")
    if any(sha((ROOT / p).read_bytes()) != digest for p, digest in generator_hashes.items()):
        raise ValueError("Preparation tools changed during generation; output is not a valid cohort")
    manifest = {"disc_id": "ULUS10567", "input_sha256": sha(source), "image_sha256": sha(image),
                "base": base, "entry": entry, "module_gp": gp, "image_size": len(image),
                "module_info_address": module_address, "generator_sources": generator_hashes,
                "initial_register_contract": "module GP; r4/r5=0 per upstream prepare.py; SP/k0 allocated by scheduler",
                "compile_without": ["SR_GAME_DLL", "game_glue.c", "driver.c"],
                "generated_files": {str(p.relative_to(output)): sha(p.read_bytes()) for p in files},
                "game_executed": False, "private_game_derived_output": True}
    (output / "PREPARED-GAME.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Private static game sources prepared; not compiled, linked or executed.")


if __name__ == "__main__":
    main()
