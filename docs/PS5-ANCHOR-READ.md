# Read-only app-anchor candidate

Status: original source implemented, actual target translation unit compiled and
a separate native application ELF linked for import inspection. **Not integrated,
converted, installed, executed or qualified for console filesystem access.** Native
savedata/other writes remain blocked. This candidate exposes no mutation API.

## Capability contract

`platform/anchor_read_ps5.h` exposes opaque `SrReadAnchor` and `SrReadFile` handles
and scalar error/size/read-count values. No `DIR`, `stat`, errno macro, file position
or native provider layout crosses the interface. Error classes reuse the existing
`SrNativeDirectoryErrorClass` wire enumeration; raw native codes are diagnostic
only and must not be compared with the game's SDK errno values.

`sr_anchor_read_adopt` requires the caller to have already admitted and opened the
existing app-owned directory. Successful adoption takes exclusive ownership of
**that exact descriptor**. It never reopens the root by pathname, substitutes a
different root or creates an ancestor. Failed adoption leaves ownership with the
caller. This layer does not decide which app owns an arbitrary supplied fd.

`sr_anchor_read_open` prevalidates the entire relative name before native opens:

- At most 1023 path bytes, 255 bytes per component and 64 components.
- No absolute/device paths, empty, dot or dotdot components, controls, backslash,
  stream/wildcard delimiters or trailing dot/space aliases; no truncation.
- Each ancestor is a **single** validated name passed to `_openat` relative to the
  retained root/previous directory fd, with `O_RDONLY|O_NOFOLLOW|O_DIRECTORY`.
- The final name uses `O_RDONLY|O_NOFOLLOW|O_NONBLOCK`; the nonblocking flag avoids
  waiting for a FIFO writer before the following type check. `fstat` must identify
  a regular file with a nonnegative size. Nonregular/unknown failures are rejected.

`O_NONBLOCK` does not make opening an arbitrary object side-effect-free: a device
node is opened before `fstat` can reject its type, and its driver may act during
open. The admitted root must exclude untrusted device nodes; this candidate does
not establish that prerequisite.

Intermediate owned descriptors are closed as the walk advances; the root fd is
retained until explicit release. The final file fd is independent of the anchor
and remains usable if the anchor is released. Handles are owned by one host thread;
concurrent use/release and dangling aliases are outside the contract.
Retaining a descriptor across renames preserves the opened object, but does not
prove that it remains under a particular pathname namespace during concurrent
rename or mount changes. Descriptor identity is not an ancestry proof.

`sr_anchor_read_at` uses the real `pread` export, preserves file position and caps
each request at 1 MiB. It checks signed 64-bit offset arithmetic and bounds reads to
the size snapshot taken at open. Short reads/EOF return their actual count. Native
errors remain errors; only EINTR is retried, at most 16 times. A file changed after
open can still produce a short read; this is not an immutable-asset verifier.

Close/release clear and consume the supplied handle even when close reports an
error. Close is not retried after EINTR, because descriptor close state is not
established for this candidate. Every allocation/open failure path releases owned
intermediates while retaining the original failure classification.

## Provider and ABI evidence

Public source reference:
`mihawk-99/PS5_PayloadSDK` commit
`b83202be73e930050e00de0f1b4d9ec46c0391bf`.
Its `sce_stubs/libkernel.c` declares `_openat`, while `libc/namespace.h` maps
`openat` to `_openat`. The source temporarily applies that same namespace mapping
while including the owning `<sys/fcntl.h>`, so the header declares the exact native
signature rather than a handwritten cross-SDK prototype.

The implementation was compiled using only the selected RADV SDK sysroot/include
for native `stat`, file flags and `sys/errno.h`, with target `x86_64-sie-ps5` and
`-std=c11 -fPIC -fno-stack-protector -fno-plt -O2 -Wall -Wextra -Werror`.
No game-SDK errno/stat layout is substituted into this translation unit.

Selected local provider files under
`ps5-native/gpu-platform-20261030/deps/radv-sdk/ps5-payload-sdk/target`:

| File | SHA256 |
| --- | --- |
| `lib/libkernel.so` | `c6579f2614d8b8f905a673f810a1b562f1432b07397d61c11e876b5648428753` |
| `include/sys/stat.h` | `33b19ac8d547c631f656b707fe2d6f737813884176aca1f71d593a35f3fd8365` |
| `include/sys/fcntl.h` | `d41a582c3b731d1a4faffe12325f552df9be867b6bfbd6817e32d51eafd9927f` |
| `include/sys/errno.h` | `1fefa8eba3a0eaec642e0fca02f9a7e0535cc44b0b389fb90fd84e6e41ac3bb1` |

The selected allowed `libkernel.so` has GLOBAL FUNC definitions for `_openat`,
`pread`, `fstat`, `close` and `__error`. This differs from simply asking a weak
undefined `openat` symbol to resolve at runtime. It is **export-table evidence**,
not implementation/ABI behavior evidence; the SDK stub bodies are not the console
implementation.

## Separate application import proof

Local outputs: `build/anchor-read/anchor_read_ps5.o`, `anchor-read-proof.elf`,
`IMPORTS.txt` and `DISASSEMBLY.txt`. The proof uses the existing genuine application
CRT and PSXS5 native PIE linker script; it does not link the game, a runtime fixture
or a payload presented as a game application. It was never executed.

The ELF is 83,616 bytes; SHA256:
`ce6f27910c2e78d96d0c69f433e2d9a4ec9882551ca9a28e27f52f7b9601ae48`.
`_openat` is a strong GLOBAL FUNC import with `R_X86_64_GLOB_DAT` relocation;
`fstat` and `pread` also have real import relocations. Dynamic dependencies are
only `libkernel.sprx` and `libSceLibcInternal.sprx`. No `libkernel_sys`, bare
`openat`, write/create/rename/delete provider or decoded `syscall` instruction was
introduced. The decoded instruction audit found zero syscall instructions; it did
not infer that from searching arbitrary file bytes.

## Qualification gap and next decision

No console call has established `_openat`'s actual return/errno convention,
`O_NOFOLLOW`/`O_DIRECTORY`/`O_NONBLOCK` behavior, `stat` layout, `pread` offsets or
the app anchor's capabilities. A successful link does not prove descriptor-relative
confinement. Unknown errors fail closed through the wire classifier, but an ABI
mismatch cannot be exhaustively detected by a source wrapper.

The parent/Cinder must independently review the exact source/provider/import
receipt before any integration. Root-anchor acquisition must remain tied to an
explicit existing app-owned root and preserve the descriptor identity. Human-
authorized targeted console evidence would still be required before claiming any
native read service works. There is no authority here to deploy, run, create paths,
repair permissions, fall back to syscalls or enable writes.

Cinder's subsequent source review found the candidate coherent with no concrete
source defect. It remains unintegrated pending actual provider-contract
qualification; that review does not establish runtime ABI, object-type policy or
namespace confinement.
