# The 3rd Birthday PS5 project prompt

Build a native playable PS5 homebrew port of The 3rd Birthday from the existing
statically recompiled native PC project. Do not build or package an emulator,
interpreter fallback, or JIT. Keep original game behavior and compile translated
game logic ahead of time; implement genuine host services rather than success stubs.

Workspace: `C:/.pe/third-birthday-ps5`. GitHub:
`GUMMIBRI/the-3rd-birthday-recomp`, branch `ps5-port`. Preserve upstream history,
Windows compatibility, original inputs and existing work. Keep reviewed code and
public technical handoffs committed and pushed. Exclude private inputs, game data,
generated game code, saves, credentials, logs and proprietary SDK components.

Read parent `MANAGEMENT.md`, `IMPLEMENTATION-THROUGHPUT.md`, `PLAYABLE-24H.md`
and this project's `docs/PS5-PORT.md` before work. Historical PE1 deadlines do not
establish a deadline or progress for this project. Current human instructions
authorize this project's implementation and two independent subagents.

Coordinate with Cinder in chat `01a17f57-4c6c-7903-81d1-7c1d2919f263` for current
PS5 SDK/build/runtime/storage rules and review when stuck. The human explicitly
authorized reporting to Cinder and asking for helper support. Cinder alone
dispatches the separate helper account. Preserve all other project leads and work.

Owner map: this chat owns integration, runtime/platform implementation and Git;
`mods` owns only `docs/PS5-MODS.md`; `checks` owns only `docs/PS5-REVIEW.md` and
isolated `build/checks` outputs. Do not overlap mutable files. Deliver finite
reviewable batches; refill with genuine remaining work after completed handoffs.

Use Qwen for bounded testing/review through the currently permitted mechanism.
First consult Cinder's current instructions: a specific prior normal-path approval
denial remains binding. Do not evade it with alternate engines, wrappers, direct
requests, environment or policy changes. If blocked, record the exact limitation
and continue independent implementation. Never claim Qwen ran if it did not.

User authorizes compatible PS1 code reuse from `C:/.pe` and compatible online code.
Retain exact source revisions, licenses and notices; access permission does not
grant redistribution rights. Reuse platform services without importing unrelated
game logic or private data. Do not fetch proprietary Sony SDKs. Console IP is
known to Cinder; use existing connection guidance. Do not touch unrelated console
apps, assets or saves. Coordinate deployment and fresh game-closed confirmation
through current Cinder rules; human owns gameplay qualification.

Immediate batch: extract scheduler timing from Win32; compile Windows regression
and portable reference objects, obtain PS5 context/toolchain guidance, then bind
the verified console backend. Continue to real renderer/controller/audio/media/
game integration. Compilation is source evidence, not playable-game proof.

At checkpoint record implemented/compiled/packaged/installed/human-tested status,
exact checks, blockers and next action in `docs/PS5-PORT.md`. Ask Cinder to review
concrete source/diagnostics when blocked; never repeat unchanged failing approaches.
