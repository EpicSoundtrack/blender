# MaterialX / OVRTX / Cycles — next-session handoff

Updated: 2026-07-29

## Objective

Deliver clean, upstreamable Blender MaterialX authoring with exact OVRTX/Hydra
and Cycles semantics, bulk NodeDef coverage, and GPU-backed parity evidence.
Do not approximate unsupported MaterialX behavior: explicitly reject it and
record the reason.

## Non-negotiable operating gates

Read `doc/materialx_project_operations.md` before any implementation work.
The important gates are:

1. **Continuous execution:** a status update is never a stopping point. A
   completed Horde task must be harvested and the next non-overlapping batch
   dispatched automatically. The project is blocked if approved capacity is
   idle without a recorded blocker and recovery action.
2. **Horde utilization:** all five healthy Horde workers must have current
   task/process evidence before root continues local-only implementation.
3. **Batching:** use homogeneous 8–16 NodeDef batches. Do not work one node at
   a time when direct semantic templates apply.
4. **MaterialX fidelity:** use exact MaterialX semantics. Unsupported forms
   require explicit rejection/evidence, not an approximation.
5. **Evidence:** do not claim parity, completion, or a green lane without a
   fresh command/test result. Update the ledger/journal after each batch.
6. **Capacity blockers:** surface immediately in chat or Slack; perform one
   bounded recovery attempt; keep all unaffected capacity assigned.

## Current verified implementation state

### Node work

- Native Color4 bounded bridge (`image`, `extract`, `convert color4→color3`):
  focused graph/reader tests previously passed, 6/6.
- Native vector remap safe forms (`vector2`, `vector2FA`, `vector3`,
  `vector3FA`): focused reader/lowering tests previously passed, 4/4.
- Hydra remap exact four forms: full Hydra mapping suite previously passed,
  34/34 after the denominator-link correction.
- Hydra vector invert exact four forms: full Hydra mapping suite previously
  passed, 35/35. Exact semantics are `amount - in`, not a mix expansion.
- Native vector invert was corrected to exact `amount - in`; it has rebuilt
  locally but lacks fresh focused test evidence after that correction.
- Native vector smoothstep four forms were added; static review is clean but
  lacks fresh focused test evidence.

Do **not** present the above as current full catalog parity. They are bounded,
historical focused results.

### Catalog / tooling evidence

- NodeDef ledger: 802 validated rows.
- Last known ledger breakdown: Hydra tested 106; Cycles reader tested 11;
  Cycles lowering tested 11; 689 rows remained unclassified. Recompute before
  reporting because tooling files are uncommitted.
- MaterialX tooling suite most recently passed: 51/51.
- New/uncommitted tooling includes the NodeDef ledger, semantic registry,
  batch scheduler, capacity monitor/preflight, Horde dispatcher, and a
  partially started Horde controller.

## Horde state

### Hosts

Use the documented SSH route from `materialx_project_operations.md`:

- `canderson-blend05.ov-agent-farm.svc.cluster.local`
- `canderson-blendit04.ov-agent-farm.svc.cluster.local`
- `canderson-blendit.ov-agent-farm.svc.cluster.local`
- `canderson-canderson-blendit2-bot.ov-agent-farm.svc.cluster.local`
- `canderson-canderson-blendit3-bot.ov-agent-farm.svc.cluster.local`

All five were SSH-reachable during this session.

### Credentials

- Credential file: `C:\Users\canderson\OneDrive - NVIDIA Corporation\Desktop\blendit_keys.txt`
- It has three raw token lines. Never print, hash, or place them in command
  arguments/logs.
- A corrected non-persistent matrix proved every supplied token authenticates
  successfully on all three workers that previously reported 401.
- Correct persistence is one **single-line** `export NVIDIA_API_KEY=...` entry
  per worker; normalize retained `.env` lines to LF and discard malformed raw
  lines. Never write all three raw lines as one variable value.

### Canonical recurring controller (supersedes the old one-shot handoff)

`tools/materialx/materialx_horde_operational.py` is the production entry
point. Start with `--once`; after the five-worker fixture is green, omit
`--once` for recurring supervision:

```text
python tools/materialx/materialx_horde_operational.py --once \
  --config <schema-v2-runtime.json> --credentials <credential.env> \
  --state <canonical-state.json>
```

The runtime requires exactly the five documented workers, a queue watermark
of at least five, registered 8–16 NodeDef family batches, a common source SHA,
and Batch Manifest v2 authority. It performs bounded poll, Completion Manifest
v2 harvest, validation, isolated three-lane integration, same-cycle combined
refill, sanitized alert delivery, and locked canonical persistence. A
prompt-only/minimal batch, exit-zero-only log, stale source, Phase-2 overlap,
or noncanonical state is rejected before credit. With no alert connector,
blockers remain visibly `unsent`; an injected `SanitizedAlertSink` may deliver
through the runtime connector without persisting messages or secrets.

The dispatcher is an internal combined-dispatch primitive. It is not an
operator-facing manual refill loop and cannot independently confer ledger
credit. Credit requires correlated Batch Manifest v2, Completion Manifest v2,
integrated receipt, and current-generation green cadence receipts.

## Windows GPU node

- Host: `10.86.82.63` (direct SSH as `canderson`; no Horde bastion).
- Toolchain installed/verified: VS 2022 Build Tools, CMake, Ninja, CUDA 12.8,
  and `C:\src\blender\lib\windows_x64`.
- CUDA Release build directory:
  `C:\src\blender\build-cycles-cuda`.
- `cycles_test.exe` exists. The Release build completed.
- The test executable initially exited `0xC0000135` because bundled Blender
  DLL directories were absent from `PATH`.
- A temporary runtime path made from all DLL-containing subdirectories under
  `C:\src\blender\lib\windows_x64` fixes startup:
  `cycles_test --gtest_list_tests` exited 0.
- A guessed filter
  `materialx_graph.*:materialx_usdshade_reader.*` matched 0 tests. Locate the
  actual test names/source revision before claiming GPU MaterialX verification.

## Local Windows lane

- Source: `C:\tmp\blender-materialx-core-work`
- Build: `C:\tmp\blender-materialx-core-build-local`
- `cycles_test` rebuild completed after native invert/smoothstep edits.
- Direct local execution exited `0xC0000135` without the bundled DLL runtime
  path. Use the same DLL-path wrapper pattern as the Windows GPU node, then
  run the exact focused native test names.

## First actions in the next session

1. Run the supervisor in `--once` mode against a fresh schema-v2 runtime and
   inspect the canonical state/append-only journal. Then enable recurring mode.
2. Keep at least five validated 8–16 NodeDef family batches queued. Restore a
   stale/blocked worker independently; do not stop the other four.
3. Run the full tooling discovery and native `MaterialX*` focused binary.
   Run local CPU/CUDA composed smoke only when the current milestone marks
   those lanes due; retain distinct numeric receipts.
4. Run Windows A40 CUDA separately when due. Do not infer its state from
   local CUDA or Horde activity.
5. Defer golden-image approval to the final human release gate. This control
   plane proof does not establish complete catalog, OVRTX, add-on, or product
   parity.

## Explicit cautions

- Do not use the restricted network route for Horde diagnosis; use the
  approved unrestricted SSH route after a bounded restricted probe if needed.
- Do not leak credentials, token fragments, or hashes in output, journals, or
  commits.
- Do not treat a successful launch PID as task completion or utilization.
- Do not claim a full CTest/GPU/parity result from historical focused tests.
- Do not restart unrelated work or overwrite the user’s existing changes.
