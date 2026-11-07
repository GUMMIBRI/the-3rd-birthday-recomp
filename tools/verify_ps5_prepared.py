"""Verify one private static-game cohort before and after compilation."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def verify(directory):
    directory = Path(directory).resolve()
    directory.relative_to((ROOT / "build").resolve())
    manifest = json.loads((directory / "PREPARED-GAME.json").read_text())
    for relative, digest in manifest["generated_files"].items():
        file = (directory / relative).resolve()
        file.relative_to(directory)
        if hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Prepared input changed: {relative}")
    for relative, digest in manifest["generator_sources"].items():
        file = (ROOT / relative).resolve()
        file.relative_to(ROOT)
        if hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Generator/interface changed: {relative}; regenerate the cohort")
    required = ["input.elf", "image.bin", "game_image.c", "recomp.c", "gen/recomp_tail.c"]
    if any(name not in manifest["generated_files"] for name in required):
        raise ValueError("Incomplete static-game cohort")
    if manifest["generated_files"]["input.elf"] != manifest["input_sha256"] or \
       manifest["generated_files"]["image.bin"] != manifest["image_sha256"]:
        raise ValueError("Prepared identity mismatch")
    expected = {p for p in manifest["generated_files"] if p.startswith("gen/recomp_") and p.endswith(".c")}
    actual = {str(p.relative_to(directory)).replace("\\", "/") for p in (directory / "gen").glob("recomp_*.c")}
    if not expected or expected != actual:
        raise ValueError("Unexpected/missing translated source files")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    data = verify(args.directory)
    print(f"Private cohort verified: {data['disc_id']}; no game execution")
