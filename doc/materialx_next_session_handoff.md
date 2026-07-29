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

### Dispatcher

`tools/materialx/materialx_horde_dispatch.py` is a tested one-shot dispatcher.
It currently does:

- fast non-LLM transport probe;
- safe credential persistence through stdin;
- background `hermes_runner.py` launch;
- immediate process check;
- sanitized capacity/journal record.

It does **not** yet perform harvest-and-refill. This is the highest-priority
unfinished operational task.

An initial pure controller exists at
`tools/materialx/materialx_horde_controller.py`, with corresponding
`test_materialx_horde_controller.py`. It has only the small fail-closed
decision core; it is not integrated with remote log harvesting or dispatch.
Continue test-first and connect it to the dispatcher.

At handoff all five Hermes processes were idle. Do not dispatch blindly:

1. Harvest the latest sanitized task log for each worker.
2. Record completion/failure evidence.
3. Select non-overlapping queued batches from the scheduler.
4. Dispatch every eligible idle worker.
5. Verify current process/log evidence.
6. Repeat automatically until a real external blocker is recorded.

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

1. Run the controller unit tests red/green, complete the controller’s remote
   harvest → evidence → queue → dispatch integration, and prove a bounded
   refill cycle with simulated tests before any live launch.
2. Harvest current Horde logs, create distinct homogeneous batches, and
   establish 5/5 active workers. Do not report maximum velocity until process
   evidence confirms it.
3. Add a reusable Windows test wrapper that supplies bundled DLL paths;
   identify actual MaterialX tests in the GPU checkout and run a focused CUDA
   batch.
4. Run fresh local focused tests for native invert and smoothstep with the DLL
   runtime path. Update the ledger only with those fresh results.
5. Review, stage, and commit tooling/code in small upstreamable units. Do not
   commit unrelated user changes.

## Explicit cautions

- Do not use the restricted network route for Horde diagnosis; use the
  approved unrestricted SSH route after a bounded restricted probe if needed.
- Do not leak credentials, token fragments, or hashes in output, journals, or
  commits.
- Do not treat a successful launch PID as task completion or utilization.
- Do not claim a full CTest/GPU/parity result from historical focused tests.
- Do not restart unrelated work or overwrite the user’s existing changes.
