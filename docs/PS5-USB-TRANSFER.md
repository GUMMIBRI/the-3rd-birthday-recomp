# Reviewed-scope private USB transfer tool

`scripts/install_ps5_usb.py` is a **fresh-install file-delivery tool**, not an
installer/launcher registration proof. The source was syntax-parsed and the
manager ran its endpoint-free local plan against the selected r3 manifest;
local validation passed. A later manager-run read-only preflight exposed the old
assumption that every homebrew directory was an app. The bounded discovery fix
below has offline fixture checks only; this worker made no console/network call
and performed no upload. Complete installed-WFM compatibility remains unqualified.

The only final destinations are:

- `/mnt/usb0/homebrew/PPSA77025`
- `/mnt/usb0/third-birthday-data/ULUS10567.iso`

No internal storage, unrelated app mutations, overwrite, delete, cross-directory
move, launch, close, reboot, cleanup or transfer retry is implemented. An existing
app or ISO destination stops the operation and requires a separate backup/update
review. Failed temporary files, partially delivered app files and receipts remain
available for recovery. Native game writes remain disabled.

## Cinder's delivery decision

Cinder approved recoverable file delivery to a positively absent app directory
after fresh project-install authorization. The latest direct human instruction
removes the blanket all-games-closed requirement: do not stop unrelated games.
In-use, denied and unknown responses stop delivery; no forced close or reboot is
performed. Whole-folder atomic activation is not assumed. The tool uploads each
file under a unique sibling `.part` name with `overwrite=0`, streams a complete
remote readback for size/SHA256 verification, then uses the proven sibling rename
API. Companion files, notices and icon precede EBOOT; the separate ISO is completed
before **`sce_sys/param.json` is delivered last**. Exact app file inventories are
checked before metadata and after delivery. Earlier failure leaves final app
metadata absent; a later verification failure may occur after metadata delivery
and must be reviewed without automatic deletion.

This is not atomic, race-free or evidence that a launcher registered the app.
Sibling absent checks and `overwrite=0` uploads reduce accidental replacement,
but the rename API has no proven exclusive no-replace transaction. Coordinate the
project's current delivery slot and actual same-app conflicts with the manager;
concurrent external changes can still invalidate preflight. No unsupported safety
property is inferred.

The WFM frontend exposes `/api/move` with `paths`, `dst`, `overwrite`. Cinder's
review of the [upstream server source](https://github.com/owendswang/ps5-web-file-manager/blob/main/src/filemgr.c)
found that `move_path` can fall back from `rename` to copy/delete on EXDEV. That
reference is unpinned main and does not attest the installed service version.
**This tool does not call `/api/move`** or use a path-traversal rename workaround.

## Inputs and default behavior

The endpoint is supplied externally through `--endpoint` or `PE3_WFM_ENDPOINT`.
There is no hard-coded console address or credential, and the endpoint is omitted
from output/receipts. The service must already be available through the approved
connection; this tool does not establish or weaken authentication.

Endpoint-free local validation used:

```
python scripts/install_ps5_usb.py build/private-delivery-r3/PACKAGE-MANIFEST.json --endpoint "" --receipt build/private-transfer-local-review-r1.json
```

Without an endpoint, it checks the finalized local candidate and records remote
state as unknown. With an endpoint, the default plan uses only GET requests for
listings and small metadata text reads; it never creates an upload/download task,
directory or file. Unknown, denied, malformed or incomplete remote responses fail
closed rather than being interpreted as an absent title.

Local validation requires `READY_FOR_MANAGER_REVIEW`, correct private app identity,
disabled native writes, icon presence, no ISO/user templates in the app payload,
exact staged file names/lengths/hashes and matching finalized EBOOT/metadata hashes.
The app root must match the manifest's sibling `PPSA77025` folder. Symlink inputs,
ambiguous relative names and duplicate case-folded names are rejected.

Remote preflight requires the existing USB/homebrew anchor and a positively
absent `PPSA77025`, including case-folded aliases. It discovers live app candidates
through directory listings rather than declaring every folder an app:

- A regular `eboot.bin` plus regular `sce_sys/param.json` identifies a candidate.
  Successfully read metadata must not contain the selected title/content ID.
  Metadata-only candidates are also checked for collisions before recording their
  incomplete-executable coverage gap. Successfully read identity is not ignored
  merely because its executable is absent.
- Unrelated incomplete packaging, such as an eboot-only app inside a hidden
  `.orbit-bundle-*`, is recorded in the receipt's `excluded` array with its path
  and reason, then skipped without payload inspection or unpacking. Unreadable
  or malformed metadata for an unrelated candidate is likewise an explicit
  coverage gap, **not** an automatic conflict and **not** proof of absence.
  Other candidate metadata continues to be checked.
- An unknown candidate whose own basename is `PPSA77025`, including a nested
  directory or case-folded alias, still blocks the operation. The exact final
  destination must remain positively absent. Any successfully read title/content
  collision still blocks, regardless of packaging completeness.
- Once an app's metadata is validated, scanning stops at that app boundary. Its
  asset, save, archive and payload trees are not traversed or read.
- Known packaging containers (`PPSA35169-Compressed`, `PS5VR-1.0.0-PPSA99177`,
  `PS4`, `PS5SX2-vk-285-118`, `PPSA31371-Compressed`, `PPSA28997-Compressed` and
  `.orbit-bundle-*`) and other ordinary folders may contain nested live apps.
  Only their directories are traversed. Ordinary assets, archives and empty
  containers without app indicators are not automatically treated as conflicts.
- No archive is unpacked and no executable/payload file content is read. Links
  are never traversed. Unrelated links are recorded as excluded coverage; a link
  bearing the selected app's basename still blocks as unknown selected evidence.
- Limits are directory depth **6**, **256** listed directories, **25,000** listed
  entries, **64 KiB** per metadata file and **1 MiB** total metadata bytes. Exceeding
  a bound is unresolved scope, not evidence of absence. Listing/access errors
  remain failures. Known apps do not consume an unbounded asset-tree scan.

The private receipt records scan bounds/counts and explicit excluded paths/reasons.
Cinder approved the narrower fresh-install decision: exact destination positively
absent and no selected ID among successfully read app metadata. This is a **local
selected-USB0 homebrew identity check**, not global absence, a reservation, or a
claim about excluded/incomplete containers, unopened archives or other storage.
Unreadable roots and exhausted scan budgets still stop for review. The latest
offline fixtures checked excluded hidden incomplete packaging, selected unknown
evidence and actual collisions; no network preflight was invoked by this worker.

## Explicit execution gate

Execution additionally requires all of:

- `--execute`
- `--project-install-confirmed`
- An externally supplied endpoint and fresh private receipt path.
- `--iso <reviewed-owned-ISO>` and `--iso-sha256 <private-reviewed-hash>`.

Those flags record the caller's **already obtained fresh human confirmation**;
they do not obtain permission or check game processes themselves. Do not supply
them from a stale authorization. The old `--all-games-closed-confirmed` option is
accepted only as an ignored compatibility flag; it is no longer an execution
gate. Current manager/artwork review must still be complete before invocation.
The ISO must match the reviewed 1,467,547,648-byte
source and supplied hash. Its original copy remains untouched. An existing remote
ISO is preserved and blocks this fresh-delivery tool, even if it may be identical.

Execution creates only fresh, explicit app/data/subdirectories. Every large file,
including the ISO, is hashed/uploaded/read back in 1 MiB chunks; a full ISO is never
buffered in memory. Upload completion is observed with bounded task polling.
Polling observes one task and does not retry a failed transfer. The private JSON
receipt records verified files, any unfinished upload and the stopping stage.
Prepare its existing parent directory before use; do not put private receipts in
public source or GitHub.

## Source-backed API contract and limitations

The upload/list/tasks/download schema follows the inspected project-local prior
WFM client. Cached WFM frontend source confirms `ok` JSON envelopes, listing path,
raw GET `/api/text`, sibling `/api/rename`, `/api/mkdir` and upload/download task
requests. The tool validates envelopes, IDs, names, paths, types and sizes and
does not infer success from HTTP 200 alone. It uses:

- GET `/api/list`, `/api/text`, `/api/tasks`
- POST `/api/upload/prepare` and streamed `/api/upload-file` with `X-WFM-*` headers
- GET `/api/upload/finish`
- POST `/api/download/prepare`, streamed GET `/api/download`, task acknowledgement
- Sibling `/api/rename` and exact-parent `/api/mkdir`

Actual installed-version compatibility remains untested. A different schema,
missing task row, unsupported metadata text response, permission failure or timeout
stops the tool. No arbitrary fallback endpoint is tried. Streaming readback adds
network traffic and must be included in the manager-approved transfer slot.

After file delivery, the receipt deliberately retains `launched=false`,
`registered=false` and `installed_verified=false`. The human launches and reports
boot/control/movie/gameplay observations. The tool does not turn static checks or
completed copies into a playable-game claim.
