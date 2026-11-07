# Mods assessment for the PS5 port

Source inspection on 2026-11-06. This report describes existing PC interfaces and
proposed PS5 integration; it does not claim PS5 mod support, gameplay testing, or
that any third-party texture pack is licensed for redistribution.

## Existing interfaces

| Interface | Existing behavior | PS5 dependency |
| --- | --- | --- |
| Native texture packs | `textures/<16-hex-hash>.png`; hash covers decoded RGBA pixels and dimensions. Replacement keeps original UVs and may have higher resolution. `texture_packs` enables it. | Host directory enumeration, PNG decode, renderer image upload, cache memory budget. |
| PPSSPP texture packs | `textures.ini` and PNG files under `textures/` or `textures/ULUS10567/`. Quick/xxh32/xxh64 hashes, address-independent fallback, hash ranges and reduced-hash ranges are implemented. | Same host filesystem and GPU requirements; preserve existing lookup priority and hashing. |
| Data patches | `patches/*.txt`: hex guest address, `u8/u16/u32/f32`, value, optional `once/always`. `SR_MODDIR` selects an alternate root. Patches run at GUI vblank. | Portable directory enumeration, explicit USB root, bound address validation, a vblank call in the console frontend. |
| Compiled code mods | `mods/<name>/mod.toml` plus C sources; generated entry hooks inspect or change `CpuState` and guest memory. Returning 0 continues the original function; 1 replaces it. | Host-side generation and console compilation/linking, plus game-module attachment or static binding. |

Sources: [mods documentation](../mods/README.md),
[data-patch loader](../runtime/src/rt/mods.c),
[texture replacement](../runtime/src/rt/gpu_sdl3vk/ge_gpu.c),
[PPSSPP matching](../runtime/src/rt/gpu_sdl3vk/texpack_ppsspp.c),
[code-mod generator](../tools/mods_gen.py).

Texture formats currently supported by this interface are PNG only: the upstream
documentation excludes DDS/KTX2, replacement mipmap levels and `[filtering]`.
Render-target feedback/effects are not replaceable texture files. Preserve these
limits in a future console settings UI rather than advertising all PPSSPP packs.

The disabled [wider-FOV example](../mods/example_wider_fov/fov.c) hooks
`cam_setup` at `0x08B435B8` for the US game and changes floating-point argument
`f12`; its [manifest](../mods/example_wider_fov/mod.toml) is disabled by default.
This proves the source interface exists, not that changing FOV preserves every
scene, camera, HUD or combat interaction.

## Code-hook semantics to preserve

`tools/mods_gen.py` sorts mod folders, skips disabled manifests, generates
`mods_hooks.h`, `mods_gen.c`, and `mods_srcs.txt`, and rejects one hook name used
for different addresses. The generated dispatcher calls enabled mods in folder
order; the first hook returning 1 suppresses subsequent mods and original code.
Each mod must check `addr` itself because the dispatcher does not filter its calls
by that mod's manifest. Adding hook addresses requires regenerated guest C; changing
a mod at existing hooks requires rebuilding/relinking its console objects.

The current Windows runtime loads `sr_game_mods_hook` through `GetProcAddress` in
[driver.c](../runtime/src/rt/driver.c); the module exports it from
[game_glue.c](../runtime/src/rt/game_glue.c). A PS5 build must bind this interface
explicitly. Merely compiling the mod source will not connect it to gameplay.
[twinstick.c](../runtime/src/rt/twinstick.c) invokes `sr_mods_hook` from the game's
function hook chain. Preserve existing built-in hook ordering when replacing the
Windows module loader.

## Concrete portability and robustness findings

- `mods.c` uses `windows.h`, `MAX_PATH`, `WIN32_FIND_DATAA`,
  `FindFirstFileA/FindNextFileA/FindClose`. The texture modules use the same APIs,
  plus `CreateDirectoryA`, `GetFileAttributesA` and `_stricmp`. These need shared
  host filesystem services; copying the Windows enumeration code into a console
  frontend does not make it portable.
- `gui.c:gui_vblank` owns the data-patch call. The console frontend must call it
  once per emulated vblank, including when display pacing differs from game rate.
  Do not apply `always` patches once per interpolated display frame.
- `mods.c:load_file` validates the starting patch address against
  `[0x08000000, 0x0A000000)` but not the complete 2/4-byte span. It accepts numeric
  prefixes through `strtoul/strtof` without checking the parse end/error, treats
  every timing token except `once` as `always`, and silently skips malformed
  lines. A bounded parser should reject these cases with line diagnostics before
  porting it as a supported console feature. Guest memory helpers do not replace
  clear input validation.
- Native texture discovery assigns `realloc` directly to `s_tp_have` and then
  appends without checking allocation failure. It needs a checked temporary
  pointer and graceful fallback to original textures under console memory pressure.
- PPSSPP pack lookup constructs a path from the pack root and manifest filename;
  this layer does not enforce containment or check `snprintf` truncation. Retain
  legitimate nested folders, but validate relative paths and reject root escapes
  before using USB pack input. Match filesystem case behavior explicitly.
- Both texture discovery and patch loading initialize once. Current behavior is
  restart-to-reload; do not imply hot reload in the initial PS5 UI.

## Recommended first supported path

1. Preserve an unmodified gameplay baseline and keep optional mods disabled for
   first-console progression qualification. Reuse upstream free aim/camera
   systems as existing source features, without inventing progression bypasses.
2. Add bounded shared filesystem services and a single explicit USB mod root,
   separate from save storage. Keep `SR_MODDIR` isolation for checks, but pass
   console paths explicitly rather than relying on process working directory.
3. Port the data-patch parser with complete span/numeric validation and clear
   diagnostics. Check parsing and once/always semantics on synthetic guest RAM,
   without using private game data. This is independent of GPU bring-up.
4. Keep build-time C mods as cross-compiled code. Validate with the disabled FOV
   example only after console game-module binding exists; retain off-by-default
   manifests and explicit US revision scope. No on-console Windows compiler or
   runtime-loaded unreviewed binary mod mechanism is required.
5. Once console texture upload works, lift texture lookup/PNG file selection out
   of the Vulkan-specific module and keep renderer upload separate. Use an
   original synthetic image for checks, bound decoded dimensions/allocation sizes,
   preserve normal texture fallback, and then let the human inspect actual packs.

## Provenance and release boundaries

The combined runtime is GPL-2.0-or-later according to
[runtime/LICENSE](../runtime/LICENSE) and
[runtime/CREDITS.md](../runtime/CREDITS.md). Preserve corresponding source and
notices for reused code. A pack's compatibility with PPSSPP is not a license grant;
record creator, source, revision and redistribution permission before bundling it.
Do not put dumped textures, ISO/EBOOT, generated game C, private saves/settings or
logs in GitHub. Texture dumps and save-based test resources remain local. Original
synthetic test images and independently authored patches can be source-controlled
with their own documented provenance.

No runtime, console, inference, external repository mutation or game-data operation
was performed for this assessment. Only this report was written.

## Follow-up implementation receipt: ISO host I/O

The lead subsequently assigned this worker only `iso.c` and
`platform/iso_file.h`. These now use checked 64-bit Windows/POSIX seek/tell
operations, with 64-bit `off_t` required at compile time. Wrapper/LBA addition is
widened before arithmetic; wrapper detection no longer truncates through Windows
`long`. Sector lookup rejects failed seeks/short reads, and raw reads report host
I/O failures while retaining short EOF byte counts. Directory buffer allocation
and rounded-size overflow are checked. The existing game-facing interface and
ISO name/extent lookup semantics are unchanged.

Windows, Linux-reference and actual `x86_64-sie-ps5` ISO translation units compiled
with `-Wall -Wextra -Werror`. An isolated synthetic Windows check under
`build/mods` passed an 8 GiB seek/tell without extending the file, case-insensitive
ISO lookup, exact data read, short EOF, missing file and negative seek failure.
No game input, full application link, PS5 execution or gameplay test was used.
This is not a complete malformed-ISO parser audit; directory record-length/name
validation remains a separate pre-existing concern.

## Follow-up implementation receipt: directory host service

`platform/host_dir.h` and `.c` provide opaque open/next/close enumeration and a
bounded root-plus-leaf join. Windows uses actual `FindFirstFileA`/`FindNextFileA`
metadata; POSIX uses the selected SDK's `opendir`/`readdir`/`lstat` definitions.
Missing, permission denial, non-directory, invalid input, excessive path length,
memory failure and other I/O errors are distinct; native error codes are retained.
Empty directories end normally. Entries expose regular/directory/link/other kinds,
so consumers can explicitly select regular files. Final-component symlink/reparse
roots are rejected, including a trailing-separator alias; links among enumerated
leaves are classified without following them. No recursion or mount mutation.

Joined names reject traversal components, separators, alternate-stream delimiters,
wildcards, controls and trailing dot/space aliases; joins never truncate. Root
anchor capability/ancestor validation remains the caller's responsibility. This is
not a filesystem sandbox or a race-free open-by-handle facility: consumers must not
assume that a classified path cannot be replaced before a later open.

Windows executable checks passed file/directory classification, empty enumeration,
missing versus non-directory distinction, native missing error retention, invalid
and traversal paths, and too-small output buffers. Actual PS5 and Linux-reference
translation units compiled with warnings treated as errors. All synthetic inputs
and check outputs are isolated under `build/mods`; no console execution or runtime
mod/texture call-site changes were performed by this worker.

## Follow-up implementation receipt: PPSSPP texture pack portability

`texpack_ppsspp.c` now enumerates through `host_dir`, distinguishes absent pack
roots from denied/broken roots, uses SDL case-insensitive comparisons, checks
allocations and constructs bounded relative image paths component by component.
Regular-file manifests and images are required; nested pack images remain valid
while absolute, traversal, stream aliases and symlink components are rejected.
Manifest I/O and overlong-line failures disable the incomplete pack instead of
silently using a truncated filename. Reinitialization releases prior state.
INI-over-root-filename priority and the existing PPSSPP hash/key/wildcard algorithms
are retained. Root image discovery does not re-enumerate once per image.

The official xxHash v0.8.3 header and BSD2 license are pinned at commit
`e626a72bc2321cd320e953a0ccf1584cad60f363`; exact source/hash receipts are in
`runtime/third_party/xxhash/PROVENANCE.md`. Header-only inclusion avoids requiring
a Windows xxHash library on PS5.

Actual PS5 and Windows translation units compiled with pinned SDL headers and
warnings treated as errors. An isolated synthetic Windows check passed manifest
discovery, INI precedence, empty suppression entries, nested image resolution,
traversal/absolute-path rejection, state reset and official empty-input xxHash
vectors. These checks do not decode game textures or prove PS5 texture upload.

## Independent review: native texture integration, PNG and SDL audio

Read-only source review of the parent's first `ge_gpu.c` host-directory integration,
`platform/png.c` and `platform/audio_sdl.c` found two concrete dump-path blockers,
reported to the lead for correction:

- `tp_init` returns after root-path truncation without clearing `s_tp_dump`.
  `tp_replace` can subsequently save under the truncated root. Invalid root setup
  must disable dump and clear the path before returning.
- Dump `mkdir` accepts `EEXIST` without establishing a real, non-link directory;
  `stat` follows output links. In particular a dangling output symlink can yield
  `ENOENT`, after which PNG `fopen("wb")` follows it to a target outside the dump
  directory. Validate the dump anchor and use a no-follow/exclusive creation policy;
  do not treat a followed missing target as an absent safe leaf.

No additional concrete overflow or ownership defect was found in the reviewed PNG
implementation: input is capped at 64 MiB, image dimensions at 4096, output/converted
pixel size arithmetic is bounded, owned allocations and streams are released, and
write/flush/close failure is reflected in the result. Path ownership is a separate
caller obligation. This does not constitute malformed-image fuzz qualification.

The SDL audio mixer preserves the eight-channel accumulator model; producer and
consumer cursors/ring state share a mutex, writes are clamped to ring capacity,
volume products and the eight-channel sum fit int32, and callback destruction
precedes mutex destruction. The pinned SDL header defines callback demand in
unconverted input bytes, matching the four-byte stereo-frame calculation. No
additional concrete source defect was found in this bounded review. The backend
is unbound/unrun: device buffering, actual audibility, timing and shutdown behavior
still need integration and appropriate target evidence. Use remains one producer
thread plus SDL callback; it is not a general concurrent init/shutdown API.

This review edited only this report and performed no runtime, inference, console
or Git actions. The two findings above describe the source at review time; later
lead correction receipts supersede their pending status.

## Native GUI implementation receipt

`platform/gui_sdl.c` adapts the real upstream SDL frontend without its Win32/GDI
fallback. SDL/Vulkan and GPU GE initialization must both succeed before `gui_on`
becomes true; failure prints the current SDL error and exits unsuccessfully.
Presentation uses actual `gegpu_present` images, with bounded PSP framebuffer
unpacking for CPU-written movie/menu frames in 5650/5551/4444/8888 formats. Physical
VRAM/RAM span, alignment, stride and format checks precede pixel reads.

The upstream controls remapping/options, cheats, camera/free aim, resolution,
aspect/interpolation, glow, FPS, intro-skip, fullscreen, screenshot-request and quit
menu paths remain connected to their real SDL/runtime APIs. Save/load list dialogs
retain selection/cancel and input-release blocking. Native monotonic timing replaces
QPC/Sleep; scheduler-paced vblanks avoid duplicate pacing, fast-forward retains
the upstream presentation thinning, and held interpolation frames retain their
vblank phases/adaptive fallback. Mod/cheat hooks remain once per emulated vblank.
Modal CPU-frame presentation refreshes pixel conversion instead of reusing stale
converted pixels. No diagnostic server button injection or Windows update service
is linked by this frontend; the updater menu is omitted rather than fabricated.

Actual PS5 and Windows-reference translation units compiled with warnings treated
as errors and generated address metadata from `versions/ULUS10567.toml`. The new
frontend is not yet a linked/running game. Its real twinstick/cheats/mod services,
renderer, scheduler, game memory and save dialog caller must be included by the
native app build. Screenshot capability still follows the renderer's explicit
availability policy. No game data, fixture, emulator, console launch or runtime
qualification was used to establish this source receipt.

## Savedata native implementation receipt

`savedata.c` now retains its actual PSP savedata modes, slot dialog/SFO metadata,
list/file queries, load binding value and existing PSP result-code families while
using a native POSIX adapter in place of Win32 file/directory/time calls. The PC
backend remains available. Directory enumeration uses `host_dir`; file timestamps
come from the selected SDK's real `struct stat`/`timespec` and local-time APIs.

Native operation requires an explicit existing `SR_MEMSTICK` root. On PS5 it must
be under a numeric `/mnt/usbN/homebrew/<app>` path. No cwd fallback or creation of
mount/app/root ancestors occurs. Relative descendants are validated as components
and traversed with descriptor-relative `openat(O_DIRECTORY|O_NOFOLLOW)`; actual
files use no-follow opens, exclusive temporary creation, checked flush/close,
file fsync, atomic rename and parent-directory fsync. Deletion is anchored through
`unlinkat`. This source uses public declared POSIX APIs, not invented directory,
stat or thread layouts and not raw syscalls. Actual PS5 providers, ABI behavior and
selected app-anchor admission remain unqualified until the native app link and
Cinder's provider review; host compilation does not establish console write safety.

Backups copy actual existing files before an overwrite. Native timestamp/serial
names avoid reusing a possibly incomplete backup, copy errors stop the overwrite,
and existing retention policy stays outside `PSP/SAVEDATA`. Filename traversal and
invalid guest data-buffer spans are rejected. Metadata/blob write errors propagate
instead of returning save success. SFO length/key/data bounds are checked before
copying, and absent/invalid data does not get replaced with an empty template.
The inherited virtual 512-MiB memory-stick capacity query is unchanged; it is not
a measurement of free USB space.

Actual PS5, Windows and Linux-reference translation units compiled with warnings
treated as errors. Isolated synthetic Windows checks exercised real save, overwrite,
backup contents, load, SFO title round-trip, PSP bind value, slot listing, delete,
missing-data result and traversal rejection. Only new synthetic files under
`build/mods` were touched. Native POSIX runtime, original-game save compatibility,
console write services and human save/reload gameplay remain untested.

## Independent native H.264/MPEG source review

Read-only review of `platform/h264_ffmpeg.c` and the `SR_NATIVE_GAME` MPEG path
identified these concrete issues for lead correction:

1. Native movie completion still follows empty compressed ring bytes. FFmpeg can
   retain delayed/reordered frames after accepting the last compressed packet;
   `sr_h264_pending_bytes` does not count these. `mpeg_get_avc_au` can set video end
   and stop further decode calls before receive/drain reaches `AVERROR_EOF`.
   Native end-of-video needs explicit decoder drain/EOF state, not compressed-byte
   emptiness. This can truncate actual movie endings.
2. Pixel output bounds cover the entire host arena, including the non-framebuffer
   gap between PSP VRAM and RAM and the upper reserved arena region. Validate an
   entire actual PSP VRAM or RAM span consistently with the native GUI rather than
   only checking host allocation containment.
3. Ring fill passes `SR_HOST(RB_data + writePos * packetSize)` and a uint32 product
   `got * packetSize` directly to native demux/decoder feeds. Existing constructor
   arithmetic also accepts certain oversized/wrapping ring requests. Check ring
   counts and full RAM spans using widened arithmetic before host reads/callbacks.
   This is inherited upstream input validation debt, not proof of an observed game
   failure, but the new native adapter does not itself close that boundary.

The adapter's own PS/ES/parser/packet input budgets are bounded; incomplete PS
packets are retained. Pending packets survive `avcodec_send_packet(EAGAIN)`;
parser output is copied into an owned packet before ES storage can move. The
receive-before-send order and parser flush/drain control are coherent in isolation,
subject to the integration EOF issue above. Decoder thread_count=1/thread_type=0
matches the dependency's intended nonthreaded build. Context, parser, frame,
packet, scale and input allocations have corresponding destruction paths.

PSP pixel channel ordering matches the existing frontend formats. Timestamps
retain the upstream fixed-step/firstTimestamp model (now advanced only for a real
decoded frame); actual PES/FFmpeg frame PTS are not yet used, so accurate variable
rate or reordered timestamp semantics are not established. No original movie,
runtime, console, inference or Git operation was used by this review. Findings
describe the source at review time; subsequent lead fixes require their own
correction receipts.

### Finite re-review after media corrections

The lead's corrections are present: native video completion now consults
`sr_h264_finished` (set only on codec `AVERROR_EOF`), end-of-input allows further
decode/drain calls despite an empty compressed ring, pixel output admits only full
2-MiB VRAM or 32-MiB RAM spans, and native ring construction/fill checks widened
packet arithmetic, callback presence and complete RAM spans.

Two remaining findings were reported:

- ATRAC AU availability still stops on video end or empty H.264-based ring
  accounting even when actual demuxed audio ES frames remain. Native audio end
  needs complete-input plus actual audio-frame exhaustion; video compressed-byte
  emptiness does not establish that the audio stream has ended.
- At native H.264 end-of-input, a retained incomplete recognized PS/PES element
  can be discarded while parser/codec drain reaches clean EOF. Distinguish the
  short resync-padding tail from an incomplete declared element and report
  malformed/truncated input. This is an input-validation issue; no actual game
  movie failure was observed.

No further concrete send/receive EAGAIN, packet lifetime or bounded decode-loop
error was found in this pass. No runtime/console/inference/Git action occurred;
source correction review alone does not establish movie playback.
