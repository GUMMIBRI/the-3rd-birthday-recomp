# Parasite Eve 3 native PS5 — current handoff

The owner selected **Parasite Eve 3** as the display name. The underlying game is
The 3rd Birthday; attribution and upstream identity remain intact. Repository
and branch remain `GUMMIBRI/the-3rd-birthday-recomp`, `ps5-port`.

## Current delivery evidence

A private native PS5 application links the supported game's ahead-of-time
translated code with the real Vulkan renderer, SDL presentation/input/audio,
ATRAC and H.264 decoders, native GUI/OSK, scheduler, ISO reader and host services.
No new CPU interpreter, emulator launcher or JIT is used. The current translator
produced 19,471 discovered functions with zero generated trapping stubs. This is
source evidence, not proof that every original instruction or scene is correct.

The resulting `eboot.bin` is under
`build/private-game-20261106-r2/native-link/converted-r1/`. It is private and
contains game-derived code/data; never upload it or its fingerprints to GitHub.
The original tester-supplied ISO, SMB source and earlier artifacts are preserved.

The selected private app folder is `build/private-delivery-r3/PPSA77025`, with
36 hash-bound files: EBOOT, metadata, original mandatory 512x512 icon, fonts,
VFPU tables and notices. Packaging gates pass; console delivery, launcher
discovery and boot remain pending. Earlier local candidates are preserved.
See `PS5-INSTALL-PLAN.md` for Cinder's reviewed recoverable USB file-delivery
sequence. Atomic whole-directory activation is not required or claimed.

Actual static checks passed:

- One verified preparation cohort compiled; source/image/module GP/base/entry
  derive from the same frozen input, with source and object hash checks.
- 359 imports resolve to the selected public system stubs; no decoded inline
  syscalls, no native TLS, and one local emulated-TLS implementation.
- Real definitions for translated function registration, game image, ICD and
  required Vulkan internal provider are present.
- Conversion preserves entry, 11 retained allocated sections and BSS.
- Loader-visible SCE NIDs, module/library providers, hash tables, 21,201 dynamic
  relocations and 25 PLT relocations match the independently checked model.
- SELF inspection reports valid integrity. Corrupted verifier fixtures reject.

There has been **no console launch, installation, boot observation, gameplay,
movie playback or save/reload test**. No full-game completion claim is made.

## Input and preparation

The owner placed a supported US disc image in the project folder. It matches the
existing public ULUS10567 ISO profile. The current source-built decryptor produces
a different ELF representation from the older PPSSPP dump recorded upstream.
Preparation keeps the original strict dump check and uses a separately named
ISO-derived route: exact ISO validation, source-built utility receipt, fresh
byte-for-byte extraction comparison, module/layout checks and final rehashes.
Do not pad, truncate or replace the original upstream fingerprint to force a pass.

Private preparation and compilation:

```
scripts/build_eboot_tool.ps1 -Zig <local-zig>
python tools/prepare_ps5_game.py <owned-decrypted-EBOOT> build/<fresh-cohort> --iso <owned-ISO> --decrypt-receipt build/eboot-tool/BUILD-RECEIPT.json
scripts/compile_ps5_game.ps1 -PlatformRoot <public-prefix> -PreparedDirectory build/<cohort>
scripts/check_ps5_runtime.ps1 -PlatformRoot <public-prefix>
scripts/link_ps5_game.ps1 -PlatformRoot <public-prefix> -PreparedDirectory build/<cohort> -ConsoleIsoPath <selected-USB-ISO-path>
```

Build FFmpeg with `scripts/build_ps5_ffmpeg.ps1`; its real static libraries and
license/configuration contract are documented in `PS5-MEDIA.md`. ATRAC uses the
already-vendored licensed decoder, compiled for the target. Do not substitute an
unsupported/blank decoder to obtain a link.

## USB-only boundary and actual blockers

Cinder provisionally allocated `PPSA77025` and
`/mnt/usb0/homebrew/PPSA77025` for source preparation. This is not a catalog-wide
reservation; actual collision checking remains a pre-install gate. The selected
ISO read path is a USB installation plan, not an observed console file.

The native driver pins its root and user paths to the selected numeric USB mount.
HOME, cache and temporary environment paths point to the app-owned USB user
subtree. Mesa disk caching is disabled. No internal `/data` write fallback exists.

Native console writes are currently disabled by `storage_gate.h`: save mutations,
guest file writes and persistent settings/screenshots return an explicit failure
or diagnostic. Real save implementation is retained, not replaced by success
stubs. Do not enable `SR_PS5_WRITABLE_STORAGE_VERIFIED` merely to bypass this gate.

The public `ps5_*at` compatibility functions reconstruct paths. They do not prove
atomic beneath-root confinement. Struct-bearing APIs also must not cross the
retained RADV/game SDK boundary by simple linker aliases. The scalar directory
adapter isolates that ABI; the root handle remains live. A genuine `_openat`
import is a candidate for a reviewed read-only capability, but full relative
mkdir/replace/delete transactions remain unqualified. See
`PS5-DIRECTORY-PROVIDER.md`. Cinder requires this boundary to be coherent before
enabling writes.

## Next work and ownership

This chat owns integration and Git. Cinder provides project guidance/review and
exclusively dispatches any separate helper account. Native read-only anchor work
and local delivery preparation are isolated worker tasks; check actual current
worker status before dispatching or editing owned files.

Continue native provider/ABI work and finish reviewed USB transfer tooling,
and obtain the human's actual boot/control/movie/combat evidence after an approved
USB-only installation. Console writes require current authorization and fresh
closed-game confirmation; no autonomous launch. Keep other project apps/saves
untouched. Qwen remains blocked by the previously recorded normal-path tool
rejection; no alternate invocation or policy change is permitted.
