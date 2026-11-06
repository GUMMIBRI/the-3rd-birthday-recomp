# PS5 port development

Status: native scheduler/context source compiles for PS5, **not a linked PS5
executable or playable port**. Resumption prompt: `../PROJECT-PROMPT.md`.

Fork: https://github.com/GUMMIBRI/the-3rd-birthday-recomp

Upstream baseline: `BasedYuki/the-3rd-birthday-recomp` at
`97c6808f1461178165a227c04fcd5c6eac10fb4e` (v0.9.6).
Development branch: `ps5-port`.

## First implemented change

The cooperative scheduler now uses `platform/fiber.h` for host context attachment,
creation, switching and deletion. Windows retains its existing native fiber calls
and stack sizes. Failed context attachment or stack allocation terminates with an
explicit diagnostic before attempting to switch through a null context.

An opt-in `SR_FIBER_UCONTEXT` reference backend implements stackful cooperative
contexts, using a thread-local current context rather than passing pointers through
`makecontext` integer arguments. It preserves suspended native call stacks and
allows dormant thread contexts to be deleted and recreated. Stacks are eagerly
allocated, unlike Windows stack reservation. At the existing 64 MiB per-thread
setting this needs a console memory-budget decision before adoption.

This backend has **not** been compiled against a PS5 SDK, linked on PS5, or run on
PS5. Availability and behavior of `ucontext`, TLS and stack allocation must be
verified against the selected homebrew SDK. Do not define a PS5 platform macro and
assume it works. `sched.c` still uses Windows timing and sleep APIs.

## Validation of this batch

Using the existing local Zig 0.15.2 C compiler:

```
zig cc -std=c11 -Wall -Wextra -Werror -Iruntime/src/rt tools/fiber_check.c -o build/port-check/fiber_check.exe
build/port-check/fiber_check.exe
zig cc -std=c11 -Iruntime/src/rt -c runtime/src/rt/sched.c -o build/port-check/sched.o
zig cc -target x86_64-linux-gnu -std=c11 -Wall -Wextra -Werror -DSR_FIBER_UCONTEXT -Iruntime/src/rt -c tools/fiber_check.c -o build/port-check/fiber_check_posix.o
```

Windows context round-trip and stack-local retention: passed. Modified Windows
scheduler translation unit: compiled. Linux reference backend: compiled only.
No game execution, full game build, console packaging or console installation.

## Ordered milestones

Owner: this project's lead chat owns implementation/integration and Git; Cinder
provides PS5 guidance and bounded code review. Mods/checks subagents have completed
their isolated reports in `PS5-MODS.md` and `PS5-REVIEW.md`.

1. **Console host foundation.** Verify SDK context facilities and memory budget;
   replace Windows timing, memory mapping, filesystem and dynamic-module calls.
   Acceptance: host-services executable cross-links using public homebrew headers,
   with guest register/stack preservation and file errors handled explicitly.
2. **Native graphics.** The current renderer is SDL3 + Vulkan. Select and implement
   a PS5-supported rendering backend, preserving PSP GE semantics, shaders, depth,
   framebuffer effects, texture formats and presentation. Availability of SDL3
   alone does not establish Vulkan support. Acceptance: console renderer links
   and the human can inspect actual game draws; no fake renderer success path.
3. **Input, audio and movies.** Connect controller, audio output and OS keyboard;
   replace Windows Media Foundation H.264 and Win32 dialogs/update facilities.
   Preserve ATRAC attribution and licensing. Acceptance: linked implementations
   with explicit failure reporting, then human checks controller/audio/movie flow.
4. **Game integration and USB storage.** Compile generated game C for the console;
   replace the Windows game DLL loader, resolve imports, and implement bounded
   USB data/save/config paths. Acceptance: clean-start opening plus save/reload
   demonstrated by the human. Keep prior saves and original inputs recoverable.

Do not package original ISO/EBOOT, generated game code, private saves/settings,
logs or proprietary SDK modules in public changes. Regenerate derived game code
locally from the tester's own game. Existing upstream license and attribution
files remain authoritative; this batch is GPL-2.0-or-later.

## Port search receipt (2026-11-06)

Before creating this fork, GitHub's live repository API reported zero upstream
forks, only the `master` branch, and recent releases v0.9.6/v0.9.5/v0.9.4 each
containing a Windows ZIP. Public searches for this repository with PS5 and
PlayStation 5 found no port. This establishes no public PS5 port found, rather
than proving that no private or unindexed work exists.

## Native host batch (2026-11-06)

Implemented original x86-64 System V cooperative context switching in
`runtime/src/rt/platform/fiber_sysv.c` and `.S`. It preserves callee-saved GPRs,
RSP, MXCSR and x87 control state; constructs an aligned initial C frame and return
trap; validates stack sizes; uses two guard pages; checks same-host-thread use;
and provides root detach after child destruction. It has no CPU interpreter/JIT.
The API assumes trusted live handles and sequential use by one host thread;
concurrent attachment and cross-stack C++ unwinding are unsupported.

`platform/clock.h` replaces direct Windows timing/sleep dependencies in the
scheduler with host clock calls. Windows retains QPC behavior; the console path
uses monotonic clock and interrupted-sleep retry.

Actual `x86_64-sie-ps5` target objects compiled successfully for the scheduler,
native context implementation, assembly switch, and context/FP-control check.
Reproduce with `scripts/check_ps5_host.ps1 -Compiler <clang> -Sysroot <public-sdk-target>`.
The existing Windows context round-trip still passes and the Windows scheduler
translation unit still compiles. The native context check is **compiled only**;
neither its FP assertions nor context switching have run on PS5.

Cinder and the independent checker reviewed the switch/layout and timing changes.
Cinder identified the critical stack commitment limitation: mprotect of a whole
64 MiB range is not proof of Windows-style incremental commitment on PS5. The
backend now accounts for all writable stack bytes under a conservative 512 MiB
aggregate ceiling, returning allocation failure beyond it. At 64 MiB per context
this admits at most eight child contexts. This is an explicit safety bound,
**not evidence that the game's thread demand or the console's mapping budget fits**.
No demand-page commitment behavior is claimed. The next required work is to verify
the selected mmap/mprotect/thread/clock providers and establish the game-compatible
stack backing policy, then link a native host executable and audit imports/syscalls.

Qwen testing was requested but **not run**: Cinder confirms the prior normal-path
automatic approval rejection still blocks invocation; no permitted current route
is available. Do not bypass it with another invocation or policy changes. The
independent helper is unassigned and Cinder exclusively coordinates it.

No console writes, game launch, installation, package or public binary release.
Current renderer/media/audio/storage/game-loader integration remains incomplete.

## Additional port search (2026-11-06)

Public web and GitHub repository searches found no native PS4 game port. The
other [HD PC launcher](https://github.com/kaiwolft/3rd-BIrthday-Launcher) states
that it uses PPSSPP; [PSP2PS4-FPKG](https://github.com/jsimesenmac/PSP2PS4-FPKG)
describes PSP emulators for PS4. These are not native PS4 game implementations
and are not being used as this port's execution architecture. No other native
game codebase was established by this search.

## Native application link milestone

The host context check now links as a real title application with pinned public
PSXS5 startup and linker scripts. Its 16 dynamic imports resolve to public
libkernel/LibcInternal stubs. Static audit finds no inline syscall instructions,
TLS sections/symbols/relocations, or payload startup definitions. The SDK shared
memory platform supplies explicit direct-memory stack backing, guard reservations,
64 KiB rounding and bounded lifetime. This removes the Linux mmap commitment
assumption. The provider's void destroy function does not expose direct-memory
release failure, so hardware leak-free cleanup is not established.

Local conversion produces an SCE application ELF (type 0xfe10) with matching
entry, six retained allocated sections and preserved BSS. The signed plaintext
SELF inspection reports valid integrity. These artifacts contain a host-services
check, **not the game**; no title ID, package, deployment or console run exists.
Post-conversion symbol/relocation/provider inspection must be expanded before a
real game release; retained-byte comparison alone does not validate loader behavior.

Reproduce linkage with `scripts/link_ps5_host.ps1 -PlatformRoot <public-prefix>`
and static checks with `scripts/audit_ps5_host.ps1`. The latter deliberately
requires this small host target to be TLS-free; adding SDL/game TLS requires a
separate reviewed runtime contract. Dependency revisions and license selection
are recorded in `PS5-DEPENDENCIES.md`.

Portable ISO file access now compiles for PS5 and preserves 64-bit positions,
checked seeks and read errors. Windows synthetic checks cover an 8 GiB seek,
ISO lookup/read/EOF and failure cases. This is not original game progression.

Next integration: retain the existing native Vulkan GE renderer using the actual
PS5 SDL_Vulkan provider, connect checked dispatch, replace GDI text/PNG and mod
filesystem APIs, then link these genuine rendering services with the host app.
