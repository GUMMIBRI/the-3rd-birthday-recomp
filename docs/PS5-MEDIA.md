# Native H.264 dependency

The PS5 movie path can use a real statically linked CPU H.264 decoder rather than
Windows Media Foundation. This dependency implements decoding, not a PSP CPU
execution backend, JIT, packaged emulator or blank-frame substitute. The parent
owns the runtime demux/decode adapter and its integration with `mpeg.c`.

## Selected source

- Official repository: [FFmpeg/FFmpeg](https://github.com/FFmpeg/FFmpeg).
- Stable tag: `n7.1.3`; resolved commit
  `f46e514491172d15bd74b4abb1814cd2f05a763e`.
- Private build checkout: `build/ffmpeg-source` (gitignored).
- Configured license: **LGPL version 2.1 or later**. GPL, version3 and nonfree
  configurations are disabled. Preserve `COPYING.LGPLv2.1`, `LICENSE.md` and
  corresponding selected source/build configuration for any distribution. Host
  GNU tooling is separate GPL3 build tooling, not linked into the game.
- No source content patches were needed. `git diff --quiet --no-ext-diff HEAD --`
  reports no tracked source content difference, and there are no untracked source
  files. Windows stat-cache metadata can produce a nonempty short status despite
  that content check; the recipe checks content rather than resetting source.

## Reproduction

```
./scripts/build_ps5_ffmpeg.ps1 -PlatformRoot <existing-public-platform-prefix> -Jobs 2
```

The recipe accepts an existing Git/MSYS shell and existing Zig host compiler.
Target objects use the retained public LLVM compiler, target
`x86_64-sie-ps5`, and game SDK headers. Zig is used only for host-side build
generators; it does not produce the PS5 libraries. The POSIX `freebsd` configure
platform selects compatible public header paths while the actual compiler target
remains PS5. Assembly and runtime CPU detection are disabled, so FFmpeg reports
its generic C architecture implementation; no SIMD performance claim is made.

The script bootstraps only missing build tools under ignored `build/media-tools`
using official public MSYS2 packages, with exact SHA256 checks:

| Tool package | SHA256 |
|---|---|
| `make-4.4.1-3-x86_64.pkg.tar.zst` | `af0bdba17f06fe037f0194069adaa31a8fe45f1a11381501896aea1fae37bd5d` |
| `coreutils-8.32-5-x86_64.pkg.tar.zst` | `62dfee1c39fd15f99c39802b35e82643bc14fffc16d6c76d4001caa385ec77e3` |

Official package records: [GNU make](https://packages.msys2.org/packages/make),
[coreutils](https://packages.msys2.org/packages/coreutils). Extraction and all
configure scratch files stay inside the project's ignored build tree. No global
package installation or PATH change persists after the script returns.

The selected configuration builds only `libavcodec`, `libavutil` and `libswscale`,
enabling the H.264 decoder and H.264 parser. It disables programs, docs, network,
autodetected external libraries, other codecs/parsers, hardware acceleration,
threads, x86 assembly and shared libraries. Support code selected internally by
the real H.264 decoder remains part of the build. Target link probes use the real
public SDK stubs through `ld.lld`, without ignoring undefined symbols or
pretending unsupported functions exist.

**Thread contract:** FFmpeg's configured threading support is off. Calls into
these libraries must remain serialized on the native game/decoder host thread.
Do not call them from the SDL audio callback or another concurrent worker. PSP
guest threads cooperating on one host thread do not themselves introduce host
concurrency, but the final adapter must retain this rule.

## Actual artifacts

The actual cross-build and installation completed with at most two compile jobs.
The supplied PowerShell recipe was then run successfully against the selected
inputs; it reproduced the same three archive hashes and writes the private
`build/ffmpeg/RESULT.json` receipt. No unchanged game build or gameplay was run.
Installed public headers are under `build/ffmpeg/install/include`; static archives
are under `build/ffmpeg/install/lib`:

| Archive | Bytes | SHA256 |
|---|---:|---|
| `libavcodec.a` | 2,599,916 | `73f443dd3ae88761b67b9a19707011d8ad4163232281aef834130460a1c5a874` |
| `libavutil.a` | 1,308,976 | `d90a4bce8034b9de784f3e5bbb1f9025d32997c0819dc90a6f3c19062a5ffa96` |
| `libswscale.a` | 1,694,586 | `442047b45a0f07ea59e7b7d4ecca1c9ca848fb260f82cac2b363d0fcac43d815` |

The representative actual H.264 decoder object is ELF64 x86-64. Instruction decode
across all three archives found zero inline syscall instructions; undefined-symbol
inspection found zero pthread/TLS references. Final application linkage still
needs provider/import inspection, and the decode adapter must be tested using
actual movie input before claiming correct pictures, timing or playback.

Locally retained configure integrity at the first completed build:

- Upstream `configure` SHA256:
  `8a5841042c43fe68c72b94f65044d0ca0c7bd529024c549fbea10e008e15f5d5`.
- Generated `config.h` SHA256:
  `1c20cdae79251656ba726c325825e6f9d07cc03a7ab5a1b9d25eb91a3c87fe23`.
- Generated `config_components.h` SHA256:
  `43a85f1c7aced8a63dfcf1fe8f68738c79563175ef95d0506b349edf26057dae`.

Build output is source/library evidence only. No game execution, console
deployment, hardware decode or movie playback was performed by this task.
