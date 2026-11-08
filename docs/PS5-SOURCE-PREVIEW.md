# The 3rd Birthday on PS5

This is a source preview of the native PS5 port. Bring your own US PSP disc
image, serial ULUS10567, and build locally. The generated EBOOT incorporates
translated game code and disc-derived material and is kept private.

The owner reached the opening cinematic and gameplay on USB build 142,
built from [afcf28c](https://github.com/GUMMIBRI/the-3rd-birthday-recomp/commit/afcf28c).
Full completion, USB save writes and frame pacing remain unverified. The owner
reported slightly floaty controls. This source preview also includes the
optional display and game FPS counter from ca03276, which compiled for PS5
and awaits a console test.

Use the [PS5 build and audit instructions](PS5-PORT.md) and the
[local preparation recipe](PS5-CURRENT.md). The build requires the matching PS5
platform toolchain and dependencies. Prepare the game from your owned ISO,
compile and link the translated code locally, then convert and audit the EBOOT.
For the tested USB layout, place ULUS10567.iso beside eboot.bin in the game folder.
The source archive includes no installable package. A compatible PS5 homebrew
launch environment and locally assembled app metadata are required.

See the [runtime license](../runtime/LICENSE) and
[third-party notices](../runtime/third_party/README.md).
