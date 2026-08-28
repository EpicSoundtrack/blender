# Horde Worker Cleanup Design

## Objective

Keep enabled Hermes workers below disk-pressure thresholds without deleting active work, reproducibility evidence, or the artifacts needed to integrate and review MaterialX batches.

## Trigger and target

Cleanup evaluation runs after every harvest/refill cycle and whenever `/home/horde` reaches 80% utilization. Cleanup stops when the filesystem has at least 15 GiB free or no eligible paths remain. A cleanup failure must not stop unrelated healthy workers or local/Windows verification.

## Protected data

The cleanup process never deletes:

- paths referenced by a live process, including its command line or current working directory;
- active Git worktrees or the source/build tree of an active assignment;
- `matx_tasks` receipts, patches, manifests, logs, SHA files, or the newest two verified bundles for a lane;
- the latest successful artifact and latest failed evidence for every batch;
- project memory, dispatcher state, credential files, environment files, or installed dependencies;
- any path whose ownership or purpose cannot be classified confidently.

## Eligible data

Deletion is restricted to exact paths under an allowlist:

- `/home/horde/.horde-tmp/*` directories explicitly identified as completed verification clones, bundle scans, patch verification, or abandoned builds;
- superseded build directories whose source worktree and receipts are already preserved and whose batch is complete;
- duplicate transferred bundles only when their SHA-256 matches a protected retained copy and they are not required as a prerequisite by an installed tip.

Age alone never authorizes deletion. A path must also be inactive, classified, and outside the protected set.

## Cleanup transaction

For each candidate:

1. Resolve and verify that its absolute path stays under an allowed cleanup root.
2. Record path, classification, size, modification time, owner, and related batch.
3. Check live process command lines and working directories for references.
4. Verify required receipts and retained artifacts before deletion.
5. Delete only the exact approved candidate path.
6. Record success or failure and measure reclaimed space.
7. Re-probe runner counts and filesystem capacity.

Cleanup is serialized per worker. It does not run concurrently with a new worktree creation, bundle import, or integration checkout on the same worker.

## Failure handling

- Permission errors preserve the path and are logged; privilege escalation is not attempted automatically.
- A candidate that changes between inventory and deletion is skipped.
- If free space remains below the target with no safe candidates, raise an immediate capacity incident in the task and Slack.
- If a cleanup affects a running job or required artifact, stop cleanup, preserve evidence, and treat it as a release-blocking incident.

## Evidence and reporting

Each cycle produces a durable JSON receipt containing the worker, trigger, before/after capacity, candidate decisions, deleted paths, reclaimed bytes, skipped paths with reasons, and runner counts before/after. Project memory records material successes and failures so a rejected cleanup technique is not retried blindly.

Every progress update reports cleanup only when it changed capacity, encountered a blocker, or skipped a material candidate. Routine no-op scans remain quiet.

## Verification

Unit tests cover allowlist enforcement, resolved-path containment, active-process protection, artifact retention, duplicate-bundle hashing, age-without-classification rejection, capacity targets, permission failures, and receipt determinism. An operational acceptance test uses fixture directories and real process references without touching production worker data. Production rollout starts in dry-run mode on one worker, then enables deletion after the dry-run receipt matches manual review.

## Rollout

1. Implement inventory, classification, and deterministic receipts with deletion disabled.
2. Validate against fixture tests and one real-worker dry run.
3. Enable deletion for `.horde-tmp` verification directories only.
4. Add superseded builds and duplicate bundles only after separate acceptance evidence.

The existing manually audited cleanup on `canderson-blendit` remains valid evidence, but it does not bypass this rollout gate.
