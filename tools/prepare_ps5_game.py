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


def file_sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(*args):
    subprocess.run([sys.executable, *map(str, args)], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--iso", type=Path, help="exact supported owned ISO for separately verified ISO-derived route")
    parser.add_argument("--decrypt-receipt", type=Path, help="receipt from scripts/build_eboot_tool.ps1")
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to((ROOT / "build").resolve())
    if output == (ROOT / "build").resolve() or output.exists():
        raise ValueError("Choose a fresh private subdirectory under build/")
    source = args.elf.read_bytes()
    version = tomllib.loads((ROOT / "versions/ULUS10567.toml").read_text(encoding="utf-8"))
    expected = version["eboot"]["decrypted_elf_sha256"]
    provenance = {"method": "profile-ppsspp-dump"}
    iso_digest = tool_digest = tool = None
    if sha(source) != expected:
        if not args.iso or not args.decrypt_receipt:
            raise ValueError("PPSSPP dump mismatch: the distinct ISO-derived route requires --iso and --decrypt-receipt")
        if args.iso.stat().st_size != version["disc"]["iso_size"] or file_sha(args.iso) != version["disc"]["iso_sha256"]:
            raise ValueError("ISO size/digest does not match the supported profile")
        receipt_path = args.decrypt_receipt.resolve()
        receipt_path.relative_to((ROOT / "build").resolve())
        receipt = json.loads(receipt_path.read_text(encoding="utf-8-sig"))
        if receipt["method"] != "locally-built-upstream-eboot-decrypt" or receipt["recipe"] != "scripts/build_eboot_tool.ps1" or \
           file_sha(ROOT / receipt["recipe"]) != receipt["recipe_sha256"]:
            raise ValueError("Unrecognized or changed preparation build recipe")
        for relative, digest in receipt["source_hashes"].items():
            file = (ROOT / relative).resolve()
            file.relative_to((ROOT / "runtime/third_party/eboot_decrypt").resolve())
            if file_sha(file) != digest:
                raise ValueError("Preparation source differs from locally built utility")
        tool = (receipt_path.parent / receipt["executable"]).resolve()
        tool.relative_to(receipt_path.parent)
        tool_digest = file_sha(tool)
        if tool_digest != receipt["executable_sha256"]:
            raise ValueError("Preparation binary changed since recorded source build")
        iso_digest = file_sha(args.iso)
        provenance = {"method": "verified-iso-derived", "iso_sha256": iso_digest,
                      "tool_sha256": tool_digest, "build_receipt_sha256": file_sha(receipt_path)}
    if source[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", source, 16)[0] != 0xFFA0:
        raise ValueError("Expected supported ELF32 little-endian PSP PRX")
    output.mkdir(parents=True)
    if tool:
        fresh = output / "ISO-DERIVED.elf"
        subprocess.run([str(tool), str(args.iso.resolve()), str(fresh)], check=True, cwd=ROOT)
        if fresh.read_bytes() != source:
            raise ValueError("Selected ELF differs from fresh preparation from supported ISO")
        if len(source) != version["eboot"]["elf_size"] or struct.unpack_from("<I", source, 24)[0] != version["eboot"]["entry"]:
            raise ValueError("ISO-derived ELF size/entry differs from supported layout")
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
    module_name = source[module_file + 4:module_file + 32].split(b"\0", 1)[0].decode("ascii")
    if module_name != version["eboot"]["module_name"]:
        raise ValueError("Module identity differs from supported game")
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
    if tool and (file_sha(args.iso) != iso_digest or file_sha(tool) != tool_digest):
        raise ValueError("ISO/tool changed during preparation; output cohort is invalid")
    if any(sha((ROOT / p).read_bytes()) != digest for p, digest in generator_hashes.items()):
        raise ValueError("Preparation tools changed during generation; output is not a valid cohort")
    manifest = {"disc_id": "ULUS10567", "input_sha256": sha(source), "image_sha256": sha(image),
                "base": base, "entry": entry, "module_gp": gp, "image_size": len(image),
                "module_info_address": module_address, "generator_sources": generator_hashes,
                "provenance": provenance,
                "initial_register_contract": "module GP; r4/r5=0 per upstream prepare.py; SP/k0 allocated by scheduler",
                "compile_without": ["SR_GAME_DLL", "game_glue.c", "driver.c"],
                "generated_files": {p.relative_to(output).as_posix(): sha(p.read_bytes()) for p in files},
                "game_executed": False, "private_game_derived_output": True}
    (output / "PREPARED-GAME.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Private static game sources prepared; not compiled, linked or executed.")


if __name__ == "__main__":
    main()
