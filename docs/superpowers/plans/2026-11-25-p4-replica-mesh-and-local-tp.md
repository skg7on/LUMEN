# P4 Replica Mesh and Local TP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Scale safe independent replicas first and add local TP only where qualified topology and benefit justify it.

**Architecture:** Use a bounded authenticated router over locally authoritative engines and an explicit submission ledger for uncertain requests. Keep TP a separately qualified track with offline sharding, fixed membership and aggregate compute/collective fences.

**Tech Stack:** C++20, versioned C backend ABI, CMake/Ninja/CTest, CPU/mock adapters and a qualified NPU adapter.

**Spec:** [P4-Replica-Mesh-and-Local-TP.md](../../architecture/P4-Replica-Mesh-and-Local-TP.md) and [shared runtime contracts](../../architecture/Shared-Runtime-Contracts.md).

**Status:** Proposed implementation plan | Date: 2026-11-25 | Release: R0.5

## Global Constraints

- “The two tracks have independent acceptance reports; a replica release does not imply TP support.”
- “Before output, retry automatically only after a confirmed nonacceptance or terminal cancellation, or under a separately qualified attempt-fencing protocol.”
- “After the first client-visible output, node failure terminates the stream with a reason; there is no automatic resume or replay.”
- “Do not hot-replace a rank in an active request.”
- “WAN/inter-site TP is outside the baseline.”
- Follow the [execution conventions and prerequisite contract](README.md); commands and source paths below are planned interfaces, not available commands in this design-only checkout.
- Keep all writes in an isolated worktree under `.claude/worktrees/`; use conventional commit subjects and preserve CPU/mock checks without a vendor SDK.

## Review Focus

- Stale adverts or cache hints must never bypass local admission — Task 1.
- Lost acceptance ACKs and duplicate/conflicting idempotency keys must not create blind independent retries — Task 2.
- Slow consumers, router loss and rollout must bound orphaned work without changing active model identity — Tasks 2–3.
- GQA head/rank mismatch must reject or explicitly replicate instead of silently mis-sharding — Task 4.
- Collective timeout and late rank results must fence the group without harming unrelated replicas — Tasks 5–6.

## Entry gate and task order

Require accepted P3 and authenticated bounded endpoints, model digests and usable load metrics. Replica work starts with multiple local processes. TP additionally requires accepted collective/topology/reset evidence, explicit sharding and small-model parity. Disable TP if its entry evidence is absent.

Tasks 1 → 2 → 3 establish the replica path. Tasks 4 → 5 → 6 are the optional local-TP branch. Task 7 evaluates each selected track independently.

## File structure

| Paths | Responsibility |
|---|---|
| `src/mesh/{node_registry,router,submission_ledger,node_lifecycle}.*` | Authenticated adverts, bounded placement, deduplication and rollout |
| `src/model/rank_pack.*`, `tools/pack/` | Explicit rank partition descriptors |
| `src/collective/collective.*`, `src/executor/rank_group.*` | Ordered transport and group fences |
| `tests/{integration,oracle,fault,perf}/`, `docs/qualification/p4/` | Replica and separate TP evidence |

---

### Task 1: Add authenticated bounded registry and routing

**Files:**

- Create: `src/mesh/node_registry.h`, `src/mesh/node_registry.cpp`, `src/mesh/router.h`, `src/mesh/router.cpp`, `tests/unit/router_test.cpp`, `configs/mesh/replicas.json`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P3 supported model/layout/precision registry and P1 service/metric contracts.
- Produces: `Status NodeRegistry::update(const SignedNodeAdvert&, uint64_t now_ns)`; `Result<RouteChoice> choose_replica(const RouteRequest&, const RegistrySnapshot&, const RoutingPolicy&)`; advert carries identity/epoch/TTL/readiness/model/capacity/thermal data.

- [ ] **Step 1: Write `routing_filters_stale_or_incompatible_nodes` assertions in `tests/unit/router_test.cpp` and register CTest `p4-routing`.** Use authenticated/forged, expired, overloaded and incompatible nodes plus stale optimistic prefix hints.

```cpp
CHECK(forged_advert == Status::invalid_input);
CHECK(expired_node_selected == false);
CHECK(incompatible_digest_selected == false);
CHECK(stale_hint.bypasses_local_admission == false);
CHECK(router_queue_size <= configured_router_capacity);
CHECK(repeated_snapshot.route == original_route);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-routing$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Start with static node config and bounded adverts/queues. Filter compatibility/readiness then use measured weighted completion estimates with stable ties/hysteresis. Final capacity admission remains local; hints never pin pages.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run one-host independent instances, stale metrics/skew/tenant pressure and explicit router/local overload rejection.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): route requests through bounded replica registry"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 2: Reconcile submissions and fence output delivery

**Files:**

- Create: `src/mesh/submission_ledger.h`, `src/mesh/submission_ledger.cpp`, `src/mesh/stream_bridge.h`, `src/mesh/stream_bridge.cpp`, `tests/fault/submission_retry_test.cpp`
- Modify: `src/mesh/router.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 RouteChoice and P1 native request/cancel/event-offset API.
- Produces: `Result<SubmissionBinding> bind_submission(const IdempotencyKey&, const RequestFingerprint&, NodeId, Ledger&)`; `RetryDecision decide_retry(const SubmissionBinding&, const SubmissionObservation&)`; `Status forward_event(const DeliveryBinding&, const OutputEvent&)`.

- [ ] **Step 1: Write `uncertain_acceptance_never_blindly_retries` assertions in `tests/fault/submission_retry_test.cpp` and register CTest `p4-submission`.** Lose acceptance ACK, repeat the same key/payload, conflict payload, fill/expire ledger and fail a node after first output. Use one router-owned delivery stream.

```cpp
CHECK(lost_ack.action == RetryAction::reconcile_original_node);
CHECK(confirmed_rejection.retry_allowed);
CHECK(conflicting_payload.status() == Status::invalid_input);
CHECK(full_ledger.status() == Status::resource_exhausted);
CHECK(after_first_output.retry_allowed == false);
CHECK(duplicate_forwarded_offsets == 0);
CHECK(slow_client.buffer_bytes <= bridge_limit);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-submission$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Bind canonical request/model identity to node/attempt, bound ledger retention and document restart semantics. Reconcile uncertain requests by key; terminate on unresolved timeout. Preserve output/cancel backpressure and terminal errors; do not imply durable multi-router deduplication or P5 resume.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Inject duplicate requests/ACKs, lost connections, delayed old streams and router/client disconnect; node deadlines bound orphan work.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "fix(mesh): reconcile uncertain submissions before retry"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 3: Implement node draining and immutable rollout

**Files:**

- Create: `src/mesh/node_lifecycle.h`, `src/mesh/node_lifecycle.cpp`, `tests/integration/mesh_rollout_test.cpp`, `docs/runbooks/replica-rollout.md`
- Modify: `src/mesh/node_registry.cpp`, `src/model/model_runner.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 1–2 bindings and P1 STARTING/READY/DRAINING/FAILED lifecycle.
- Produces: `Status begin_drain(NodeId, uint64_t deadline_ns, Registry&)`; `Status transition_node(NodeId, ServiceState, Epoch)`; rollout maps new requests to a new immutable model digest while old sessions retain their generation.

- [ ] **Step 1: Write `drain_and_rollback_preserve_active_generation` assertions in `tests/integration/mesh_rollout_test.cpp` and register CTest `p4-lifecycle`.** Drain a busy node, start a new model digest, interrupt rollout, lose router heartbeat and roll back new assignments.

```cpp
CHECK(draining_node.new_assignments == 0);
CHECK(active_request.model_digest == old_digest);
CHECK(new_request.model_digest == selected_new_digest);
CHECK(orphan_work_duration <= configured_node_timeout);
CHECK(rollback.changed_live_kv_semantics == false);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-lifecycle$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Make adverts soft state and node ownership local. Use bounded drain/cancel and epoch-aware readiness; exclude stale nodes for new work without claiming remote execution stopped. Add static multi-instance launch/runbook examples.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Test crash/drain/rollback with multiple local instances, then repeat on independent devices and record routing restore/detection separately.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): drain and roll replicas by model generation"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 4: Pack explicit TP shards and local KV mappings

**Files:**

- Create: `src/model/rank_pack.h`, `src/model/rank_pack.cpp`, `tests/oracle/rank_pack_test.cpp`
- Modify: `src/model/pack_writer.cpp`, `tools/pack/main.cpp`, `tools/inspect/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P1 pack_writer, P3 qualified traits/layouts and accepted local rank/topology profile.
- Produces: `Result<RankPack> shard_model(const ModelPack&, const PartitionSpec&)`; PartitionSpec explicitly maps attention/MLP/embedding/LM-head tensors, replicated tensors, head ownership and collective points.

- [ ] **Step 1: Write `tp_partition_is_explicit_and_reference_equivalent` assertions in `tests/oracle/rank_pack_test.cpp` and register CTest `p4-sharding`.** Use a small fixture with divisible heads and a GQA case requiring either rejected rank degree or an explicit validated replication map.

```cpp
CHECK(unsupported_head_partition.status() == Status::unsupported);
CHECK(reconstructed_weights == reference_weights);
CHECK(replicated_tensors_match_descriptor);
CHECK(rank_local_capacity_limit == most_loaded_rank_limit);
CHECK(tp_cpu_logits_error <= qualified_tolerance);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-sharding$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Never infer partitioning solely from shapes. Version rank manifests and rank-local page maps, record actual alignment/replication, and add `lumen-pack --partition <json>` plus inspect output.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run small-model single-rank/CPU-TP logits and greedy parity for every proposed degree before real collective integration.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(modelpack): emit explicit local tp rank shards"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 5: Add ordered collectives and atomic rank-group execution

**Files:**

- Create: `src/collective/collective.h`, `src/collective/collective.cpp`, `src/executor/rank_group.h`, `src/executor/rank_group.cpp`, `tests/fault/rank_group_test.cpp`
- Modify: `src/engine/step_plan.h`, `src/memory/kv_transaction.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 4 shards, P2 KvTxn and accepted transport/queue/event semantics.
- Produces: `Result<GroupSubmission> RankGroupExecutor::submit(const StepPlan&)`; `Status on_rank_result(const RankResult&)`; `Status Collective::enqueue(const CollectiveDescriptor&)`. Descriptor carries group epoch, step, ordered operation ID and bounded buffers.

- [ ] **Step 1: Write `group_commit_waits_for_every_rank_and_collective` assertions in `tests/fault/rank_group_test.cpp` and register CTest `p4-group`.** Delay one rank and one collective, mismatch an operation ID and fail the second rank's reservation.

```cpp
CHECK(one_rank_pending.group_commits == 0);
CHECK(collective_pending.group_commits == 0);
CHECK(mismatched_collective == Status::stale);
CHECK(partial_rank_reservation.visible_mappings == 0);
CHECK(all_fenced.group_commits == 1);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-group$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Reserve every rank atomically and dispatch fixed-membership immutable plans. Aggregate required compute/collective/postprocess fences before engine commit. Use only qualified device collectives or explicitly measured bounded host staging; no assumed CUDA/NCCL/overlap semantics.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run device-labeled logits parity and measured small-message/all-reduce bandwidth/jitter/host CPU costs on the selected topology.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(executor): commit fixed rank groups behind collective fences"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 6: Fence TP failures and bounded group recovery

**Files:**

- Create: `src/executor/group_recovery.h`, `src/executor/group_recovery.cpp`, `tests/fault/group_recovery_test.cpp`
- Modify: `src/executor/rank_group.cpp`, `src/mesh/node_lifecycle.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 5 group completion ledger, P1 reset/quarantine and Task 3 readiness.
- Produces: `Status fence_group(GroupId, Epoch failed_epoch, FailureReason)`; `Result<Epoch> restart_group(GroupId, const RecoveryProof&)`; proof requires qualified teardown/reset and self-test.

- [ ] **Step 1: Write `rank_loss_fences_only_the_affected_group` assertions in `tests/fault/group_recovery_test.cpp` and register CTest `p4-recovery`.** Inject rank loss during copy/launch/collective/commit, late old-epoch results and a reset that cannot prove DMA retirement.

```cpp
CHECK(failed_group.new_dispatches == 0);
CHECK(old_epoch_commits == 0);
CHECK(uncertain_reset.reuses_buffers == false);
CHECK(unrelated_replica.accepts_new_requests);
CHECK(active_request_rank_replacements == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-recovery$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Advance/fence epochs, fail affected requests, quarantine all uncertain rank buffers and rebuild membership only after drain/teardown/self-tests. Keep healthy independent replicas routable and expose failure stage/time.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run bounded recovery fault matrix on real ranks; report unsupported resets and keep readiness false rather than force reuse.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "fix(executor): fence failed tp groups before recovery"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 7: Qualify independent replica and TP tracks

**Files:**

- Create: `src/observability/p4_acceptance.h`, `src/observability/p4_acceptance.cpp`
- Create: `tests/perf/p4_acceptance_test.cpp`, `bench/workloads/p4-scale.json`, `docs/qualification/p4/replica-report.md`, `docs/qualification/p4/tp-report.md`
- Modify: `tools/bench/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 1–6 evidence as enabled and P3 matched one-node baseline.
- Produces: `Status validate_p4_report(const PhaseEvidence&, const P4AcceptanceProfile&)`; profile declares replica and optional TP support separately with topology/model/limits. Define P4AcceptanceProfile and the gate function in p4_acceptance.h/.cpp, consuming PhaseEvidence from P1 Task 10.

- [ ] **Step 1: Write `mesh_acceptance_cannot_infer_tp_or_hide_router_cost` assertions in `tests/perf/p4_acceptance_test.cpp` and register CTest `p4-acceptance`.** Feed replica-only evidence, an unqualified TP result and synthetic routing/skew/rejection timings.

```cpp
CHECK(replica_only.supports_tp == false);
CHECK(unqualified_tp_blocks_tp_enablement);
CHECK(aggregate_goodput_includes_router_overhead);
CHECK(rejected_demand_reported);
CHECK(failure_run.duplicate_output_offsets == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p4-acceptance$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Report independent acceptance matrices, model/capacity justification for TP, routing skew and failure isolation. Disabled TP reports a reason rather than a fictitious passing board test.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Measure equal one/two replicas with sufficient router/network capacity (candidate ≥1.7× SLO goodput), skew and failure reroute. Enable TP only for larger-model feasibility or measured same-model benefit that meets the declared SLO; do not extrapolate to WAN.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(mesh): qualify replica scaling and optional local tp"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

## Phase acceptance and handoff

Publish R0.5 replica report and, only when enabled, a separate local TP report. Require bounded dedup/reconciliation, no duplicated forwarded events, immutable rollouts, atomic group commit and bounded fault recovery. Candidate two-replica ≥1.7× goodput remains workload-specific; TP support is conditional and never inferred from replication.

## Spec coverage

| Spec sections | Tasks |
|---|---|
| §1–3 registry/routing | 1 |
| §4 idempotency/streaming | 2 |
| §5 lifecycle/rollout | 3 |
| §6 local TP | 4–5 |
| §7 TP faults/value | 6–7 |
| §8–9 acceptance/delivery | 7 and each track's tests |
