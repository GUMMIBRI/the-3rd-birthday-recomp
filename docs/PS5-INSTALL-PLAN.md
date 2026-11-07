# Proposed private USB installation — Parasite Eve 3

This is a reviewable plan, **not an installation receipt or authorization to
write to the console**. No console/network access, transfer, launch or package
script invocation was performed for the original plan; the current 36-file local
candidate was subsequently staged and statically checked without console access.

The original game is The 3rd Birthday (US, ULUS10567). The owner selected Parasite
Eve 3 as the display name. The app is a private native development candidate;
its game-derived executable, ISO, manifests and fingerprints must stay private.
Public GitHub receives reviewed source and technical documentation only.

## Exact proposed locations and source

| Item | Proposed location |
| --- | --- |
| Provisional title ID | `PPSA77025` — not globally reserved |
| Final console app | `/mnt/usb0/homebrew/PPSA77025` |
| Separately owned ISO | `/mnt/usb0/third-birthday-data/ULUS10567.iso` |
| Original local reviewed EBOOT | `build/private-game-20261106-r2/native-link/converted-r1/eboot.bin` |
| Local staged app | `build/private-delivery-r3/PPSA77025` |
| Private file/hash inventory | `build/private-delivery-r3/PACKAGE-MANIFEST.json` |
| Local staging result | `build/private-delivery-r3/STAGING-RESULT.json` |

Local paths above are relative to `C:/.pe/third-birthday-ps5`. The inspected staging
result reports **36 files**, `READY_FOR_MANAGER_REVIEW`, native writes disabled,
no ISO or save/settings template, and the mandatory original 512x512 icon. Read the actual current manifest
again before transfer; this document is not an immutable selection of later files.
`scripts/package_ps5_app.ps1` stages and checks private local files only. Its
`-Finalize` gate checks the current hash-bound SCE audit and does not install.

The executable expects the planned numeric-USB paths. A different USB mount or
ISO location requires a separately reviewed matching build/plan; do not substitute
an internal path or silently fall back to `/data`.

## Gates before any console mutation

1. Cinder/project lead reviews the exact staged EBOOT, metadata, auxiliary files,
   notices and private manifest. Recheck the conversion result, native import/
   syscall audit, post-SCE audit, SELF integrity and all staged file lengths and
   hashes. No source-cohort or binary substitution is permitted after selection.
2. Verify the mandatory `sce_sys/icon0.png` against the selected private manifest.
   It is the original 512x512 RGB8 noninterlaced Parasite Eve 3 placeholder from
   `runtime/assets/ps5/`, with its MIT notice and provenance. Cinder confirmed
   registration requires this file. No other project or original game art is used.
3. Perform only the necessary **read-only metadata collision check** through the
   approved connection: inspect the actual numeric USB mount, homebrew directory
   and current title/path records for `PPSA77025`, its content ID and the intended
   app. Check that `/mnt/usb0` is the selected existing mount. An access error,
   unreadable record, incomplete inventory or unknown result is **not absence**.
   Do not create mount roots or infer permission from another game's deployment.
4. A collision with another title/owner stops this plan. Do not adopt, rename,
   overwrite or delete that app. Cinder must select/review a new identity and
   rebuild matching metadata/path constants. Existing PE1/PE2/SC2 and other app
   files, assets, saves and settings remain untouched.
5. If an existing `PPSA77025` is positively identified as this same project, make
   a complete recoverable private backup of its current app files, metadata and
   any existing user subtree **before any replacement**. Preserve the file/hash
   inventory and verify the backup. This fresh-install plan does not authorize
   a whole-directory overwrite: prepare a separate exact update manifest that
   carries forward all user data and obtain approval for that concrete update.
6. Present this exact candidate, paths and file manifest for **fresh project
   installation authorization and confirmation that all games are fully closed**.
   A prior deployment's confirmation or another project's approval is stale.
   Do not launch/close a game to obtain the condition automatically. Include any
   required staging-directory creation and same-volume rename in the authorization.

A suitable final approval request, after the gates above have concrete receipts,
is: “Install this reviewed private Parasite Eve 3 candidate to
`/mnt/usb0/homebrew/PPSA77025`, with the separately owned ISO at
`/mnt/usb0/third-birthday-data/ULUS10567.iso`, using the attached manifest and
temporary verified staging. Please confirm installation and that all games are
fully closed. The game will not be launched automatically.” This document does
not ask or record that approval now.

## Recoverable file delivery after authorization

1. Record the approved candidate and current read-only pre-install inventory in
   a private receipt. Freeze the local staged files; compare every upload against
   the selected manifest's exact relative name, byte count and SHA256.
2. Positively confirm the final app destination is absent, then create only that
   exact project folder. Deliver companions, icon and notices first, EBOOT next,
   and `sce_sys/param.json` LAST. Keep param absent on any earlier failure.
3. Upload each manifest file to a unique temporary sibling name in its owned directory.
   Verify completion, length and hash, then rename it to the manifest filename.
   Recheck the full staged file set: all and only the approved files, matching
   hashes, with no credentials, logs, original ISO or empty user templates added.
   Keep the manifest/transfer receipts private and outside the scanned app.
4. Hash verification must use an approved genuine server digest or a reviewed
   scoped readback. Completion/size alone is not a verified hash. Follow Cinder's
   current transfer policy; if no permitted hash-verification route is available,
   record that unresolved gate and stop before final activation. Do not claim a
   hash match from a progress bar or bypass a rejected transfer operation.
5. Handle the tester-owned ISO **separately**. Preserve its original local/source
   copy. If the target ISO already exists, verify its identity without overwriting
   an unknown file. If absent and transfer is authorized, upload as a unique
   temporary file in the owned data directory, verify its complete length/hash
   against the private owned-ISO receipt, then rename to `ULUS10567.iso` only when
   the final name is positively unused. It is not part of the public app package.
6. Reconfirm the selected app remains this operation's fresh folder before the
   final param delivery. Stop on unexpected files or metadata; never overwrite.
   This is recoverable per-file delivery, not an atomic registration transaction
   or race-free scanner guarantee. WFM move may fall back to copy/delete and is
   not used. Cinder reviewed this bounded fresh-folder delivery alternative.
7. Verify final registration files and final inventory through the approved
   read-only checks. Preserve backups, originals and failure evidence. No chmod,
   unrelated cleanup, internal storage writes, rollback or automatic reboot/
   launcher restart is included. Do not delete staging/recovery data automatically
   to make an incomplete result appear clean.

## Handoff to the human

Record file delivery, launcher discovery and human boot separately, including exact private
candidate/manifest receipts and any transfer or registration failure. A successful
upload is not evidence of boot, rendering or gameplay.

**Native save/file/settings/screenshot writes remain disabled** in this candidate
until the storage capability boundary is qualified. Do not enable
`SR_PS5_WRITABLE_STORAGE_VERIFIED` as part of installation. Keep all existing saves
and settings recoverable; this candidate must report write failures explicitly.

The human launches the app and supplies initial boot, display, controller, audio,
movie and gameplay observations. There is no automatic launch or autonomous
playtest in this plan. Save/reload is not an acceptance claim while native writes
remain disabled. Report concrete boot evidence and errors back to Cinder for
review before another console change.
