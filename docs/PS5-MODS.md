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
