# Native application dependency selection

The source fork retains its upstream GPL-2.0-or-later grants. The PS5 host
application target deliberately combines compatible later-version grants with
GPL-3.0-or-later platform components and is distributed, if ever released, under
GPL-3.0-or-later terms with complete corresponding source. This does not relicense
third-party GPL-2.0-only code or any game data. Audit each added component before
including it; this host target contains no translated game code.

| Component | Revision / integrity | License and role |
|---|---|---|
| [PSXS5](https://github.com/SynoPiia/PSXS5) native startup and linker scripts | `5c17ea31aec5791c12451e915386fa98d670adb4` | GPL-3.0-or-later, `tooling/native/app_crt.cpp`, `app-symbols.map`, `ps5-pie.ld`, `tooling/psbc/ps5-pie-unwind.ld`; real title application startup, not payload CRT |
| [PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) public platform | `b83202be73e930050e00de0f1b4d9ec46c0391bf` | GPL-3.0-or-later platform `shm.c` and `placement.c` pulled from static archive for explicit direct-memory stack backing |
| Selected public platform archive | SHA256 `a6b6f96ec969a3aa1c9de2dc3d0b744cb50bd44c4e2d1398538b52ee9ba3c00e` | Build checks exact archive before linking; headers must belong to pinned source |
| Project host context switch, clock adapter and ISO file adapter | This branch | GPL-2.0-or-later original changes; no imported emulator execution core |

Build scripts take an explicit existing public toolchain prefix. Dependencies are
not silently downloaded, and no Sony proprietary libraries or SDK are published.
The build uses public import stubs; system modules are resolved on the console.
Selected external source is referenced for rebuilding rather than copied without
notices. A binary distribution requires the selected corresponding source and
license texts; this source milestone publishes no binary package.

The direct-stack backend reserves a window with a guard at both ends, allocates
the full writable interior from the explicit direct-memory pool in 64 KiB units,
and maps it read/write without execute. It retains the 512 MiB aggregate bound.
It does not emulate Windows commit growth. Actual game thread demand and console
pool availability remain qualification work; allocation failure is a real error.

