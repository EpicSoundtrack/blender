# Task 1 — Horde controller integration report

## Result

Implemented a bounded, dependency-injected `run_controller_cycle` in
`tools/materialx/materialx_horde_controller.py`.

- It treats the supplied worker state as current activity/process evidence.
- It harvests each idle worker before considering it for refill; active and
  already-blocked workers are untouched.
- The injected backend accepts/returns only small categorical contracts.
  Harvest payloads containing logs, prompts, credentials, or unknown fields
  are invalid and fail closed.
- The controller records categorical harvest and dispatch journal events,
  worker state, assigned active batches, and alerts. It never emits prompts,
  raw logs, or credentials.
- It validates queue IDs before any harvest call, rejects a queue ID already
  attached to a current worker, and preserves non-overlap among queued IDs.
- A failed/missing/invalid harvest blocks only that worker. A dispatch failure
  is journaled and blocks only that worker; unaffected workers continue.
- Queue exhaustion is an explicit `queue_empty` alert. Only successful
  dispatches appear in `assigned_batches`.

## Files changed

- `tools/materialx/materialx_horde_controller.py`
- `tools/materialx/test_materialx_horde_controller.py`

## Commit

- `608ef6a74c0 Add fail-closed Horde controller cycle`

## Test-first evidence

The new controller tests were added before implementation and initially failed
because `run_controller_cycle` did not exist. The no-current-batch-overlap
regression then failed before its validation was added. The failed-dispatch
assignment regression then failed before only successful dispatches were
reported as assigned.

## Verification

Focused controller suite:

```text
python -m unittest discover -s tools/materialx -p 'test_materialx_horde_controller.py' -v
Ran 8 tests in 0.001s
OK
```

Full MaterialX tooling suite:

```text
python -m unittest discover -s tools/materialx -p 'test_*.py' -v
Ran 59 tests in 0.163s
OK
```

`git diff --check` also completed without whitespace errors.

## Self-review

An independent read-only review identified two issues: a queued ID could
overlap a current worker's ID, and a failed dispatch could still be represented
as assigned. Both were fixed and regression-tested before the final full suite
run. No remote worker command was invoked.

## Concerns

The controller intentionally supplies an injected backend contract only; a
future operational adapter must perform actual SSH log harvesting and bridge
the existing one-shot dispatcher without widening the sanitized controller
output contract. Existing untracked Horde JSON artifacts were preserved and
not included in this task's commit.

## Independent review repair (2026-07-29)

The independent review required repairs to all Critical and Important findings.
They are implemented in commit:

- `b17ee2cefc9 Repair Horde controller evidence handling`

Repairs:

- Dispatch exceptions now journal `dispatch_failure` with categorical
  `missing` evidence, and malformed dispatch responses journal categorical
  `invalid` evidence. Valid success/failure responses retain
  `command_result` evidence.
- Duplicate non-empty current worker batch IDs are rejected before any backend
  call.
- Queue exhaustion keeps a successfully harvested worker reusable in `idle`
  state while still emitting an explicit `queue_empty` alert. A second cycle
  can refill that worker.
- Tests now assert exact sanitized journal records for successful, failed,
  missing, invalid, malformed, and exception outcomes. Private prompt/log
  strings are asserted absent from controller output.

Repair verification:

```text
python -m unittest discover -s tools/materialx -p 'test_materialx_horde_controller.py' -v
Ran 11 tests in 0.001s
OK

python -m unittest discover -s tools/materialx -p 'test_*.py' -v
Ran 62 tests in 0.258s
OK
```

`git diff --check` completed without whitespace errors before the repair
commit. No live worker command was invoked.
