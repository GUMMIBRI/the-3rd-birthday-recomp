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
