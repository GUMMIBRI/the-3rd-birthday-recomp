# xxHash

- Official repository: https://github.com/Cyan4973/xxHash
- Selected tag: `v0.8.3`
- Resolved commit: `e626a72bc2321cd320e953a0ccf1584cad60f363`
- Header: https://github.com/Cyan4973/xxHash/blob/e626a72bc2321cd320e953a0ccf1584cad60f363/xxhash.h
- License: BSD 2-Clause, retained verbatim in `LICENSE` and the header.
- Header SHA256: `17973c0dc49d9854ca26caa191f0e12f7a424b68858d9a78de3860d959d85e4b`
- LICENSE SHA256: `6ffedbc0f7878612d2b23589f1ff2ab15633e1df7963a5d9fc750ec5500c7e7a`

Both files were fetched from the exact official commit without modification on
2026-11-06. `texpack_ppsspp.c` uses `XXH_INLINE_ALL`; no platform-specific external
xxHash binary is required. Existing PPSSPP XXH32/XXH64 algorithms and seeds remain
unchanged. This dependency contains no game data.
