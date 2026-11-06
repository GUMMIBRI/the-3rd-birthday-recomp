# PS5 port development

Status: initial source work, **not a PS5 executable or playable port**.

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

Owner: Cinder for initial integration; no separate project lead assigned yet.

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
