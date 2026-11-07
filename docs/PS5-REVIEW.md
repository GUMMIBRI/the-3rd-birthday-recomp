# Independent PS5 portability review

Reviewed 2026-11-06 by the checking subagent. Scope: initial fiber abstraction,
scheduler integration and source-level native platform dependencies. No game,
console, Qwen inference or external service was run. No runtime source was changed.
This review does not establish console compatibility or gameplay correctness.

## Immediate conclusion

The Windows scheduler call replacements preserve the existing native fiber calls,
calling convention and stack sizes. The added null checks prevent switching to a
failed context. No concrete new Windows-path defect was found in this bounded
review. This is a useful abstraction boundary, not a PS5 scheduler implementation.

## Adoption blockers and next actions

1. **The POSIX backend requires 64 MiB allocation per started guest thread.**
   `sched.c` currently requests `64 << 20` bytes at fiber creation; the POSIX branch
   of `platform/fiber.h` calls `malloc(reserve)` instead of reserving address space
   and committing a small initial stack. With `MAXTHREADS = 128`, the theoretical
   host-stack allocation reaches 8 GiB, before the 128 MiB guest arena
   (`recomp.c:30-43`), renderer and decoder buffers. Actual thread count and deep
   call-stack use are not established. Do not enable this backend on console
   unchanged. Measure required stack use or implement a bounded reserve/guard-page
   strategy using the approved SDK facilities. Deleted scheduler slots retain
   their host contexts until reuse/start; account for dormant contexts too.
2. **Context API/ABI and TLS are unverified on the console.** `fiber.h` uses
   `getcontext`, `makecontext`, `swapcontext`, `ucontext_t` and C11 `_Thread_local`.
   Linux object compilation does not establish any of those imports, TLS setup or
   context register preservation on the selected PS5 runtime. A target SDK compile
   and link must prove symbol availability; then a bounded context exercise must
   cover multiple suspended stacks, floating-point/vector values, stack alignment,
   restart/delete and allocation failure. Avoid substituting stub success returns.
3. **The scheduler still has Windows clock/sleep dependencies.** `sched.c`
   includes `windows.h` and uses QPC types/functions, `timeBeginPeriod` and `Sleep`.
   The parent owns the clock extraction. Preserve the existing microsecond virtual
   clock, fast-forward rebasing, 59.94 Hz deadlines and alarm delivery; replacing
   them with yield counts would alter gameplay timing.

## Remaining native platform inventory

| Area | Concrete source dependency | Required implementation decision |
| --- | --- | --- |
| Renderer/presentation | `gpu_sdl3vk/sdl3vk.c:19-23`, Vulkan instance/surface setup around `:962-973`, swapchain extension at `:1017` | Prove a compatible PS5 Vulkan loader/driver and SDL surface path, or supply a native backend that preserves GE behavior. SDL availability alone is insufficient. |
| GPU UI text | `gpu_sdl3vk/sdl3vk.c:484-507` and `:612-653` use GDI DIB/text drawing | Replace GDI-generated menu/toast pixels, even if Vulkan itself can be retained. |
| Compiled game attachment | `driver.c:31-58` loads `game.dll`, resolves entry points and fills `SrApi`; `game_glue.c:9,41,44,47` uses PE exports | Select static linking or a supported console module. Preserve the API version check, function registration, hook and patch-flag access. Changing only the filename cannot load a Windows DLL. |
| Movies | `h264_mf.c:19-22` includes Windows Media Foundation COM interfaces | Supply actual H.264 decoding behind the existing feed/frame contract; keep the MPEG-PS and requested PSP pixel conversions coherent. |
| Audio | `audio.c:13-14,41,56,74-87` uses WinMM, event handles and a Windows feeder thread | Supply a console or SDL audio sink plus synchronization, retaining 44.1 kHz stereo mixing/backpressure. ATRAC/SAS decoding is a separate layer. |
| Saves and memory stick | `savedata.c:95-121` and later enumeration/deletion; `hle.c:967,986,1125`; `hle_ext.c:371-412` | Replace Windows path/enumeration/rename APIs with rooted console filesystem operations. Preserve safe-write/backup behavior and distinguish missing paths from denied mounts. |
| ISO reads | `iso.c:69,168,175-176` uses `_fseeki64`/`_ftelli64` | Map to the approved 64-bit file-offset API with checked read/seek errors. |
| Native dialogs/input | `gui.c`, `osk_win.c` | Replace Win32 dialogs/XInput fallback and keyboard UI with the selected console input/UI path. |
| Optional control server | `ctrl_server.c:13-14,89` uses Winsock | Disable explicitly for initial console build or port to the supported socket API; do not silently open a network listener. |
| Mods/texture discovery | `mods.c:17,60-66`; `gpu_sdl3vk/ge_gpu.c:47,1071-1086`; `texpack_ppsspp.c:22,172-192` | Replace Windows directory APIs and retain data-root confinement. The separate mods subagent owns detailed mod recommendations. |
| Updates/startup discovery | `update.c` and `driver.c:174-194` | Separate Windows launcher/update behavior from console boot and deliberate local asset selection. |

These are source findings, not a claim that every other file is portable. A
console build should maintain an explicit source list and import report rather
than compile every Windows source behind permissive compatibility macros.

## Fiber interface constraints worth retaining

- The backend's current context is `static _Thread_local` inside a header. It is
  valid only when all create/switch/delete calls for a context family occur in one
  translation unit and one host thread, as the header states. If more services
  need the API, move state/functions into one implementation file before sharing
  contexts across translation units; otherwise each caller can see a different
  current context.
- A fiber entry returning aborts intentionally. Scheduler entries loop and yield;
  arbitrary future task functions must not assume normal return is supported.
- The root context has no detach/free operation, and scheduler globals have no
  reset path. Current process-lifetime use matches upstream, but an in-process
  restart feature would require a deliberate lifecycle design.
- The current `fiber_check.c` tests a single child, two yields and one integer
  stack local on Windows. It does not test a console ABI, two live child contexts,
  SIMD state, stack overflow, exception/unwind behavior, or a game restart.

## Recommended next integration milestone

Deliver a target-SDK-linked host foundation with explicit clock, context, memory
and filesystem implementations, plus its unresolved-import report. Preserve the
original generated game ABI and Windows build. Request Cinder review before
adopting context/TLS or renderer assumptions that the public SDK cannot yet prove.
Keep original game data and generated game code out of the public source branch;
retain the upstream GPL-2.0-or-later license and third-party notices.

## Supplemental review: native SysV contexts and clock extraction

The parent supplied a subsequent implementation in `platform/fiber_sysv.c`,
`platform/fiber_sysv.S`, `platform/clock.h` and the scheduler. This section
supersedes the earlier adoption recommendations where `SR_FIBER_SYSV` is selected;
the opt-in ucontext reference backend remains subject to its earlier limitations.
The parent reports target `x86_64-sie-ps5` object compilation, not a linked or
executed PS5 program. This review is source inspection only.

No concrete implementation blocker was found in the bounded supplemental review:

- **Initial stack alignment:** the usable mapping top is rounded down to 16-byte
  alignment, then two eight-byte slots are installed. The saved RSP points at the
  `fiber_enter` address. The assembly `ret` consumes that address, leaving RSP at
  8 modulo 16 at the C function entry, as required by the x86-64 SysV ABI. The
  second slot supplies a return trap. `fiber_enter` explicitly aborts if the task
  returns, so scheduler entries cannot fall through into an unrelated context.
- **Context layout and registers:** seven `uintptr_t` fields occupy offsets 0
  through 48. Assembly saves RSP, RBX, RBP and R12-R15 at those offsets. MXCSR at
  56 and x87 control word at 60 have static offset assertions. These match the
  assembly loads/stores. Caller-saved GPR and SIMD registers are correctly left
  to the C compiler across the out-of-line function call; SysV does not require
  Windows XMM6-XMM15 preservation. MXCSR and x87 control state are initialized
  from the creating context and restored per switch. No unwinding across switched
  stacks is supported or claimed.
- **Guard mappings and bounds:** stack capacity is bounded to 256 KiB through
  64 MiB and 128 children. Page size is checked, rounded capacity cannot overflow
  within those limits, and two guard pages remain `PROT_NONE` around the usable
  interval. Failure of mapping/protection frees acquired resources before return.
  A successful delete unmaps the entire allocation and decrements child count.
  This avoids the old unconditional `malloc(64 MiB)` reference strategy, but
  actual PS5 virtual-memory reservation/commit behavior still needs evidence.
- **Single-thread/root lifetime:** attach establishes an owner via `pthread_self`;
  operations require `pthread_equal` on that owner. Current/root contexts cannot
  be deleted. Detach requires the root to be current and all children deleted.
  Root register state is captured naturally by its first `sr_sysv_swap` call.
  Global context state is now in one implementation translation unit, avoiding
  the reference header's per-translation-unit state issue. This is a single host
  scheduler contract, not a thread-safe context registry; concurrent attach is
  outside its supported use.
- **Clock extraction:** Windows uses the same QPC frequency/ticks and Sleep calls.
  The non-Windows implementation uses monotonic nanoseconds and handles EINTR
  by sleeping the remaining interval. The scheduler's microsecond conversions,
  late-frame accounting, 59.94 Hz period, turbo increment and fast-forward rebase
  formulas remain equivalent to the prior code. No new timing regression was
  identified by source comparison.

Required next evidence remains a target link/import report for pthread, mmap,
mprotect, munmap, sysconf, monotonic clock and nanosleep, followed by a bounded
native context check with the approved runtime. Verify actual stack mapping policy,
alignment, callee-saved integer and FP-control state, multiple parked children,
delete/recreate and detach/reattach. These checks do not establish gameplay or a
console rendering path. The earlier renderer, media, audio, DLL and filesystem
inventory remains applicable.

## Focused renderer integration plan (2026-11-06)

Inspected the actual public PS5 SDL prefix at
`C:/.pe/ps5-native/gpu-platform-20261030/attempt-resolution-mods-20261031/install-sdl`
and its corresponding `sdl3-src`. **SDL_Vulkan is implemented here, not just
SDL_GPU. A complete renderer rewrite to SDL_GPU is not required by the available
platform source.** This is source/archive evidence, not a successful display test.

`llvm-nm --defined-only libSDL3.a` confirms callable definitions for
`SDL_Vulkan_GetInstanceExtensions`, `SDL_Vulkan_CreateSurface`,
`SDL_Vulkan_GetVkGetInstanceProcAddr`, their `_REAL` implementations, the PS5
Vulkan hooks and `CPE_Vulkan_QueryDisplay`. The archive has an unresolved
`vk_icdGetInstanceProcAddr` dependency intentionally supplied by the static RADV
link. Public SDL video bootstrap uses `VK_KHR_display`; the PS5 video driver
forces a fullscreen desktop-sized window. It installs the Vulkan surface hooks
in `src/video/ps5/SDL_ps5video.c` and `SDL_ps5vulkan.c`. Build configuration sets
both `SDL_VIDEO_VULKAN` and `SDL_GPU_VULKAN`.

### Smallest genuine path retaining this game's draw core

1. Retain `ge.c` -> `GeGpuHooks` -> `ge_gpu.c`, its SPIR-V shader arrays, persistent
   color/depth targets and direct VkImage presentation. Keep the existing
   `sdl3vk_get_vk` interface returning the real instance/device/queue/family so
   rasterization and presentation share one device. Replacing the VkImage output
   with an SDL_GPU texture is an additional renderer project, not a necessary
   starting step.
2. Use the pinned PS5 `SDL_Init`/`SDL_CreateWindow(SDL_WINDOW_VULKAN)` path and
   `SDL_Vulkan_GetInstanceExtensions`/`SDL_Vulkan_CreateSurface`. The required
   instance extensions are **VK_KHR_surface and VK_KHR_display**; device extension
   is **VK_KHR_swapchain**. SDL's surface path selects an actual advertised display
   mode through `CPE_Vulkan_QueryDisplay`/`CPE_DisplaySelect`, including unchanged
   hint/environment selection. The driver overrides requested window dimensions
   to actual desktop mode, so use `SDL_GetWindowSizeInPixels`/surface capabilities;
   do not invent a 1080p window or change display selection after bootstrap.
3. Load Vulkan calls through checked dispatch from
   `SDL_Vulkan_GetVkGetInstanceProcAddr` (the static ICD provider). Upstream calls
   Vulkan functions directly. A small dispatch header/TU can preserve source call
   names with `VK_NO_PROTOTYPES` + PFN tables. Load global calls before instance,
   instance calls after creation and device calls through `vkGetDeviceProcAddr`.
   Missing required pointers must fail with the exact function name. Do not assume
   linking a Windows Vulkan loader or the availability of exported `vk*` wrappers.
4. Replace narrowly isolated host services: GDI toast/menu rasterization
   (`sdl3vk.c:484-663`); GDI+ PNG load/save and screenshot encode
   (`:726-849`); `Interlocked*` virtual-input queue (`:335-360`); directory discovery
   in `ge_gpu.c`/`texpack_ppsspp.c`. Use a licensed portable PNG implementation
   with matching RGBA/BGRA semantics and bounded image sizes, plus owned-root
   storage. Preserve in-game menus/controller bindings. Optional screenshot or
   remote-test facilities may be explicitly unavailable initially; image loading
   and UI must not return fabricated success. Do not remove original game draws.
5. Target-compile this coherent renderer batch against the exact pinned SDL and
   Vulkan headers, then link the approved filtered static RADV derivative with
   the deliberate provider extraction Cinder describes. Inspect the actual
   callable provider definitions, dispatch relocations, SDK imports and decoded
   raw syscalls. Only after a genuine linked app exists can a human-authorized
   console check establish presentation. No console action is authorized here.

### Exact qualification gates

- Upstream requests Vulkan **1.1**. SDL bootstrap's display probe requests 1.0,
  which does not itself prove that this driver's game instance supports 1.1.
  Enumerate/query the selected driver and device; fail explicitly if unmet.
- Upstream queries optional `dualSrcBlend`; absence already selects its original
  approximation path. Preserve that existing decision and report feature state;
  never claim unsupported dual-source blending was enabled.
- Prove `R8G8B8A8_UNORM` color/sampling/transfer use, `D16_UNORM` depth attachment,
  host-visible+coherent staging memory, and source/destination blits. Surface
  capabilities must admit requested TRANSFER_DST and COLOR_ATTACHMENT usage and
  opaque composition. Upstream assumes a nonempty format list (`fmts[0]`) and
  a matching present-capable graphics queue; strengthen failures at these seams.
- SDL_GPU's patched nonconformant-RADV opt-in property applies to **SDL_GPU device
  ranking**, not a hidden requirement of the SDL_Vulkan hook. A direct Vulkan
  renderer must retain explicit observed-driver/capability checks and honest
  nonconformance reporting; setting an SDL_GPU property will not configure its
  independently created VkDevice.
- The installed `lib/pkgconfig/sdl3.pc` contains a stale `prefix` pointing at
  `attempt-conformance-opt-in-20261030/install-sdl`, despite inspection of the
  resolution-mods prefix. Use explicit verified include/archive paths or regenerate
  metadata before selecting dependencies. Do not let pkg-config silently choose
  another archive/header pair.
- Preserve source/license notices for the SDL PS5 hooks (altered Zlib code based on
  griches/PS5_DevilutionX revision 4dc807484de4bd1c1020da3d7d67a886c9dd6c3a)
  and exact static RADV member/provider provenance. Reuse only platform services,
  not PE-specific game logic.

Required call families are instance/device enumeration, surface/swapchain,
command pools/buffers, queue submit/present, fences/semaphores, allocation/mapping,
images/views/samplers, descriptor pools/sets, shaders/pipeline/render-pass,
framebuffers, viewport/scissor/blend constants, vertex buffers, push constants,
draws, copies/blits/clears and pipeline barriers. There are 78 distinct direct
`vk*()` call names across the current `sdl3vk.c` and `ge_gpu.c`; resolve the complete
set from source when generating the dispatch table, rather than proving only
`vkCreateInstance` and assuming every draw entry point is callable.

## Implemented checked Vulkan dispatch module

Owned source delivery: `gpu_sdl3vk/vk_dispatch.h` and `vk_dispatch.c`.

- `SrVkDispatch` contains typed PFNs for every one of the 78 direct Vulkan calls
  currently found in `sdl3vk.c` and `ge_gpu.c`, plus `vkGetDeviceProcAddr`.
- The loader obtains its only global entry from
  `SDL_Vulkan_GetVkGetInstanceProcAddr`; global calls use a null instance,
  instance calls use the supplied live VkInstance, and device calls use that
  instance's checked `vkGetDeviceProcAddr` and supplied live VkDevice.
- Each required pointer is checked. A failed stage returns 0 and names its first
  missing function in stderr and SDL error state. It still resolves other stage
  entries so any available destroy pointers remain available for checked cleanup.
  Failure is never reported as success. No draw may continue after failure.
- `SR_VK_DISPATCH` enables aliases; without that define, the renderer's original
  Windows Vulkan prototypes and direct calls remain unchanged. The module itself
  is a real loader, not a fallback graphics implementation.

Parent integration order (not yet wired by this worker):

1. For target renderer TUs, define `SR_VK_DISPATCH` and include `vk_dispatch.h`
   before any other Vulkan header. The header selects `VK_NO_PROTOTYPES` and
   aliases `vkFoo` to `(sr_vk.vkFoo)`. Its implementation suppresses aliases.
2. After SDL has loaded its Vulkan provider/window, call `sr_vk_load_global()`.
3. Call `vkCreateInstance`, then `sr_vk_load_instance(instance)` before any device,
   surface-capability, physical-device enumeration or destruction calls.
4. Create the VkDevice and call `sr_vk_load_device(device)` before queue retrieval
   or resource allocation. Load failures preserve any resolved cleanup pointers;
   check `if (vkDestroyDevice)`/`if (vkDestroyInstance)` before cleanup on an
   initialization failure. Do not dereference absent pointers or call generic
   full renderer shutdown while its required cleanup pointers are absent.
5. After device resources, device, surface and instance have been destroyed, call
   `sr_vk_reset()` before SDL unload. Do not reload/reset dispatch while live
   resources still depend on it. One sequential renderer/instance/device only.

Compiled `vk_dispatch.c` to a genuine `x86_64-sie-ps5` object with `-std=c11
-Wall -Wextra -Werror`, the public PS5 SDK, pinned SDL include prefix and RADV
Vulkan include directory. Object output: `build/checks/vk_dispatch.o`.
Undefined symbols are only `SDL_SetError`, `SDL_Vulkan_GetVkGetInstanceProcAddr`,
`__stderrp`, `fprintf`, and `memset`: **no global Vulkan loader/export dependency**.
A source-to-header set comparison confirms all 78 current call names have aliases.
No application link, ICD runtime, game execution or console operation was performed.

## Portable overlay and dispatch integration delivery

Implemented `portable_text.h/.c` and the non-Windows branches of `label_render`
and `menu_render` in `sdl3vk.c`. Windows retains its existing GDI rasterization.
The portable path draws actual bitmap glyphs into the same opaque BGRA upload
buffers, keeping scaled toast/FPS labels, menu title, tab columns, selected-row
background and colors. Bitmap drawing and rectangle fills clip to width/height
while respecting a separate pixel stride. UTF-8 Latin-1 characters use available
font glyphs; unsupported sequences/codepoints use a visible question mark.
The 8x8 glyphs are scaled to the existing menu sizing contract. This is a bitmap
font appearance change on non-Windows, not evidence of visual console quality.

Font provenance: copied **unmodified** from
`C:/.pe/ps5-native/gpu-platform-20261030/attempt-resolution-mods-20261031/sdl3-src/src/render/SDL_render_debug_font.h`
to `runtime/third_party/sdl_debug_font/SDL_render_debug_font.h`, preserving SDL's
Zlib license notice and Marcel Sondaar/IBM public-domain VGA-font attribution.
SHA256: `235dd799a51dc7f130fd6970a7eed449e3ab6c2e257b2fd81f8889476efd850a`.
The corresponding installed SDL revision metadata says
`SDL-3.4.16-HEAD-HASH-NOTFOUND`; an exact SDL Git revision is **not** established
by that metadata. The content hash identifies the source actually copied; do not
invent a Git revision. The rasterizer is original GPL-2.0-or-later code around
that separately licensed font data.

In `sdl3vk.c`, `SR_VK_DISPATCH` now wires global loading after SDL window/provider
creation, instance loading immediately after successful instance creation and
device loading immediately after successful device creation. Partial-init shutdown
guards each potentially absent callable pointer, releases acquired panel/image/
buffer resources and resets handles. Initialization failure saves the SDL error
before cleanup and restores it afterward. Vulkan-result failure diagnostics now
also set SDL's error text. Dispatch reset occurs after Vulkan cleanup and before
SDL teardown. Windows builds do not enable dispatch aliases by default.

Actual checks for this delivery:

- Portable text translation unit compiled for `x86_64-sie-ps5` with `-std=c11
  -Wall -Wextra -Werror`; output `build/checks/portable_text.o`.
- Synthetic host executable `build/checks/text_check.exe` passed: asymmetric F
  glyph orientation, negative and integer-boundary clipping, sentinel preservation,
  stride padding, explicit tab columns/overflow tabs, newline width and invalid
  font-size rejection. These are bounded raster checks, not game testing.
- Modified `sdl3vk.c` compiled as a Windows translation unit against the pinned SDL
  and Vulkan headers; output `build/checks/sdl3vk_windows.o`.

The parent owns the remaining non-Windows PNG/GDI+ block, screenshot save,
virtual-input atomics and full renderer target compilation. This worker did not
change PNG or screenshot implementations, run a game, link an app, touch the
console, invoke Qwen, or perform Git operations.

## Integrated renderer read-only review

Reviewed the parent's non-Windows PNG/screenshot and atomic queue integration with
shared dispatch and UI resources. No application/game/console execution was
performed. The parent reports successful actual PS5 translation-unit compilation;
this review does not add runtime evidence.

Dispatch initialization is ordered correctly: SDL provider/window, global loader,
VkInstance, instance dispatch, VkDevice, device dispatch, then queue/resources.
`gegpu_init` consumes the same live device/queue through `sdl3vk_get_vk`, so draw
and presentation handles are not mixed across devices. Available cleanup pointers
are guarded after a dispatch-stage failure, and `sr_vk_reset` occurs only after
the presentation layer's Vulkan cleanup. The init wrapper preserves existing
SDL error text across teardown; VK_TRY now creates useful SDL errors as well.

Concrete findings requiring correction before relying on allocation-failure paths:

1. **Screenshot resize can retain freed memory handles.** In `shot_record`, the
   old buffer and memory are destroyed, but only `s_shot_buf` is cleared. If the
   subsequent buffer creation/allocation fails, `s_shot_mem` can retain a freed
   handle. The newly expanded shutdown path then attempts to free that stale
   handle again. Clear buffer, memory, map and capacity together immediately on
   release; publish new capacity only after creation, allocation, binding and
   mapping all succeed. This is source-proven failure-path risk, not an observed
   console failure.
2. **Overlay/screenshot bind and map results are unchecked.** `label_render`,
   `menu_render` and `shot_record` currently ignore results from
   `vkBindBufferMemory` and `vkMapMemory`. A failed bind followed by successful
   memory mapping can leave an apparently usable CPU map attached to an unbound
   VkBuffer, later submitted for copy. A failed mapping must not set screenshot
   capacity/pending or leave a permanent partial UI allocation. Construct these
   resources transactionally or unwind the entire local allocation and return
   failure; retain the first error before cleanup.
3. **Some hand-written initialization failures only log to stderr.** For example,
   no present-capable physical device logs and returns 0 without setting SDL error.
   The wrapper can then restore older unrelated SDL error text. Add named errors
   at those source branches, or clear SDL error at initialization entry and supply
   a meaningful fallback when the failing branch has no SDL diagnostic.

Other important lifecycle constraints (not evidence of a new failure):

- The C11 queue uses correct release/acquire ordering for **one producer and one
  consumer**: slot publication precedes release of write count, and slot reuse
  follows release/acquire of read count. Unsigned counters preserve wraparound
  distance while occupancy is bounded to 64. Existing call sites originate in the
  single control-server command loop. Do not treat this as a multi-producer queue.
- `gegpu_shutdown` waits/unhooks but intentionally does not destroy its renderer
  resources or reset its globals; it assumes process exit follows. Dispatch reset
  and device destruction must occur after GE stops using the device. A future
  native driver that supports in-process restart needs real GE teardown before
  presentation shutdown, rather than reusing stale device/pipeline/descriptor
  handles. Current GUI shutdown paths immediately `_Exit`; that is not a reusable
  resource-lifecycle guarantee.
- Menu/label GPU upload buffers keep the same BGRA contract and bounded raster
  dimensions. Portable drawing clips correctly, but its target compile and host
  synthetic checks do not establish legibility, color appearance or console
  upload/presentation synchronization.

The parent owns fixes; this review changed only the report. PNG decoder/texture
correctness is independently reviewed by the mods worker. No Git, Qwen or console
operations occurred.

## Native RTC and directory HLE implementation delivery

Changed only `runtime/src/rt/hle_ext.c` for this batch. Windows FILETIME and
FindFirst/FindNext branches remain in place; the shared guest-string helper now
also reports truncation, which existing Windows callers may ignore.

Native RTC uses `CLOCK_REALTIME`, validates the returned nanoseconds/range and
adds the exact **62135596800000000 microsecond** Unix-to-year-1 Gregorian offset.
No local timezone conversion or hard-coded timezone is used. `sceRtcSetTick`
converts microsecond ticks directly through Gregorian year/month/day arithmetic,
supporting year 1 through 9999 with leap-century rules and the original fractional
microsecond field. Invalid ticks or realtime-clock failures return errors rather
than a fabricated current time. Windows retains its earlier FILETIME conversion
and earlier year-1601 lower bound.

Native directory HLE uses the reviewed `host_dir` handle interface and typed
`lstat`. `sceIoDopen` rejects truncated/invalid mapped paths, including the parent's
empty-path validation sentinel. Real memory-stick folders enumerate their direct
entries; flattened scratch paths enumerate the containing directory and filter
its existing prefix convention. `sceIoDread` synthesizes `.` and `..` exactly once,
then reports original entry names, directory attributes and file sizes. The eight
handle slots retain explicit open/close ownership and return bad-fd/too-many-open
errors. Native removal now propagates mapped errno failures instead of reporting
all failures as missing. Native status mapping distinguishes missing, denied,
not-directory, invalid, too-long, no-memory, unsupported and generic I/O errors.
Host symlinks/device objects are explicitly unsupported by this FAT-like provider;
they are not silently followed or exposed as fake regular files. Flat-directory
prefix behavior, including implicit component names, remains the upstream design.

Verification:

- Actual `x86_64-sie-ps5` HLE translation unit compiled with `-std=c11 -Wall
  -Wextra -Werror`; output `build/checks/hle_ext.o`.
- Windows translation unit compiled with the same warning policy; output
  `build/checks/hle_ext_windows.o`.
- A harness using the exact source-extracted Gregorian conversion helper passed
  **100788 independent Windows FILETIME reference comparisons** (every month of
  1601..9999), year-1/maximum-tick bounds and Unix-epoch conversion. This checks
  calendar math, not the PS5 realtime provider.
- A harness using the exact source-extracted native directory handlers, actual
  Windows `host_dir` implementation and an explicit host-stat shim passed real
  and flattened listings, synthesized dots, size reporting, prefix filtering,
  closed/invalid handles and missing-directory errors. Fixtures and executable
  are isolated under `build/checks`. This does not validate the PS5 stat/dir ABI;
  that still requires target linking/runtime evidence.

No changes were made to unrelated preexisting HLE placeholder handlers, renderer,
Git state, game inputs, console or Qwen. The parent owns integration with its
separately implemented `hle.c` path provider and static-game driver.

## Native memory patches and static code-mod routing delivery

Implemented only `runtime/src/rt/mods.c` for this batch. Native patch discovery
uses `host_dir` to enumerate regular `.txt` files under `<SR_MODDIR>/patches`,
with case-insensitive extension matching and explicit directory/read errors.
Missing optional patch directories mean no data mods; denied/unsupported I/O is
reported. Windows retains the original wildcard discovery branch.

The parser now validates each complete physical line and complete numeric token.
It rejects trailing fields, unsupported timing (only `once`/`always`), unknown
types, signed unsigned-values, overflowing addresses/values, nonfinite or
out-of-range floats, overlong lines and partial numeric prefixes. A `u16`, `u32`
or `f32` patch must fit wholly inside the original allowed data interval
`[0x08000000, 0x0a000000)`. Thus a four-byte patch at `0x09ffffff` is rejected;
a one-byte patch there remains valid. Float patches record their actual four-byte
write width. Existing vblank timing is unchanged: `once` applies at the first
patch vblank, `always` reapplies each vblank. Memory patches still change data,
not recompiled instructions.

Native static code mods have a collision-free strong-binding contract:

- Runtime `mods.c` owns the sole public `sr_mods_hook` routing entry.
- Compile **only generated `mods_gen.c`** with
  `-Dsr_mods_hook=sr_generated_mods_hook`.
- When enabled code mods cause the generator to emit that dispatcher, compile
  runtime `mods.c` with `-DSR_STATIC_CODE_MODS=1` and link the generated/mod objects
  explicitly. Its strong extern then initializes `g_game_mods_hook`; a missing
  enabled dispatcher is a link failure rather than a weak-null silent omission.
- With no generated code-mod dispatcher, omit `SR_STATIC_CODE_MODS`; data patches
  still work, and the Windows DLL driver retains its existing dynamic pointer
  assignment. Do not globally rename `sr_mods_hook` across runtime/generated code,
  which would reintroduce a collision or route hooks incorrectly.

Checks: genuine PS5 and Windows translation units compiled with `-std=c11 -Wall
-Wextra -Werror`. Native undefined-symbol inspection shows the expected strong
`sr_generated_mods_hook` and `sr_dir_*` dependencies, with no Win32 discovery
imports. An owned-memory harness including the actual `mods.c` passed malformed
and overflow rejection, boundary write sentinels, `once` versus `always` behavior,
overlong physical-line rejection and strong generated-hook routing. It operated
only on allocated synthetic guest RAM and ignored build fixtures, not game data.
No application link, gameplay, console, Qwen or Git operation was performed.

## Native controller text-entry delivery

Implemented new `runtime/src/rt/platform/osk_sdl.c`, exporting the existing
`sr_osk_input(const wchar_t *, const wchar_t *, wchar_t *, int)` interface.
It uses the real native `gui_dialog` controller UI, not a Win32 dialog, synthetic
input or an unconditional success/cancel stub. Human actions select lowercase,
uppercase, digits or punctuation through six-character pages; space, backspace,
accept and cancel are explicit keyboard actions. Character pages allow repeated
entry and return to keyboard actions. No timer or automatic acceptance exists.

The OSK follows `hle.c`'s actual representation: each `wchar_t` element contains
one guest UTF-16 unit even on the PS5's wider-wchar host. Initial Unicode units are
preserved in a separate bounded working buffer, including surrogate pairs.
Backspace removes a complete trailing surrogate pair. New characters cover US
printable ASCII; unsupported existing display characters are shown as explicit
UTF-16 escapes rather than destroying their original data. The menu shows the
current tail, unit count and capacity status. This preserves existing Unicode;
it does not claim a full multilingual keyboard or Unicode font renderer.

The output buffer is copied only after the human selects **Accept text**.
Cancel, unavailable dialog, invalid input/limits and allocation failure return 0
without writing `out`; failures are reported through the existing toast/log path.
Length is bounded to 2..4096 UTF-16 units including terminator, with an initial
text longer than the supplied limit rejected rather than silently truncated.
The caller currently caps fields at 256. The existing HLE caller separately
rewrites its original guest input on cancellation and sets CANCELLED; that caller
behavior was not changed by this module.

Actual PS5 translation-unit compilation passed with `-std=c11 -Wall -Wextra
-Werror`; output `build/checks/osk_sdl.o`. Its undefined imports are the expected
CRT plus `gui_dialog` and `gui_toast`; there are no Win32/system-dialog imports.
No GUI automation, game launch, console write, Qwen invocation or Git operation
was performed. Parent integration must select this source instead of `osk_win.c`
and retain the existing HLE UTF-16/status-machine contract. Human controller/UI
behavior remains untested.

## Post-conversion loader-table verifier delivery

Implemented `tools/verify_ps5_sce.py`. It independently parses the loader-visible
ELF64 program headers and PT_DYNAMIC tables, rather than relying on retained
section bytes. CLI:

```
python tools/verify_ps5_sce.py original-native.elf converted/eboot.elf IMPORTS.json SCE-VERIFY.json
```

Supported format is explicitly pinned to the public PSXS5 revision
`5c17ea31aec5791c12451e915386fa98d670adb4` native converter. The actual format
implementation is `tooling/native/sce_module_writer.cpp` and `elf_object.cpp`;
the `tooling/psbc` directory contains only the linker script in this checkout.
The source copy distributed in `native-candidates/tool/source/sce_module_writer.cpp`
is byte-identical to that pinned writer, SHA256
`c919d29ef08dc4a93f57ce32d44b386c94f91d7d24533829a7f27295a0a75c8e`.
The verifier records that format model; it does not independently attest which
converter executable produced a file.

The verifier is original code with format/NID/hash-model attribution to the
writer: BlackBearReloaded (2026), with portions derived there from SharpProspero,
SvenGDK (2026). Its SPDX license is GPL-3.0-or-later, preserving that source-model
license/attribution. The runtime's GPL-2.0-or-later terms allow GPLv3 selection;
this does not relicense independently licensed dependencies.

Actual checks include:

- Original ELF SHA256 must match its import audit. Converted entry point must
  match and be in an executable mapping. Tables/spans must be file-backed and
  unambiguous; native TLS, unknown converted tags and unknown table widths fail.
- DT_NEEDED module order and DT_SCE_NEEDED_MODULE / IMPORT_LIB records must match
  audited providers, IDs, names, versions and attributes. The converter's
  `.sprx` -> `.prx` normalization is handled explicitly.
- Every converted undefined dynamic symbol must retain the original symbol
  index, binding/type/attributes/value/size and match its exact expected import
  name. NIDs use the real source algorithm: SHA1 of ASCII name plus the public
  16-byte suffix, reverse the first eight digest bytes, base64 without padding,
  replacing slash with dash. Library/module numeric ID encoding uses the
  converter's separate 64-character alphabet.
- Loader SysV hash buckets and chains are reconstructed from canonical
  `NID#library-name#module-name` keys and compared exactly, not inferred from the
  numeric-ID symbol-string alone.
- Dynamic relocations must equal the original relative/symbolic relocations in
  converter order plus exactly eight known ORBI process-parameter pointers. PLT
  relocations must remain unchanged. Process/heap parameter block layouts and
  RELACOUNT are checked. Only the supported x86-64 64/GLOB_DAT/JUMP_SLOT/RELATIVE
  models are admitted; destinations must fit writable mappings, imported symbol
  indices must exist and relative targets must remain mapped. Imported symbol
  runtime addresses remain the console loader's responsibility, not a fabricated
  address check.
- Original initialization/finalization pointers and arrays must be preserved and
  bounded. This is static verification, not an assertion about firmware behavior.

Results on existing artifacts:

- Small host fixture: **16 mapped imports, 30 dynamic relocations**, passed.
- Current real-game converted-r1 artifact: **359 mapped imports, 21201 dynamic
  relocations and 25 PLT relocations**, passed, including the eight known added
  parameter relocations. Exact artifacts/digests remain in ignored local receipts
  `build/checks/HOST-SCE-VERIFY.json` and `GAME-SCE-VERIFY.json`.
- Seven deliberately corrupted small fixtures all rejected: NID byte, module ID,
  relocation destination, relocation type, canonical hash, unknown dynamic tag and
  truncated file. Receipt: `build/checks/SCE-NEGATIVE-CHECKS.json`.

No source/converter bytes, runtime state, console installation, Qwen service or
Git state was changed by verification. These results strengthen post-conversion
import/relocation evidence and do not establish a bootable or playable game.
