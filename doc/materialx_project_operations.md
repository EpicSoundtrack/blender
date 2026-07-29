# MaterialX Project Operating Memory

## Delivery rule

Prioritize batch throughput over serial node work. Implement compatible MaterialX
node families together, generate bulk tests, run local CPU tests continuously,
and use GPU/render plus fresh `WITH_MATERIALX=OFF` validation as batch gates.
Never repeat a completed audit or test unless the relevant code or environment
changed.

## Compute rule

Use all available local, GPU, and Horde capacity. Keep bounded, non-overlapping
work assigned to each healthy worker: implementation, bulk-test/coverage, and
review. Do not report a worker as active without process or log evidence.

## Mandatory agent-utilization gate

- Before each implementation turn, inspect agent status and assign every free
  local slot a bounded independent task unless a concrete dependency makes
  parallel work impossible.
- When an agent finishes, immediately assign its next implementation, test,
  review, commit-audit, or release-preparation task. Idle capacity while such
  work exists is a process defect.
- A build, review, or external dependency must trigger independent work on
  the other agents; it is never a reason to pause the program.
- Every progress update must include active-agent assignments, test evidence,
  blockers, and a next task for each freed slot.
- An actionable review finding is an immediate repair-dispatch event.
- Before reporting reduced utilization, state the exact dependency and the
  attempted alternative.
- Horde capacity counts only after both checkout revision and active runner
  status are verified. Stale workers may perform read-only audits but are not
  implementation progress.

## Velocity operating protocol

### Capacity and queue discipline

- Keep every available local agent slot assigned. Maintain three queues at all
  times: implementation, verification/review, and integration/commit audit.
- Dispatch the next queued task before an agent finishes its current task when
  the dependency is known. No completed agent may remain unassigned for more
  than one progress turn.
- Split work into node-family batches. A batch should normally contain 3–8
  compatible NodeDefs, one shared lowering pattern, and one chained test.
  Use a smaller batch only for an architectural boundary or a traced defect.
- A task that has not produced a red/green result within 20 minutes must be
  decomposed, handed to another agent, or escalated with exact evidence.

### Verification cadence

- Every completed implementation batch requires its focused CPU test evidence.
- After every three completed node-family batches, run a serialized local
  regression slice covering all changed reader/graph/Hydra tests.
- After every five completed batches or any renderer-facing architectural
  change, submit the current batch to the Windows GPU node. Record either a
  pass/fail result or a concrete reachability/authentication failure.
- A local focused pass is not a Windows GPU verification substitute.
- Before release claims, run the full local suite, the Windows GPU batch, and
  the deferred visual/golden-image gate.

### Build and review control

- One named agent owns each shared build directory at a time. Other agents
  may edit or prepare tests, but must not invoke the same build target until
  ownership is released.
- Reviewer findings are triaged immediately: P0/P1 gets a dedicated repair
  agent, P2 is queued with an owner and deadline. A review cannot silently
  become a stall.
- Keep a separate read-only commit-audit lane. It groups verified hunks while
  implementation and tests continue; do not wait for all node work before
  planning commits.

### External-worker control

- Probe the Windows GPU node at the start of each verification cycle. If it
  cannot be reached, make one unrestricted probe, record the outcome, and
  continue local work without repeatedly rediscovering connectivity.
- For Horde, verify checkout revision and active runner before dispatch.
  Stale checkout repair is a bounded setup task; meanwhile use healthy workers
  for read-only audit, test-plan, or review tasks.

### Progress-report contract

Every update must report: active slots and assignments; batches completed
since the prior update; exact tests run; Windows/Horde status; blockers with
owner; and the next queued task for each slot. If any slot is idle, state why
and the recovery action in the same update.

## Horde connection and runner

Use the unrestricted SSH route:

```text
ssh -o BatchMode=yes -o ConnectTimeout=12 -o StrictHostKeyChecking=no \
  -J horde@bastion.horde-gke.nvidia.com:2222 \
  horde@<worker>.ov-agent-farm.svc.cluster.local
```

Dispatch Hermes through `/home/horde/matx_tasks/hermes_runner.py` using a
base64-encoded prompt. Verify with:

```text
pgrep -af 'hermes.*--yolo' | grep -v 'pgrep -af' || true
```

An SSH success does not prove Hermes is healthy. Hermes HTTP 401
`Invalid proxy server token` is a credential/proxy-token failure, not a VPN,
bastion, or worker-connectivity failure. Do not repeatedly rediscover it;
refresh or install the already-mapped worker credential and retry the runner.

## Verified current resource facts (2026-07-27)

- Local GPU: NVIDIA RTX 5000 Ada Generation Laptop GPU, 16 GB VRAM.
- SSH-reachable Horde workers: `canderson-blend05`, `canderson-blendit04`,
  `canderson-blendit`, `canderson-canderson-blendit2-bot`, and
  `canderson-canderson-blendit3-bot`.
- Hermes verified active: `canderson-blend05`, `canderson-blendit04`.
- Hermes proxy-token 401 previously observed on: `canderson-blendit`,
  `canderson-canderson-blendit2-bot`, and
  `canderson-canderson-blendit3-bot`.
- The rotated-key file is user-provided sensitive material. Never print keys,
  key prefixes, or values in logs or responses. Reuse the established mapping
  procedure rather than re-hashing unlabeled keys as a rediscovery step.

## Credential-operation record (2026-07-27)

- Success: sourcing `/home/horde/.hermes/.env` before
  `hermes_runner.py` validated Hermes on `canderson-blendit`.
- Failure: an SSH-stdin `read` method did not deliver a key to the remote
  command. It can print a false `updated` result and must not be reused.
- Failure: SSH reachability is independent of Hermes credential validity;
  `canderson-canderson-blendit2-bot` and
  `canderson-canderson-blendit3-bot` still returned Hermes HTTP 401 after
  the attempted `.env` updates. Do not repeat source/proxy diagnosis; use the
  known worker-key deployment procedure or obtain the missing assignment.
- Rule: validate every credential change with a no-edit Hermes prompt, then
  record the exact worker/result before dispatching production work.

## Verification record

- Phase 1 commit `e7b158b05ea`: native suite 120/120 and installed Blender
  suite 5/5 passed.
- Phase 2 unary Hydra commit `9a520ea7126`: normal and fresh
  `WITH_MATERIALX=OFF` Hydra CTests passed.
- Current uncommitted vector2/place2d batch: fresh MaterialX-off
  `cycles_hydra` CTest passed after the omitted-scale regression correction.
- GPU pixel parity is not yet proven. A Windows CUDA runtime hang requires an
  external all-thread debugger capture; do not claim GPU parity from CPU/Hydra
  success.

## Connectivity alert policy (2026-07-28)

- Default escalation channel: direct Slack message to Charles Anderson
  (`UQVFU6FN0`). Do not send test or status chatter.
- Send one concise alert for each distinct worker outage after the normal
  probe and one unrestricted retry have both failed. Include worker, failure
  class, the retry made, and the smallest user action needed.
- Send at most one recovery notice when that same worker becomes reachable.
  Suppress repeats unless the failure class changes or a new verification cycle
  begins.
- A missing local build runner is a setup blocker, not a connectivity outage;
  report it in the regular project update rather than alerting on Slack.

## Windows GPU runner procedure (10.86.82.63, added 2026-07-29)

- The runner is reachable directly from the primary session as
  `canderson@10.86.82.63`; verify with a bounded `ssh ... hostname` probe.
  Do not route provisioning through an agent authorization wrapper after it
  rejects a directly supplied user approval.
- Explicit user authorization obtained: install Visual Studio 2022 C++ Build
  Tools, CMake, Ninja, CUDA Toolkit 12.8.1, and initialize
  `C:\src\blender\lib\windows_x64` on this host. Keep machine changes
  limited to those Blender/Cycles GPU-test prerequisites.
- Start long provisioning through the direct primary-session route in the
  background and write stdout/stderr to
  `C:\Users\canderson\blender_gpu_provision.{out,err}.log`; monitor the
  process and logs rather than repeating installation commands.
- After provisioning, verify `cl`, `cmake`, `ninja`, and `nvcc`; configure the
  minimal CUDA-enabled Cycles target first, then run the smallest GPU test
  before scheduling a full Blender build. Surface any new blocker immediately
  in chat or Slack; do not silently retry it.

## Approval escalation policy (2026-07-29)

- Default route for a new required approval is a direct Slack message to
  Charles Anderson (`UQVFU6FN0`), not a stalled terminal session or a passive
  chat update.
- The message must state the exact action, scope, material risk, and the
  smallest approval phrase needed. Send it once per distinct action and resume
  immediately after approval.
- Previously authorized implementation, testing, and reversible worker setup
  continues without waiting. Use the approval route only for a genuinely new
  authority boundary or a tool-enforced escalation.

## Overnight delivery failure and corrective controls (2026-07-29)

### Failure record

- The overnight shift delivered useful verified node batches but missed the
  intended catalog/Cycles throughput. The principal causes were repeated
  non-gating Windows-runner provisioning, small unplanned batches, and build
  ownership friction. Reporting activity instead of a node-delivery ledger
  concealed that gap until morning.
- Never treat runner repair, broad audit, or repeated environment diagnosis as
  a substitute for landed catalog coverage. They are sidecar work only.

### Non-repeat rules

- Every overnight run starts with a written node ledger: selected NodeDefs,
  owning worker, test command, and explicit success metric. New work may not
  displace a ledger item unless it fixes a red test or prevents a MaterialX
  semantic incompatibility.
- One build owner executes all local rebuilds. All other workers stay on
  independent test-first implementation, review, reader coverage, or catalog
  selection; they do not retry locked builds.
- Windows/Horde setup receives one bounded attempt per shift. After that it is
  background-only unless it gates an already-green local batch. It must not
  consume an implementation slot.
- Every morning update begins with: catalog nodes landed overnight, exact test
  totals, remaining ledger nodes, and any slots that were idle. It must not
  lead with infrastructure activity.
- If a planned batch is semantically unsafe (for example, MaterialX range
  gamma/boolean clamp without matching IR), explicitly reject it and replace
  it immediately with the next safe ledger batch. Do not burn the remaining
  shift debating or approximating it.
- A repeated failure mode is added here immediately with its evidence and a
  mechanical prevention rule before work continues.

### Authoritative NodeDef ledger (added 2026-07-29)

- There is one checked-in per-NodeDef ledger, updated in the same change as
  every implementation, rejection, and focused test result. It records the
  NodeDef ID; Cycles reader, Cycles lowering, and OVRTX/Hydra states; semantic
  disposition; exact test evidence; and next owner/action.
- A NodeDef is never called complete from an agent report or recollection. It
  is complete only when its ledger row contains every required state and test.
- Start each shift by generating supported, explicitly rejected, and remaining
  counts from that ledger, then allocate work only from remaining rows.
- After each batch: update the ledger, run the batch test, and report the
  changed counts. If a mechanical count cannot be produced, repair the ledger
  before reporting progress totals or selecting another batch.

### Batch-only delivery rule (added 2026-07-29)

- New implementation work must be a semantic family manifest with at least
  four catalog NodeDefs. The task names the shared lowering template, all
  NodeDef IDs, reader/lowering/Hydra scope, and one parameterized focused
  test matrix before any code is written.
- A one-NodeDef task is forbidden unless it repairs a failing family test,
  removes an unsafe semantic approximation, or fixes a release-blocking
  regression. The task must cite that failing test or blocker.
- Each family produces a reusable registration table and parameterized tests;
  adding a sibling variant is data registration, not a new bespoke code path.
- The scheduler allocates only `template_ready` ledger rows. Discovery and
  complex semantic design are separate sidecar lanes and cannot consume an
  implementation slot during a delivery shift.

### Math-utility fast path (added 2026-07-29)

- Treat the large simple-math portion of the MaterialX catalog as a finite set
  of lowering templates, not individual features: scalar/vector/color unary;
  binary componentwise; scalar broadcast; min/max/clamp; remap; mix/subtract;
  conversion; and component compose/separate.
- Once a template is semantically proven, land all compatible NodeDefs through
  one registration manifest in 8-16-node batches. Sibling forms do not get
  bespoke implementation/review cycles.
- Validation for a proven template is one parameterized graph/reader or Hydra
  matrix covering all registered IDs, literal and linked input classes, and
  one compile/run. Deeper CPU/GPU render work is reserved for a new template,
  a red result, or a release-golden fixture.
- The backlog prioritizes template-ready math utilities before complex texture,
  closure, geometry, or semantically unsafe forms. Any worker choosing a
  simple utility must extend an existing template/manifest unless it proves a
  new template is required.

### Compiler/registry architecture is a project gate (added 2026-07-29)

- The primary deliverable is a shared MaterialX-to-renderer compiler surface,
  not a manually maintained list of per-NodeDef implementations. NodeDefs are
  data registrations over a typed semantic IR wherever their definitions fit
  an existing operation signature.
- Before accepting a new bespoke NodeDef branch, the implementer must show why
  it cannot be expressed by the shared typed registry: operation, input order,
  broadcasting, conversion, interpolation, coordinate, texture, closure, or
  graph-composition template. If it can, registry/template work is mandatory.
- Cycles lowering, USD MaterialX reading, and OVRTX/Hydra mapping must consume
  the same semantic classification/registration data. Divergent hand-authored
  catalogs are a gate failure unless a renderer-specific capability is recorded.
- The entire 802-NodeDef catalog is continuously classified into
  `direct_template`, `composed_template`, `renderer_specific`, or
  `explicit_rejection`. The next implementation queue is generated from these
  classes, not selected ad hoc from individual names.
- Per-NodeDef bespoke tests are prohibited for a proven template. Generated
  registration cases plus representative CPU/GPU golden fixtures are the
  required validation model. A new bespoke test exists only for a new semantic
  template, a discovered regression, or a renderer-specific edge case.

### Blender upstreamability is a project gate (added 2026-07-29)

- Every change must be shaped for direct Blender-source review: existing
  naming/style, SPDX headers, existing CMake/test conventions, no private
  runtime dependency, no external service requirement, and no generated source
  committed unless Blender's source tree already uses that generation model.
- Keep responsibilities separated and reviewable: MaterialX semantic registry
  and classification; USD reader; Cycles lowering; Hydra/OVRTX mapping; and
  tests/tooling. Renderer-specific code may consume shared semantics but must
  not hide a second divergent registry.
- New abstractions must be small, local, documented by their public contract,
  and justified by removing duplicated renderer code. Do not introduce a
  broad framework to accelerate a single batch.
- Commits are small and dependency ordered: shared semantics first, consumer
  integration second, tests/evidence with the affected layer. Stage only the
  relevant hunks from the shared worktree; never sweep unrelated dirty changes
  into an upstream candidate.
- Before any upstream handoff, run Blender formatting/source checks and the
  focused native/Hydra tests, provide a concise design note explaining
  MaterialX semantic fidelity, and identify every explicit unsupported form.

### Horde utilization is a hard project gate (added 2026-07-29)

- Before root starts or extends any implementation batch, probe the documented
  Horde workers and dispatch a bounded non-overlapping ledger batch to every
  healthy, credential-valid idle worker. This is a gate, not a preference.
- A worker counts as active only with current `hermes --yolo` process evidence
  or a live task log. SSH reachability, a submitted PID, or an old report does
  not satisfy the gate.
- If a healthy worker is unassigned, root work pauses long enough to assign it
  and record its task; it does not continue with local-only implementation.
- A failed worker must have one recorded bounded retry and an immediate Slack
  alert when user action is needed. Other healthy workers remain fully
  utilized; do not wait on the failed worker.
- Every update includes: healthy workers, active process/log evidence, assigned
  batch, and idle workers with a recovery action. Omitting this evidence is a
  gate failure, not a reporting shortcut.

### Continuous execution and automatic refill is a hard project gate (added 2026-07-29)

- A successful dispatch is not completion. The project remains blocked until a
  controller harvests the finished task log, records evidence or failure, and
  dispatches the next non-overlapping scheduled batch to every healthy idle
  worker without waiting for a user prompt.
- Root may not report the project as on track, at maximum velocity, or fully
  resourced unless every approved lane has current work evidence or a recorded
  external blocker with an immediate escalation and recovery action.
- A status update is never a terminal execution point. It must be followed by
  the next queued operation; stopping at a checkpoint is a gate failure.
- The controller must fail closed when a result cannot be harvested: mark the
  worker blocked, preserve the sanitized log, alert immediately, and continue
  assigning all unaffected workers.

### Rotated Horde inference credentials are an immediate gate (added 2026-07-29)

- When the user provides a credential file, treat it as authorized operational
  input: inspect only its structure, never print or hash its values, then
  validate every candidate against each failing Hermes worker with a bounded
  no-write probe.
- Persist only a validated credential to `NVIDIA_API_KEY` in each affected
  worker's `/home/horde/.hermes/.env`, then run a no-edit Hermes probe and
  record process/log evidence before dispatching production work.
- An unlabeled file is not a reason to wait when a bounded compatibility probe
  can determine a working credential without disclosure. Do not ask the user
  to identify keys they cannot identify.
- New node implementation is gated until all SSH-healthy workers either have
  successful Hermes evidence or a recorded credential failure with an immediate
  Slack/chat escalation.

### Capacity blockers must be surfaced immediately (added 2026-07-29)

- Any condition that leaves a local agent, Horde worker, GPU runner, build
  lane, or test lane idle is surfaced immediately in this chat or by direct
  Slack message. Do not spend a second diagnostic attempt before reporting it.
- The first alert states the affected capacity, observed evidence, whether
  productive work can continue elsewhere, and the smallest user action needed.
  It must not contain secrets or raw credentials.
- After the alert, make one bounded recorded recovery attempt. If it fails,
  keep all unaffected capacity assigned and wait only for the explicit external
  action; never silently poll, rediscover, or substitute infrastructure activity
  for node delivery.

### Mandatory success/failure journal (added 2026-07-29)

- Every completed operational action and every node batch appends one journal
  record in the tracked delivery ledger: timestamp, action or batch ID,
  outcome (`success`, `failure`, or `blocked`), exact command/test evidence,
  and the reusable procedure or mechanical prevention rule.
- A prior `success` is the required starting procedure for the same class of
  work. A prior `failure` may not be retried until its record identifies what
  is different; otherwise use the recorded fallback and continue productive
  work.
- Status updates cite the latest journal records for changed state. No result
  may be reported from recollection alone.

## Operational journal — 2026-07-29 resumed session

### Catalog evidence

- **Success:** a fresh authoritative ledger validation against the provisioned
  MaterialX libraries completed with exit 0 and exactly 802 rows. Current
  counts are: Cycles reader tested 15, Cycles lowering tested 15, Hydra tested
  106, native Cycles CPU-tested disposition 11, combined native/Hydra
  CPU-tested disposition 4, and unclassified disposition 685. These counts
  replace the stale 11/11/689 handoff values but are not a full-parity claim.
- **Success:** commit `3fd18a5d6c0` bulk-reconciled positive native
  implementation/test evidence against the passing 190-test Windows batch.
  Cycles reader and lowering evidence increased from 15 to 197 NodeDefs;
  Hydra remained 106 and combined native/Hydra evidence increased from 4 to
  97. Four rejection-only and six implementation-without-positive-test IDs
  remain deliberately uncredited. Independent review passed with fresh
  ledger validation 802/802, native graph/reader 190/190, and focused Python
  ledger/semantic tests 5/5.

### Horde recovery

- **Failure:** the first harvest command used an invalid remote `cut`
  delimiter and falsely printed `LOG=missing` for all workers. **Prevention:**
  select the newest no-space task path with
  `ls -1t /home/horde/matx_tasks/<pattern> | head -1`; do not reuse the
  timestamp-plus-`cut` parser.
- **Failure:** historical 401 task logs were initially described as current
  credential failure. **Prevention:** an old log is harvest evidence only; it
  is not current health evidence. Current credential health requires a fresh
  no-edit Hermes probe.
- **Failure:** two inline remote `python3 -c` credential probes lost quoting
  through PowerShell/OpenSSH and produced syntax errors before authentication.
  **Prevention:** never embed the probe or persistence program in the SSH
  command. Transfer fixed non-secret helper scripts, pass credential candidates
  only through stdin, suppress authentication output, and return only the exit
  category.
- **Success:** the helper-based candidate matrix validated a supplied rotated
  credential, persisted exactly one LF-normalized
  `export NVIDIA_API_KEY=...` line, and passed a post-persistence no-edit
  Hermes probe on `blendit`, `blendit2`, and `blendit3`. Existing persisted
  credentials also passed fresh probes on `blend05` and `blendit04`.
- **Success:** all five workers were SSH-reachable and received distinct
  read-only tasks with active process evidence:
  `mx-audit-blend05-20260729`, `mx-audit-blendit04-20260729`,
  `mx-catalog-unary-20260729`, `mx-catalog-binary-20260729`, and
  `mx-catalog-compose-20260729`.
- **Success:** `mx-audit-blend05-20260729` and
  `mx-audit-blendit04-20260729` completed with explicit
  `__MATERIALX_EXIT_CODE__=0` sentinels. The results identified prior dirty
  Hydra work but made no source changes; they remain audit evidence only.
- **Failure:** `mx-catalog-unary-20260729`,
  `mx-catalog-binary-20260729`, and `mx-catalog-compose-20260729` ended with
  proxy HTTP 401 responses even though the preceding helper probes succeeded.
  **Prevention:** a credential probe is not production-ready evidence unless
  it invokes the same runner, environment-loading path, model, and proxy route
  as the production batch. Diagnose and validate the exact
  `hermes_runner.py` path on one worker before refilling all affected workers.
  Never treat a wrapper exit code alone as success; scan the sanitized log for
  authentication failure categories before accepting its sentinel.
- **Success:** commit `dc589229a3f` hardened live harvest so authentication
  and proxy failure categories take precedence over an exit-0 wrapper
  sentinel. The controller blocks and suppresses refill for that worker while
  persisting only sanitized categories. Test-first evidence: focused 23/23 and
  full MaterialX tooling 75/75 passed; independent review remains required
  before integration is considered complete.
- **Failure:** independent review of `dc589229a3f` found that the first
  matcher missed common 401 formats and its tests mocked categories rather
  than executing the real classifier. **Prevention:** classifier tests must
  execute the exact production script against representative raw log
  categories followed by a false exit-0 sentinel.
- **Success:** repair commit `682000c1b67` added one shared secret-free
  production classifier and executed it in tests against three representative
  401 formats followed by exit 0. Each returned only `auth_failure`; focused
  tests passed 24/24 and full tooling passed 76/76. A second independent review
  remains required before integration is considered complete.
- **Failure:** the second independent review found three unambiguous bare 401
  forms (`HTTP 401`, `HTTPError: 401 Client Error`, and a request status 401)
  that the repaired classifier still trusted when followed by exit 0.
  **Prevention:** the executable classifier regression corpus must cover both
  descriptive authentication wording and bare HTTP-status forms; any
  unambiguous status 401 is `auth_failure` regardless of wrapper exit.
- **Success:** commit `c18f29fc99c` repaired the bare-status gap. Final
  independent review executed the exact production classifier against six
  exit-0 401 forms, a proxy failure, and a benign success: all categories were
  correct. Focused dispatch/controller/operational tests passed 35/35; every
  failure blocked and suppressed refill, benign success refilled once, and
  persisted records contained only sanitized categories.
- **Constraint:** worker checkouts are heterogeneous. `blend05` and
  `blendit04` contain prior dirty work; the other three are clean at another
  revision. Until exact-branch worktrees are synchronized, these workers count
  only as read-only audit capacity, not landed implementation capacity.
- **Failure:** the original rotated-key probe trusted the Hermes process exit
  code, but Hermes returned exit code `0` while its response contained an HTTP
  401 authentication failure. **Prevention:** a credential probe succeeds only
  when sanitized output contains the requested standalone `READY` marker and
  contains no authentication-error category; process exit alone is never
  credential evidence.
- **Failure:** setting a candidate only in the subprocess environment did not
  test that candidate. Hermes deliberately prefers its persisted
  `~/.hermes/.env` value for provider credentials. **Prevention:** validate
  candidates in an isolated temporary `HERMES_HOME` that contains the worker's
  production config and only the non-secret runtime selectors
  (`NVIDIA_BASE_URL`, `HERMES_INFERENCE_PROVIDER`, and `HERMES_MODEL`).
- **Success:** the corrected isolated-home, response-validating probe accepted
  all three supplied rotated candidates on `blendit`; the helper regression
  suite passed both the false-zero rejection and isolated-home tests.
- **Failure:** a valid `.env` update can be overwritten during worker startup.
  `/etc/init-hermes.sh` rematerializes `.env` from the Kubernetes-injected
  `NVIDIA_API_KEY`. **Prevention:** do not persist while the initializer is
  active, and update the injected worker secret for restart durability.
- **Failure:** after stable persistence and a constant-time exact-match check,
  the real-home and production batch probes still returned authentication
  failure. Metadata-only inspection found a matching
  `credential_pool.nvidia` entry, and Hermes runtime source selects that pool
  before environment fallback. **Prevention:** rotate or replace the supported
  NVIDIA credential-pool entry as part of persistence, then require a real-home
  no-edit probe. Updating `.env` alone is insufficient while a stale matching
  pool entry exists.
- **Success:** an stdin-only helper used Hermes' supported credential-pool
  add/remove functions plus its env writer to replace the stale NVIDIA pool
  entry without placing credentials in process arguments or logs. Fresh
  real-home no-edit probes returned the standalone `READY` marker on
  `blendit`, `blendit2`, and `blendit3`.
- **Success:** after credential recovery, the three workers received distinct
  bounded read-only ledger-planning tasks with live process evidence:
  `mx-unary-plan-v2-20260729` (PID `3785420`),
  `mx-binary-plan-v2-20260729` (PID `3633498`), and
  `mx-compose-plan-v2-20260729` (PID `3474019`).
- **Success:** the full five-worker gate was restored with two additional
  non-overlapping, bounded read-only tasks and live process evidence:
  `mx-native-residual-audit-v2-20260729` on `blend05` (PID `135034`) and
  `mx-hydra-ledger-gap-v2-20260729` on `blendit04` (PID `149637`). Both prompts
  explicitly prohibit edits, staging, resets, cleaning, and builds in their
  pre-existing dirty checkouts.
- **Constraint:** this repair does not change Kubernetes state. Worker restart
  durability still requires rotating the injected `NVIDIA_API_KEY`, because
  `/etc/init-hermes.sh` rematerializes `.env` from that injected value during
  startup.

### Windows GPU recovery

- **Failure:** the isolated GPU worktree contained an empty
  `lib/windows_x64` placeholder, so CMake reported missing precompiled
  libraries. **Prevention:** inspect both library trees before configuring a
  new worktree; when the target is verified empty, junction it to the already
  provisioned `C:\src\blender\lib\windows_x64` tree rather than downloading or
  copying dependencies.
- **Success:** the isolated `materialx-gpu-source` worktree imported the
  reviewed Git bundle at `4bc6d203c66`, remained clean, and exposed the native
  MaterialX tests.
- **Success:** the library junction resolved all 61 provisioned entries and
  the CUDA/Cycles configuration completed successfully in
  `C:\src\blender-materialx-gpu-build` with MaterialX, USD, GTests, and CUDA
  enabled.
- **Failure:** the yielded local command cell that owned the remote Ninja
  build was unavailable after session compaction, so its final exit output
  could not be recovered. **Prevention:** every remote build must write its
  stdout/stderr and numeric exit status to persistent files on the target; a
  yielded local cell is monitoring only, never the evidence store.
- **Failure:** the first post-compaction inline PowerShell process probe lost
  its `$p` variable to local interpolation, and a follow-up inline `cmd`
  query had invalid quoting. **Prevention:** transfer a fixed `.ps1` probe and
  execute it with `-File`; do not place stateful PowerShell or nested quoted
  filters in SSH command strings.
- **Success:** persistent target-side probing recovered the finished
  `cycles_test.exe` from the CUDA-enabled build on the NVIDIA A40 Windows
  node. With the 30 required bundled DLL directories on `PATH`, the exact
  native invert/smoothstep MaterialX graph and reader batch passed 4/4 in
  302 ms with exit 0. Evidence is stored at
  `C:\src\blender-materialx-gpu-build\materialx_focused_tests.log`.
  This proves the CUDA-enabled Windows build and native semantic tests; it
  does not by itself prove CUDA render parity.
- **Success:** the fixed build probe established that Ninja had exited and
  `.ninja_log` ended with the `bin/tests/cycles_test.exe` link step. The
  resulting executable was present at
  `C:\src\blender-materialx-gpu-build\bin\tests\cycles_test.exe`.
- **Success:** the Windows runner's CMake cache had Cycles, CUDA, MaterialX,
  USD, and GTests enabled; `nvidia-smi` identified an NVIDIA A40 and exited
  successfully.
- **Success:** the exact focused invert and smoothstep MaterialX graph/reader
  batch passed 4/4 on the Windows runner (302 ms, exit 0). The durable test
  log is
  `C:\src\blender-materialx-gpu-build\materialx_focused_tests.log`.
- **Constraint:** these four tests verify graph lowering and USDShade reading
  in a CUDA-enabled Windows build, but do not themselves execute a CUDA render.
  Do not report GPU render parity until the dedicated CUDA-render smoke and
  comparison tests pass.
- **Success:** the complete Windows native MaterialX graph/reader batch passed
  190/190 (83 `materialx_graph` plus 107 `materialx_usdshade_reader`; 402 ms,
  exit 0). Durable evidence is stored in
  `C:\src\blender-materialx-gpu-build\materialx_graph_reader_full_tests.log`
  and `materialx_graph_reader_full_tests.exit`.
- **Constraint:** CTest registers six Blender-level MaterialX tests, including
  `cycles_materialx_composed_cuda_authority_blender`, but the isolated build
  does not yet contain `bin\blender.exe`. That test defaults to CPU unless
  `CYCLES_TEST_DEVICE=CUDA` is set.
- **Success:** a no-build Ninja query confirmed that `blender` is the direct
  executable dependency for the composed CUDA smoke. A dry run reported 7,481
  remaining build steps; its durable plan is
  `C:\src\blender-materialx-gpu-build\blender_target_dry_run.log`.
- **Failure:** `Start-Process` descendants launched from the Windows OpenSSH
  session were terminated when the SSH job closed, even with redirected
  output. A Task Scheduler launch also returned command success but remained
  `Ready` with `LastTaskResult=267011` and never ran. **Prevention:** require a
  persistent state file plus live process evidence; neither a submitted PID
  nor a scheduler command success is utilization evidence.
- **Success:** launching the fixed worker through `Win32_Process.Create`
  transferred ownership outside the OpenSSH job. The full Blender target build
  is active as PID 6992 with persistent `running` state and log evidence under
  `C:\src\blender-materialx-gpu-build`.
- **Failure:** one poll attempted to concatenate two remote PowerShell `-File`
  commands; quoting attached the separator to the first filename and no probe
  ran. **Prevention:** invoke one transferred status script per SSH command,
  or create a fixed aggregate script; never concatenate remote `-File`
  invocations inline.
- **Success:** a second CIM-owned worker (PID 8068) is waiting on the persistent
  Blender build exit file. On build exit 0 it automatically sets
  `CYCLES_TEST_DEVICE=CUDA` and runs only
  `cycles_materialx_composed_cuda_authority_blender`, persisting independent
  state, log, and exit files. A nonzero build or timeout fails closed without
  starting the smoke.
