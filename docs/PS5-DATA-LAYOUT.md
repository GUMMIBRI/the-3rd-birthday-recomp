# Next-successor data layout policy and current gaps

Chosen stable folder: **`third-birthday-data`**. Preserve the already established
name and existing flat `ULUS10567.iso` placement. Do not rename the current USB
folder, active saves or explicit settings merely to adopt the new layout.

The next successor should ship a skeleton beside `eboot.bin`:

```
PPSA77025/
  eboot.bin
  third-birthday-data/
    README.txt
    ULUS10567.iso             # personal install only; not in public packages
    textures/
    patches/
    modpacks/
    save-import/PSP/SAVEDATA/ # manual import staging, not active save storage
    runtime/vfpu/
    runtime/font/
```

Public output includes only licensed runtime resources, empty structure/readmes
and placeholders, never the original disc or private save/import content. A
`modpacks` staging folder does not imply a new mod format or automatic installer.

Default data root: the app-contained `third-birthday-data` folder. Optional
whole-folder read-only locations are `/data/third-birthday-data` and
`/mnt/usbN/third-birthday-data` on available numeric USB mounts. Supporting reads
there does not enable internal writes or authorize migration/publication. Current
personal deployment remains USB-only.

Selection must preserve explicit compatible paths. Automatic discovery must
validate one complete candidate, distinguish absence from unreadable data, and
report multiple complete candidates as a conflict. Never combine partial roots,
silently choose between ambiguous copies, unpack archives or create mount roots.
The app-contained skeleton alone must not mask a complete relocated personal
folder. Report missing required files clearly.

## Implementation inventory

| Area | Actual current behavior | Next-successor gap |
| --- | --- | --- |
| Disc | Compiled explicit ISO path under USB0's external `third-birthday-data` | App-contained default and whole-root discovery are missing |
| App mount | Compiled selected USB0 app root; generic validator accepts a single numeric USB digit | Available-mount discovery and app-relative launch root are missing; multi-USB is not established |
| Internal read root | Driver rejects non-USB ISO paths | Reviewed `/data` read support is missing; no internal write permission follows |
| Texture/patch overrides | `SR_MODDIR` points to existing app `user`; texture and patch readers consume its subfolders | Bind to the chosen complete data root while preserving explicit/legacy selection without merging |
| Runtime resources | VFPU tables use `assets/vfpu` or `PSP_VFPU_TABLES`; fonts support `SR_FONTDIR` and current font paths | Group licensed resources in the named folder and bind their actual existing environment interfaces |
| Public package | Current recipe copies resources to generic `assets/vfpu` and `font` | Named skeleton, readme and resource grouping are not implemented |
| Manual save import | No dedicated staging or validated import workflow | Add manual staging and explicit validation/copy flow; never use staged imports as active saves automatically |
| Active saves | Existing app `user/memstick`; native mutations remain disabled | Preserve active paths and baselines; storage qualification is a separate blocker |
| Selection diagnostics | No complete-root manifest/discovery or ambiguity resolution | Implement bounded required-file checks and missing/unreadable/conflict reports |

This is a queued compatibility policy, not implemented discovery. No build,
console file move, save migration, internal write enablement or runtime test was
performed for this policy. The installed app and its current explicit ISO path
remain unchanged. Prioritize actual boot failures and Cinder's current integration
holds before scheduling the successor implementation.
