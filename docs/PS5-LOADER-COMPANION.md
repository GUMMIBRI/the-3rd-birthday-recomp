# Required public native loader companion

Native app packages require `sce_module/libc.prx` in addition to the main SELF.
The kernel explicitly reported its absence on the first PE3 launch with
`0xa0020102` (`PRX_SCE_MODULE_LOAD_ERROR`). Main-ELF imports and valid SELF integrity
did not establish that this external package requirement was satisfied.

This is the source-derived public GPL loader shim, not a proprietary Sony module
or a complete libc. Keep the existing main executable's provider mapping intact.

- Public source: [SynoPiia/PSXS5](https://github.com/SynoPiia/PSXS5/blob/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native/libc_builder.cpp).
- Revision: `5c17ea31aec5791c12451e915386fa98d670adb4`.
- Generator: `tooling/native/libc_builder.cpp`, Copyright 2026 BlackBearReloaded,
  SPDX `GPL-3.0-or-later`. Source dependencies include `hash.hpp` and the runtime
  API/import surface used by the public tooling; retain that source/build context.
- Signed public artifact: 1,284,674 bytes, SHA256
  `e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`.
- Local admitted baseline: the immutable PE1 release's `sce_module/libc.prx`,
  independently matched against the public native application packaging recipe.
  The recipe requires this exact size/hash. This fix reuses those verified bytes;
  it does not claim an independent reproducible rebuild of the signed module.

Package this provenance and the source project's GPL-3.0 license alongside the
module. Do not copy unrelated PE1 game code, data, assets, settings or notices.
The packaging and transfer admission gates require the actual module and notices;
an old manifest lacking them must fail even if its main SELF audit passed.

Adding this component addresses the confirmed missing-file failure only. Human
relaunch is still needed to establish startup and discover any subsequent issue.
