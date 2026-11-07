# Pre-entry module-load error — independent source/package review

Question: what can reject the application before startup despite passing native
import, conversion-model and SELF-integrity checks?

The owner's image reports `0xa0020102 / PRX_SCE_MODULE_LOAD_ERROR` for `eboot.bin`.
The manager subsequently reported kernel evidence identifying the missing PRX.
This worker independently confirmed the corresponding concrete package omission:
**the deployed r4 manifest contains no `sce_module/libc.prx`, while the selected
public native application recipe requires that exact loader companion.**

## Verified required artifact

Existing reviewed baseline file:

```
C:/.pe/ps5-native/release-v1.1.6-build20-20261106/PACKAGE-ROOT/PPSA77022/sce_module/libc.prx
```

Actual local read-only verification:

- Length: **1,284,674 bytes**.
- SHA256: `e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`.
- The same artifact identity is recorded in the known PE1 Build21 package.

`native-candidates/recipe/package-application.ps1` makes `CompanionRuntime` a
mandatory argument, validates this exact size/hash, and explicitly copies it to
`app/sce_module/libc.prx`. `native-candidates/recipe/APPLICATION-ABI.md`, under
“Runtime and delivery limits”, requires EBOOT, this companion, parameter metadata
and an icon. It states that the companion is a **loader shim, not a full libc**.
The PE3 package omitted that required component while carrying EBOOT and metadata.

DT_NEEDED-only verification does not prove a complete application folder. The
companion is part of the application loader/process contract; its absence cannot
be dismissed because normal application imports resolve to libkernel and
SceLibcInternal. The existing SCE verifier checks converted loader tables but
does not inspect the surrounding application filesystem. Its passing result
never established companion availability.

## Public source and license provenance

The component's tracked builder is
[PSXS5 tooling/native/libc_builder.cpp](https://github.com/SynoPiia/PSXS5/blob/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native/libc_builder.cpp),
pinned revision `5c17ea31aec5791c12451e915386fa98d670adb4`.
Its header identifies BlackBearReloaded, copyright 2026, and
**SPDX GPL-3.0-or-later**. It deterministically emits the loader-compatible module
from tracked semantic manifests; this is not a proprietary Sony SDK binary.

The local copies at `native-candidates/tool/source/libc_builder.cpp` and the
pinned upstream `tooling/native/libc_builder.cpp` are byte-identical, SHA256
`fb9e4eb66ef6c1d9e9bcfd059a3a3aaf86156fe065838303a0c3fbd789786a1a`.
The corresponding public tool source includes `LICENSE`, `hash.hpp`,
`runtime/api-surface.txt` and `runtime/imports.txt`. Preserve the source pin,
corresponding build/source availability, original copyright and GPL notice.

For the bounded additive package correction, include a dedicated companion notice
and provenance record alongside the GPL-3.0 license text from
`native-candidates/tool/source/LICENSE`. Identify the original author, public
source revision, exact builder and signed artifact identities above. The existing
PS5 PayloadSDK notice describes another component and should not stand in for
this author's attribution. Do not relabel the shim as a full libc or replace
application allocation/stdio providers merely because its filename is `libc.prx`.

## Comparison with the known PE1 native application

Read-only ELF/SELF inspection found the same basic supported converter shape:
FreeBSD ABI 2 ELF64, `ET_SCE_DYNEXEC (0xfe10)`, 14 program headers, 12 SELF segment
entries, SELF program type `0x101`, extended program type 1 and authority
`0x3100000000000002`. Both carry ORBI process-parameter size `0x60`, version 5,
companion SDK `0x08050001` and module SDK `0x02000009`.

The PE3 process parameters start at ELF file offset `0x0b200518`, VA
`0x0b1fc518`; its companion-SDK field is at file offset `0x0b200528`.
PE1 Build21 has the same field value at file offset `0x015d5ef8`.
The larger PE3 text/data image is a real format difference, but this review found
no need to change its entry point, imports, relocation tables or SELF authority
to correct the independently established missing companion.

## Bounded correction and limits

The manager owns the additive `sce_module/libc.prx` plus notices delivery,
current package/manifest coherence and private transfer receipt. Preserve the
existing EBOOT, selected icon/background overlays, parameter file and owned ISO.
Do not deploy a raw emitted companion ELF in place of the checked signed PRX.
No user input, unrelated app operation, automatic launch or rollback is implied.

No console/network call, new build/conversion, target execution, Qwen invocation
or Git operation occurred in this independent review. The missing-file diagnosis
does not establish successful startup after correction or eliminate later runtime
problems. The human supplies the next boot observation.
