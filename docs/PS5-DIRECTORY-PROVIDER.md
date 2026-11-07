# Native directory provider boundary

`platform/native_directory_ps5.c` is an original scalar adapter to the selected
public `ps5platform` directory implementation. Its translation unit must use the
retained **RADV SDK headers**, matching the linked platform archive. Runtime
callers built with the separate game SDK include only
`platform/native_directory_ps5.h`; they never receive `DIR`, `dirent` or `stat`.

The interface supplies opaque open/close handles, next-entry names, an independent
kind enumeration, 64-bit size and the provider's integer errno. The independent
`sr_native_directory_error_class` API maps that integer, using only the retained
provider's errno constants, into wire classes OK/MISSING/DENIED/NOT_DIRECTORY/
TOO_LONG/NOMEM/UNSUPPORTED/INVALID/IO. The caller must use those classes rather
than comparing the number with its separately compiled SDK's errno macros.
The original provider errno is retained for diagnostics. `next` returns
1 for an entry, 0 for end, -1 for error. Every end/error clears output storage.
Null error pointers are allowed; names and info pointers are mandatory. Handles
are trusted live objects owned sequentially by one host thread; they are not a
safe API for arbitrary or stale external pointers.

The adapter calls `ps5_opendir`, `ps5_readdir`, `ps5_dirfd`, `ps5_fstatat` and
`ps5_closedir`, rather than the generic title-refused `opendir` family. The
provider's checked `getdents` parser supplies terminated names; the adapter does
not read `d_namlen` or `d_fileno`, which the pinned provider does not populate.
It skips dot entries and rejects unportable separators, controls, wildcard/stream
delimiters, trailing dot/space aliases, overlong names and unterminated records.

Opening normalizes trailing slashes before `lstat`, rejects a final root symlink,
and compares opened descriptor metadata with that root's device/inode. Each
enumeration step checks the root identity again, and obtains entry metadata with
the provider's no-follow `ps5_fstatat` path. Symlink entries are classified as
`SR_NATIVE_SYMLINK`, not followed. The scalar stat operation likewise uses
`lstat`; it reports final symlinks as such.

Paths of 1024 bytes or more are rejected: the pinned platform's descriptor-path
tracking has a 1024-byte bound. This is deliberately narrower than the generic
runtime's 4096-byte buffer, so it cannot silently lose provider tracking state.
Paths with traversal or ambiguous components are rejected before provider calls.

This is **not** a general filesystem sandbox. Ancestor authority, selected USB app
anchor, no-follow/race-safe subsequent file opens and write policy belong to the
caller. The provider's `*at` compatibility implementation reconstructs paths after
descriptor identity checks; it is not an atomic kernel-relative lookup primitive.
Concurrent ancestor or leaf replacement is not eliminated by these checks.

## Selected public source and license

- Repository: [mihawk-99/PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK)
- Revision: `b83202be73e930050e00de0f1b4d9ec46c0391bf`
- Provider: `platform/src/directory.c`, Copyright 2026 Mihawk,
  GPL-3.0-or-later. Its source SHA256 is
  `a574327bcc9cea199a9c7ba147eeb7a531f468721d0a588da0c3fd64550a4b5b`.
- `platform/include/ps5platform/libc.h` SHA256:
  `7bc041c0252bba75ca7578aaf46f080ef5539855a01f5c29b59f31a35b6b14e8`.
- Selected public platform archive SHA256:
  `a6b6f96ec969a3aa1c9de2dc3d0b744cb50bd44c4e2d1398538b52ee9ba3c00e`.
- RADV SDK `sys/stat.h` SHA256:
  `33b19ac8d547c631f656b707fe2d6f737813884176aca1f71d593a35f3fd8365`.
- RADV SDK `dirent.h` SHA256:
  `438f1342a822bdab93d51b9fd18dca73c9416b9be090323088f58cabf099dfe8`.

The adapter C source uses GPL-3.0-or-later; the scalar header uses
GPL-2.0-or-later. The native application's deliberate GPL3-compatible combination
is described in `PS5-DEPENDENCIES.md`. The existing public PE directory bridge was
read as an ABI boundary reference; no PE game logic or private input was imported.

## Actual validation

Compiled successfully for `x86_64-sie-ps5` using the existing public LLVM and
retained RADV SDK include tree:

```
clang --target=x86_64-sie-ps5 --sysroot=<prefix>/deps/radv-sdk/ps5-payload-sdk/target \
  -isystem <prefix>/deps/radv-sdk/ps5-payload-sdk/target/include \
  -I <prefix>/upstream/PS5_PayloadSDK-b83202be73e930050e00de0f1b4d9ec46c0391bf/platform/include \
  -std=gnu11 -fPIC -fno-stack-protector -O2 -Wall -Wextra -Werror \
  -c runtime/src/rt/platform/native_directory_ps5.c -o <output>/native_directory_ps5.o
```

The retained RADV headers contain `sys/errno.h` and `sys/fcntl.h`, without the
top-level forwarding headers. The provider source includes those actual headers
directly; no game SDK header was mixed into this translation unit.

`llvm-nm -u` reports the expected `ps5_*` provider calls, `fstat`, `lstat`,
`__error`, allocation and string functions; no generic `opendir`, `readdir` or
`closedir` dependency remains. Local output is
`build/native-directory/native_directory_ps5.o`. A static assertion checks the
scalar info record's 16-byte layout.

Source SHA256s at delivery:

- `native_directory_ps5.c`:
  `bc91b4c1100ce1f1d717a5f82e8a86b955f161dd2dad3b623ed8906a65531782`
- `native_directory_ps5.h`:
  `6050387a11eff14a3418015a2fd327ea62635f1db3a9186be8a0078daa40d468`

Compilation and provider-boundary inspection only. No application linking,
console filesystem access, game launch, inference or Git operation was performed.
The runtime's parent-owned `host_dir.c` still needs explicit integration and the
final linked application's imports, relocations, syscall decode and actual
hardware ABI remain separate qualification work.

## Descriptor-relative write-provider investigation

Read-only investigation of the selected game SDK and retained RADV SDK found the
same relevant **defined GLOBAL** dynamic symbols in each stub set:

| Public stub | Relevant exports |
|---|---|
| `libkernel.so` | `_openat` |
| `libkernel_web.so` | `_openat` |
| `libkernel_sys.so` | `openat`, `_openat`, `mkdirat`, `fstatat`, `renameat`, `unlinkat` |
| `libSceLibcInternal.so` | none of these |
| `libScePosixForWebKit.so` | none of these |

The complete selected `*.so` export scan also found no `_mkdirat`, `_fstatat`,
`_renameat`, `_unlinkat`, or corresponding `sceKernelOpenat`/`OpenAt`,
`Mkdirat`/`MkdirAt`, `Fstatat`/`FstatAt`, `Renameat`/`RenameAt`,
`Unlinkat`/`UnlinkAt` variants. A public SDK export inventory is not proof that no
other firmware function exists; no unknown NID or hidden ABI is being guessed.

The `_openat` export is a significant narrower candidate, rather than evidence
that the full relative filesystem API works. The selected public source has:

- `sce_stubs/libkernel.c:278` and `sce_stubs/libkernel_web.c:278`: `_openat`
  function-symbol declarations.
- `libc/namespace.h:84`: internal libc `openat` is mapped to `_openat`.
- `libc/syscalls.c:1841`, `:1871`, `:1901`, `:1921`, `:1941`: standard
  `fstatat`, `mkdirat`, `openat`, `renameat`, `unlinkat` definitions execute
  inline syscall instructions. Linking those static SDK implementations would
  violate the current application runtime contract; they are not an adapter.
- `platform/include/ps5platform/libc.h`: documents that the standard `*at`
  functions are unavailable to the supported title import route, and supplies
  named compatibility implementations instead.
- `platform/src/directory.c:95`: `ps5p_resolve_at` checks a tracked descriptor's
  device/inode and constructs a pathname. `ps5_openat:268`, `ps5_unlinkat:287`,
  `ps5_fstatat:306`, `ps5_mkdirat:315`, `ps5_renameat:324` then call ordinary
  pathname operations. This is useful compatibility logic but does **not**
  provide kernel-atomic descriptor-relative lookup or mutation.

The stub addresses themselves are dummy link metadata and many symbols share an
address; they are not console implementation bodies. Export presence, GLOBAL
binding, or successful static linkage alone cannot establish `_openat`'s title
permissions, flags, variadic mode argument, errno behavior, descriptor-relative
semantics or runtime ABI.

Selected source/stub SHA256 evidence:

| Input | SHA256 |
|---|---|
| game SDK `libkernel.so` | `3bc1a6679e06f935787aec50c7f91d5aa51cd9c833503a07fb4f60130d056dc9` |
| game SDK `libkernel_sys.so` | `4cac93afd35c4f0cd34dc16daedf743ebb5691bb8dc0176c29dbc6cf9573f3a2` |
| game SDK `libkernel_web.so` | `266c51203325ab218750af68bd339622466cd980ae2e86ae868166a628917117` |
| pinned `sce_stubs/libkernel.c` | `668eedf57ebae86843bc2fcf3ca912c9937025dc39eefbfef46585eb8fa61e45` |
| pinned `libc/syscalls.c` | `82bb957e859b7fcba00f6d777fe753a7e6fce7bac864f42f730b130fda187a77` |

### Finite next route

1. Have Cinder review a small, explicit `_openat` provider boundary using the
   chosen provider headers, opaque descriptors and scalar flags/results. A direct
   genuine `libkernel.sprx` import may support existing-root-relative open/create
   without pulling the standard SDK syscall object. Verify its exact import and
   converted NID/provider before any hardware qualification; no implementation
   is authorized or claimed by this investigation.
2. For an opened regular file, the already available `fstat` import can return
   scalar metadata about that descriptor. An `_openat` call with a verified
   no-follow policy could reject links rather than recreating an unverified
   `fstatat` ABI. This is a candidate contract, not an observed result.
3. **Keep native writes disabled now.** The evidence does not supply verified
   atomic relative directory creation, rename or unlink. Importing
   `libkernel_sys`, retaining raw syscall objects, treating pathname wrappers as
   atomic, or replacing failures with success would not resolve this blocker.
4. If Cinder/human qualification establishes only `_openat`, a separately designed
   app-owned flat/versioned storage service could use exclusive-created files and
   complete-record validation rather than assuming rename/unlink. That would be
   a new real persistence design needing explicit integration of original save
   and directory semantics, not a one-line compatibility alias. Precreated owned
   directories can reduce bring-up scope but do not establish arbitrary mkdir
   support or complete PSP memory-stick behavior.

No console call, write, test, syscall implementation, permission change or Git
operation was performed. The concrete remaining blocker is a verified coherent
provider for required relative mutations; no unsupported firmware capability or
runtime failure has been inferred solely from headers.
