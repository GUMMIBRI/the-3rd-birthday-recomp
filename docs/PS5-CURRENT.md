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

The delivered private app folder is `build/private-delivery-r4/PPSA77025`, with
36 hash-bound files: EBOOT, metadata, Cinder's selected private icon/backgrounds,
fonts, VFPU tables and notices. Earlier local candidates are preserved.
See `PS5-INSTALL-PLAN.md` for Cinder's reviewed recoverable USB file-delivery
sequence. Atomic whole-directory activation is not required or claimed.

The owner authorized the fresh USB0 app/owned-ISO install through Cinder, then
removed the blanket all-games-closed condition. No automatic launch, force-close
or reboot is permitted. Cinder selected the exact final artwork receipt; no
unselected preview or private artwork was published.

Actual read-only preflight completed: exact app destination absent, 59 app
identities read in the bounded selected-root scan, with one unrelated incomplete
packaging container explicitly excluded. ShadowMount's 132-title snapshot has
no matching title/content ID. This is local collision evidence with documented
coverage limits, not a global ID reservation.

USB file delivery is complete: all 36 app files under
`/mnt/usb0/homebrew/PPSA77025` and the owned ISO under
`/mnt/usb0/third-birthday-data/ULUS10567.iso`. The private receipt is
`build/private-usb-install-r4.json`. Every file has a matching local streaming
SHA256, completed upload and verified remote regular-file type/size; only small
final metadata was read back for a remote hash. No large-file remote hash is
claimed. Final file inventory passed. The upload slot was released to Cinder,
Forge and Raven; no remaining PE3 transfer is active.

The owner then requested an icon-only small-title correction to `[parasite 3ve]`.
That scoped update is complete, with actual old remote bytes backed up locally,
old identity checked against r4, and the selected new small icon verified by
remote SHA256. Private receipt: `build/private-icon-update-r1/RESULT.json`.
Protected EBOOT/background/ISO/parameter metadata and the parameter hash stayed
unchanged; no automatic launch occurred. The original r4 package remains intact;
the console icon now reflects this separately recorded overlay. Future updates
must use that actual current identity rather than assume the original r4 icon.

A subsequent owner-authorized background-only overlay adds three muted red
mitochondrial shapes beside the regular PE logo. Both selected DDS backgrounds
were delivered; private receipt: `build/private-background-update-r1/RESULT.json`.
Original local r4 backgrounds are backed up. Evidence is exact local SHA256,
completed uploads and remote regular-file type/size, without full background
readback or remote hash claims. The current icon and parameter remote hashes,
and protected EBOOT/ISO metadata remained unchanged. No automatic launch occurred;
the write slot was released. Preserve both overlay receipts for future updates.

Cinder later selected the pose/leg-alignment successor backgrounds for PE2 and
PE3. After explicit release of PE2's completed HD transfer and a Cinder slot grant,
the four selected DDS files were delivered. Combined private receipt:
`build/private-trilogy-background-update-r1/RESULT.json`. Four original local
backgrounds are backed up. Local SHA/upload completion/remote type-size passed;
both apps' current icon/parameter remote hashes and protected executable/module
metadata stayed unchanged. No other files were targeted and no app was launched.
The write slot is released; Cinder confirmed the other queued uploads complete.
The previous PE3 background overlay is now a preserved historical baseline.

The owner-approved PE3-only successor corrects handgun orientation/grip, adds
combat boots and curves the three muted red mitochondrial shapes. Both selected
DDS files were delivered; private receipt:
`build/private-pe3-grip-boots-update-r1/RESULT.json`. Previous installed local
backgrounds are backed up. Local SHA/upload completion/remote type-size passed;
current icon/parameter remote hashes and protected executable/module/ISO metadata
remained unchanged. No PE2 file was targeted, no app was launched and the write
slot is released. Shell artwork refresh/appearance remains human-observed.

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

The first human launch failed before game startup with `0xa0020102`
(`PRX_SCE_MODULE_LOAD_ERROR`). Matching ShadowMount logs confirm registration and
the launch; the kernel explicitly identifies the missing `sce_module` PRX.
The original package omitted the required public loader companion. Main SELF
audits did not catch this surrounding-filesystem requirement.

The corrected coherent package is `build/private-delivery-r5/PPSA77025`, with
39 files and the current selected artwork. Packaging and fresh-install admission
now require the exact signed public `sce_module/libc.prx` plus dedicated GPL/source
notices. Old r4 admission correctly fails. The three missing files were delivered
additively on USB; receipt: `build/private-load-error-r1/USB-REPAIR-r2.json`.
Local hashes/upload completion/remote type-size passed; EBOOT, artwork, ISO,
parameter bytes and the current icon were preserved. The first identity attempt
changed zero files: r5 metadata serialization order differed from the deployed
r4 bytes despite equal JSON values. The correction used the actual immutable
deployed metadata baseline. Both receipts remain private and preserved.

The subsequent human retry passed the missing-module barrier, then failed on
startup. Matched logs record `exit(2)` before `SIGSYS`; symbolization of the actual
loaded main image maps the return address to the CRT call of `exit` after `main`
returned. The original early error is not captured, because multiple startup
branches share status 2. The finite read-only report and logs are private under
`build/private-sigsys-r1/`. Next review a minimal stage/error diagnostic and the
native return path with Cinder. No speculative provider change, new build, upload,
autonomous launch or repeat human test was requested. Successful startup,
gameplay, movies and save/reload remain unverified.

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

Next-successor data policy is queued in `PS5-DATA-LAYOUT.md`: retain the stable
`third-birthday-data` name, ship it inside the app, and support optional whole-root
read selection on available numeric USB mounts or `/data`. Current code still
uses the explicit external USB0 ISO path; no discovery/migration support is
claimed. Active saves and the current deployment are preserved. No build or
console write was made for this policy.

This chat owns integration and Git. Cinder provides project guidance/review and
exclusively dispatches any separate helper account. Native read-only anchor work
and local delivery preparation are isolated worker tasks; check actual current
worker status before dispatching or editing owned files.

Next obtain the human's actual launcher-discovery/boot/control/movie/combat
observations and work concrete failures with Cinder. Native provider/ABI
qualification remains necessary before enabling saves. USB file delivery is
complete; no autonomous launch or repeated upload. Keep other project apps/saves
untouched. Qwen remains blocked by the previously recorded normal-path tool
rejection; no alternate invocation or policy change is permitted.

The startup diagnostic successor assigns distinct kernel-visible exit codes:
20 app-root override, 21 arguments/path syntax, 22 image address range, 23 app
directory anchor, 24 working directory, 25 path length, 26 environment setup,
27 image profile, 28 missing translated entry, 29 renderer readiness, and 30
ISO initialization. These codes identify the branch even if termination again
reports SIGSYS. The existing CRT exit path is retained because the exact earlier
startup cause and a replacement termination contract are unverified.

The pending directory diagnostic candidate refines stage 23 when the PS5
adapter is reached. Exit 40 means path normalization, 41 lstat, 42 root
type validation, 43 adapter allocation, 44 opendir, 45 descriptor lookup
or fstat, and 46 identity comparison. Stage 23 remains the fallback for
failures before the adapter. Existing callers retain the original status
and native error contract. The installed /app0 EBOOT predates these codes.

The USB app-folder ISO test uses the selected absolute path
`/mnt/usb0/homebrew/PPSA77025/ULUS10567.iso`. The earlier ISO copy is preserved.
The latest captured matching SIGSYS preceded this path-only upload. The
`/app0` and app-folder ISO binaries share the same CRT return address, so
that address alone cannot distinguish them. Bind the next launch using the
installed file receipt, console time, ShadowMount title and PID, and the
matching kernel exit record. The kernel log renders exit codes in hex.
