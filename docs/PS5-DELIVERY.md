# Private native application staging

The local packaging script prepares the real reviewed native game EBOOT and its
required runtime files. It does not access the console, install, launch, publish,
reserve an ID globally, or overwrite an existing application.

The manager selected these **provisional** values for this private candidate:

- Display name: **Parasite Eve 3**. Original game: **The 3rd Birthday**.
- Application ID: `PPSA77025`.
- New app destination, after separate authorization/checks:
  `/mnt/usb0/homebrew/PPSA77025`.
- Tester-owned ISO destination, separately managed:
  `/mnt/usb0/third-birthday-data/ULUS10567.iso`.
- Content ID: `UP9000-PPSA77025_00-THIRDBIRTHDAY000`.
- Development content version: `01.000.001`.

No existing PE1/PE2/SC2 identity, artwork, assets, settings or saves are selected.
These values are not a catalog reservation or proof that the destination is free;
the manager must check the actual console immediately before any installation.

## Reproduce local staging

```
./scripts/package_ps5_app.ps1 -PlatformRoot <existing-public-platform-prefix>
```

Default inputs are the reviewed game conversion under
`build/private-game-20261106-r2/native-link/converted-r1` and the strict
`build/checks/GAME-SCE-VERIFY.json` post-conversion gate. The output is restricted
to the ignored `build/private-delivery-r3` tree. A fresh output directory is
required; an existing candidate is never silently replaced.

The script can stage a `PENDING_POST_SCE_GATE` candidate when that final gate is
missing. Revalidate the same staged bytes later without recopying them:

```
./scripts/package_ps5_app.ps1 -PlatformRoot <existing-public-platform-prefix> -Finalize
```

`READY_FOR_MANAGER_REVIEW` is emitted only when:

1. The current EBOOT size/hash and current SCE ELF hash match the reviewed
   conversion result, which explicitly records disabled native writes.
2. The native game audit matches the conversion's input and reports no inline
   syscalls and exactly one emulated TLS implementation.
3. Retained allocated sections have passed comparison, and the hash-pinned native
   tool reports valid integrity for the current SELF container.
4. The strict post-SCE gate is a boolean pass bound to the same native/SCE hashes
   and import count.
5. Every staged file still matches its private manifest and the conversion result
   itself has not changed since staging.
6. The mandatory original `sce_sys/icon0.png` matches the reviewed 512x512 RGB8
   noninterlaced asset and is present in the staged inventory.

This label means the local static staging checks passed. It does **not** mean
installed, bootable, rendered, playable, save-capable or approved for public binary
release. The mandatory icon is included; actual console registration remains unverified.

## Metadata and included files

The script reads the field schema of the published PE1 Build20 native
`sce_sys/param.json` as a read-only reference and verifies its public source hash.
It constructs a new JSON object with the manager-selected identity; it does not
copy PE1 values or art into the candidate. Shared native schema values such as
free application type and version formatting are retained as technical defaults.
The default age-level field is a schema value, not a rating claim.

Required runtime files are included without alteration:

- Seven tracked `runtime/assets/vfpu/*.dat` tables, at `assets/vfpu/` in the app.
  `recomp.c` uses these paths and aborts if required tables are missing.
- The tracked `jpn0.pgf` and `ltn0.pgf` replacement fonts, at `font/`.
- The upstream runtime `LICENSE` and `CREDITS.md`, plus public component notices
  for the selected PS5 platform, SDL, Mesa/RADV, FFmpeg, compiler runtime, stb,
  xxHash and SDL debug bitmap font. The notices retain their individual terms;
  their presence does not relicense original game content.
- A private readme explaining USB paths, disabled native save/write services,
  and current test limitations.

Cinder confirmed that native registration requires `sce_sys/icon0.png`. The fresh
candidate includes an original 512x512 rectangle/text placeholder, authored for this
project with no game artwork, external font or other project art. Editable source,
deterministic renderer, PNG, MIT license and provenance are under
`runtime/assets/ps5/`. Run `python runtime/assets/ps5/render_icon.py` to regenerate
the opaque RGB8 noninterlaced PNG. A repeat rendering produced identical bytes and
the actual image was visually inspected. No ISO artwork was extracted or changed.
Earlier no-icon candidates remain preserved and are not selected for installation.

The candidate contains no ISO, save/settings template, private logs, credentials,
proprietary Sony module or downloaded emulator binary. The native EBOOT does
contain game-derived translated code prepared from the tester's own input; the
candidate and its file-level hashes are therefore private local delivery data.
The script and this generic schema/provenance document may be kept in GitHub;
`PACKAGE-MANIFEST.json`, `STAGING-RESULT.json`, all app binaries/fonts/tables and
any tester input remain in the ignored build tree.

## Actual local outcome and remaining gates

The script staged a fresh 36-file hash-verified private candidate and passed its current
post-SCE gate. The manifest records source-to-copy hashes for every required
auxiliary file and inventory hashes for all app files. Exact private artifact
hashes and paths are in `build/private-delivery-r3/STAGING-RESULT.json` and
`PACKAGE-MANIFEST.json`, not this public document.

A local negative check substituted a passing-looking receipt with a different
SCE hash: finalization correctly stayed `PENDING_POST_SCE_GATE`. Revalidation
against the genuine current receipt restored `READY_FOR_MANAGER_REVIEW` without
changing the staged app files. This tests staging logic, not a console launch.

Native save/write services remain explicitly disabled. Source preparation pins
planned writable locations to the USB app subtree, but no hardware filesystem
trace or console boot evidence was produced here. Manager-owned installation
still requires a current title/path collision check, current owner authorization,
fresh game-closed confirmation and actual native registration checks. No console action was performed by the staging task.
