# P5 Advanced Distribution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement only justified advanced distribution features with durable ownership, bounded transfers and lossless output continuity.

**Architecture:** Gate each feature independently, then build compatible frozen checkpoints, durable ownership transitions and a credit-limited transfer plane. Remote handoff drains source delivery through watermark K before grant; PP pipelines distinct sequences inside one outer atomic plan.

**Tech Stack:** C++20, versioned C backend ABI, CMake/Ninja/CTest, CPU/mock adapters and a qualified NPU adapter.

**Spec:** [P5-Advanced-Distribution.md](../../architecture/P5-Advanced-Distribution.md) and [shared runtime contracts](../../architecture/Shared-Runtime-Contracts.md).

**Status:** Proposed implementation plan | Date: 2026-11-25 | Release: R0.7 optional

## Global Constraints

- “P5 is optional for R1.0.”
- “Reservation alone grants no execution right.”
- “Both target PREPARED and output-drain acknowledgement are required.”
- “If source or authority is unreachable, fail closed; a timeout is not proof that old execution stopped.”
- “The target starts newly committed output events at K+1.”
- “Overlapping independent outer plans, committing microbatches separately, or issuing multiple steps of the same sequence requires a new versioned execution/commit protocol and is deferred.”
- Follow the [execution conventions and prerequisite contract](README.md); commands and source paths below are planned interfaces, not available commands in this design-only checkout.
- Keep all writes in an isolated worktree under `.claude/worktrees/`; use conventional commit subjects and preserve CPU/mock checks without a vendor SDK.

## Review Focus

- Coordinator restart, lost ACKs and stale grants must not produce two valid owners — Task 3.
- Duplicate/corrupt chunks and partial DMA must keep target pages hidden — Task 4.
- Committed events 6–10 queued behind forwarded events 1–5 must drain before grant — Task 5.
- Slow PP stages and saturated decode destinations must remain credit-bounded — Tasks 6–7.
- Cancelled handoff, stale prefix hints and rejected speculative suffixes must not publish output or leak reservations — Tasks 5, 8 and 9.

## Entry gate and task order

Require the actual single-NPU demo and accepted P4 foundations. Task 1 freezes per-feature workload/compatibility/authority/security evidence. Production remote ownership stays blocked until durable authority, source fencing and delivery checkpoint semantics are selected and tested. MoE/EP, MLA and multimodal require separate requested capability designs.

Task 1 selects the supported scope. Remote handoff follows 2 → 3 → 4 → 5. PP Task 6 is independently gated; P-D Task 7 depends on accepted handoff. Hints Task 8 and speculation Task 9 are separate optional branches. Task 10 accepts each selected feature independently.

## File structure

| Paths | Responsibility |
|---|---|
| `src/mesh/{feature_gates,checkpoint,ownership_authority}.*` | Feature evidence, exact snapshots and durable grants |
| `src/memory/kv_transfer.*`, `src/mesh/handoff.*` | Bounded bytes, preparation fences and delivery barrier |
| `src/executor/pipeline_group.*`, `src/mesh/phase_router.*` | Outer-plan PP and coupled role admission |
| `src/mesh/prefix_directory.*`, `src/model/speculation.*` | Advisory hints and separately gated draft/verify |
| `tests/fault/`, `docs/qualification/p5/` | Partition, integrity and net-benefit evidence |

---

### Task 1: Enforce per-feature entry and benefit gates

**Files:**

- Create: `src/mesh/feature_gates.h`, `src/mesh/feature_gates.cpp`, `tests/unit/distribution_gates_test.cpp`, `docs/qualification/p5/input-profile.json`
- Modify: `tools/inspect/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Accepted P3/P4 evidence, model/layout identities and measured link/service profiles.
- Produces: `Result<EnabledFeatures> qualify_distribution(const DistributionInputs&, const FeaturePolicy&)`; inputs pin durable authority provider/failure model, fence method, security profile, topology and accepted benefit/compatibility evidence.

- [ ] **Step 1: Write `advanced_distribution_requires_feature_specific_evidence` assertions in `tests/unit/distribution_gates_test.cpp` and register CTest `p5-gates`.** Request remote KV without authority, P-D without destination capacity and PP without justified feasibility/latency.

```cpp
CHECK(no_authority.remote_kv_enabled == false);
CHECK(no_decode_capacity.phase_split_enabled == false);
CHECK(replica_only_report.implies_pp_support == false);
CHECK(unsupported_layout_conversion == Status::unsupported);
CHECK(every_disabled_feature_has_reason);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-gates$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Default every P5 feature off and bind evidence to exact model/precision/layout/shards/topology. Freeze provider/protocol choices in ADR 0006 before implementation; reject timeout-only authority. Record selected scope rather than enabling every architecture possibility.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Inspect a P3/P4-only configuration and prove R1.0 does not require P5. Measure full transfer/coordination cost against recompute/local wait before selecting remote modes.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): gate advanced distribution on qualified evidence"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 2: Serialize exact frozen checkpoints and transfer manifests

**Files:**

- Create: `src/mesh/checkpoint.h`, `src/mesh/checkpoint.cpp`, `tests/unit/checkpoint_test.cpp`
- Modify: `src/engine/engine.h`, `src/engine/engine.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 compatibility profile and P1 engine/decoder/RNG state including distinct output-event offsets.
- Produces: `Result<FrozenCheckpoint> Engine::freeze_session(RequestHandle)`; `Status validate_checkpoint(const FrozenCheckpoint&, const CompatibilityManifest&)`. Snapshot includes pending sampled token, KV/generated cursors, needed history, decoder/stop suffix, RNG, model/shard identities and output watermark K.

- [ ] **Step 1: Write `checkpoint_preserves_pending_input_and_decoder_state` assertions in `tests/unit/checkpoint_test.cpp` and register CTest `p5-checkpoint`.** Freeze only after a delayed postprocess finishes; include sampled token without KV, withheld UTF-8 and mismatched layout/model identities.

```cpp
CHECK(unfinished_postprocess.freeze_status == Status::stale);
CHECK(snapshot.pending_token == last_sampled_token);
CHECK(snapshot.kv_tokens + 1 == snapshot.prompt_tokens + snapshot.generated_tokens);
CHECK(snapshot.decoder_suffix == withheld_suffix);
CHECK(snapshot.output_watermark != snapshot.generated_tokens); // distinct units
CHECK(incompatible_manifest == Status::unsupported);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-checkpoint$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use versioned checked lengths/offsets, exact digests and canonical checkpoint identity. Stop future source scheduling and fence in-flight work before snapshot; do not infer missing state from KV alone.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Round-trip every state field and reject corrupt/oversized/shard-incompatible manifests. Sensitive data stays out of default logs.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): freeze compatible token and output checkpoints"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 3: Implement durable single-owner grant transitions

**Files:**

- Create: `src/mesh/ownership_authority.h`, `src/mesh/ownership_authority.cpp`, `src/mesh/authority_store.h`, `src/mesh/authority_store.cpp`, `tests/fault/ownership_authority_test.cpp`
- Modify: `tests/CMakeLists.txt`, `docs/adr/0006-distributed-execution-authority.md` when acceptance evidence exists

**Interfaces:**

- Consumes: Task 1 accepted durable provider and Task 2 checkpoint/transfer identity.
- Produces: `Result<OwnerGrant> prepare_grant(const GrantRequest&)`; `Result<OwnerGrant> commit_grant(const PreparedProof&, const SourceFenceProof&, const DeliveryDrainProof&)`; `Result<AuthorityRecord> reconcile(SessionId, TransferId)`. Store performs durable compare-and-swap on owner/epoch/version/checkpoint.

- [ ] **Step 1: Write `authority_survives_restart_and_denies_split_brain` assertions in `tests/fault/ownership_authority_test.cpp` and register CTest `p5-authority`.** Crash/restart the store after each transition, lose grant ACK, partition source/authority and replay an old prepared record.

```cpp
CHECK(concurrent_valid_owner_grants <= 1);
CHECK(lost_ack.reconciled_epoch == durable_granted_epoch);
CHECK(unreachable_authority.grant_status != Status::ok);
CHECK(no_source_fence.grant_status != Status::ok);
CHECK(no_output_drain.grant_status != Status::ok);
CHECK(stale_epoch_execution_rejected);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-authority$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use the selected durable atomic provider behind the store interface; an in-memory simulator is not a production implementation. Validate source revocation and target checkpoint before CAS, issue monotonically fenced grants, and reconcile ambiguous outcomes instead of rolling back a durable grant.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run host fault models then real-provider crash/restart/partition tests. Source restart/resume must query authority; timeout never authorizes takeover.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): persist and fence remote session ownership"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 4: Build credit-limited integrity-checked KV transfer

**Files:**

- Create: `src/memory/kv_transfer.h`, `src/memory/kv_transfer.cpp`, `tests/fault/kv_transfer_test.cpp`
- Modify: `src/memory/kv_transaction.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 2 manifest/checkpoint, P2 KvTxn/leases and accepted encrypted transport/copy semantics.
- Produces: `Result<TransferTxn> reserve_transfer(const TransferManifest&, const TransferBudget&)`; `Status receive_chunk(TransferTxn&, const TransferChunk&)`; `Result<PreparedProof> prepare_target(TransferTxn&, const FenceProof&)`; `Status abort_transfer(TransferTxn&, const FenceProof&)`.

- [ ] **Step 1: Write `transfer_is_idempotent_bounded_and_invisible_until_prepared` assertions in `tests/fault/kv_transfer_test.cpp` and register CTest `p5-transfer`.** Duplicate/reorder/corrupt chunks, use stale slot generations, stall copy and exceed byte credits.

```cpp
CHECK(duplicate_same_digest.applied_once);
CHECK(conflicting_chunk.status() == Status::corrupt);
CHECK(inflight_bytes <= configured_transfer_cap);
CHECK(unfenced_target.visible_to_engine == false);
CHECK(partial_dma_timeout.target_quarantined);
CHECK(source_leases_retired_only_after_last_read);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-transfer$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Reserve target capacity and bounded metadata/bytes before receive, lease source snapshots, validate every range/checksum and fence destination DMA. PREPARED grants no execution/cache visibility. Keep compute/transfer pools isolated enough to preserve admitted decode headroom.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run source/target failure and cancel at each chunk/fence boundary, plus real measured copies/encryption/network congestion. Record retention/cleanup and no leaked reservations.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(memory): transfer kv behind credits and integrity fences"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 5: Orchestrate drain-before-grant handoff

**Files:**

- Create: `src/mesh/handoff.h`, `src/mesh/handoff.cpp`, `tests/fault/handoff_output_test.cpp`
- Modify: `src/mesh/stream_bridge.h`, `src/mesh/stream_bridge.cpp`, `src/engine/engine.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 2–4 and P4 delivery binding/event offsets.
- Produces: `Result<HandoffHandle> begin_handoff(RequestHandle, NodeId)`; `Result<DeliveryDrainProof> drain_through(SessionId, uint64_t K)`; `Status resume_target(const OwnerGrant&, const FrozenCheckpoint&)`; authority reconciliation resolves cancel/grant races.

- [ ] **Step 1: Write `handoff_drains_queued_events_before_owner_change` assertions in `tests/fault/handoff_output_test.cpp` and register CTest `p5-handoff`.** Source commits events 1–10 while router forwarded 1–5; delay remaining forwarding, lose grant ACK and race cancellation.

```cpp
CHECK(before_events_6_to_10_forwarded.owner_changed == false);
CHECK(after_drain.router_watermark == 10);
CHECK(target.first_new_event_offset == 11);
CHECK(all_forwarded_offsets == expected_offsets_without_loss_or_duplicates);
CHECK(slow_client_deadline.no_grant);
CHECK(cancel_race.current_owner_cancelled);
CHECK(ambiguous_delivery.resume_rejected);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-handoff$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Require target PREPARED plus source fence and router drain proof K before authority CAS. Transfer only unresolved decoder suffix/state, not ordinary queued events. A pre-grant abort permits source resume only after authoritative no-grant confirmation; post-grant ACK loss reconciles the target owner.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run partition/lost-ACK/cancel/slow-client matrix and compatible-target token continuation. Keep one surviving router stream; client/cross-router reconnect resume remains unsupported without a separate persisted ACK protocol.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "fix(mesh): drain committed output before remote ownership grant"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 6: Pipeline distinct-sequence microbatches in one outer plan

**Files:**

- Create: `src/executor/pipeline_group.h`, `src/executor/pipeline_group.cpp`, `tests/fault/pipeline_group_test.cpp`
- Modify: `src/model/rank_pack.cpp`, `src/executor/rank_group.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 PP gate, P4 partition/group fence interfaces and P1 decoder completion rule.
- Produces: `Result<PipelineSubmission> submit_pipeline(const StepPlan&, const StageGraph&, ActivationBudget&)`; StageGraph fixes layer/rank/epoch dependencies and finite distinct-sequence microbatches within one outer plan.

- [ ] **Step 1: Write `pipeline_plan_commits_all_microbatches_after_all_stages` assertions in `tests/fault/pipeline_group_test.cpp` and register CTest `p5-pipeline`.** Delay a stage and a decoder proposal, fill activation credits, fail a microbatch and try a second outer plan on an occupied worker.

```cpp
CHECK(pending_stage.outer_commits == 0);
CHECK(pending_decoder.outer_commits == 0);
CHECK(activation_bytes <= configured_activation_cap);
CHECK(second_outer_plan.status() == Status::resource_exhausted);
CHECK(failed_microbatch.partial_item_commits == 0);
CHECK(one_sequence.uncommitted_steps <= 1);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-pipeline$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Pack explicit layer-stage descriptors, fence activation production/consumption and aggregate every stage/transport/sampling/postprocess result. Bound per-boundary credits; group failure does not affect independent replicas. Repartition only after drain.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run single-stage/reference parity, stage-loss recovery and real bubble/latency/activation-byte measurements. Enable only when declared capacity or net workload value justifies full-plan emission delay.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(executor): pipeline bounded microbatches within atomic plans"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 7: Couple prefill/decode placement and admission

**Files:**

- Create: `src/mesh/phase_router.h`, `src/mesh/phase_router.cpp`, `tests/fault/phase_admission_test.cpp`, `configs/mesh/phase-split.json`
- Modify: `src/mesh/router.cpp`, `src/mesh/handoff.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Accepted Tasks 1–5, target role/load/capacity adverts and transfer credits.
- Produces: `Result<PhaseReservation> reserve_phase_path(const PhaseRequest&, const PhaseCapacitySnapshot&)`; `Status complete_prefill(PhaseReservation&, const FrozenCheckpoint&)`; reservation couples decode commitment, KV/transfer credits and deadline.

- [ ] **Step 1: Write `phase_split_never_builds_an_unbounded_waiting_kv_queue` assertions in `tests/fault/phase_admission_test.cpp` and register CTest `p5-phase-routing`.** Saturate decode capacity, fail target handoff, cancel prefill and offer short requests where local serving is cheaper.

```cpp
CHECK(no_decode_destination.prefill_dispatches == 0);
CHECK(waiting_kv_bytes <= configured_commitment_cap);
CHECK(failed_handoff.reservations_released_after_fences);
CHECK(colocated_fallback.qualified);
CHECK(first_sampled_token.delivered_once);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-phase-routing$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Reserve a compatible target or bounded handoff commitment before prefill. Transfer final prefill KV/pending first token with delivery state; choose colocated fallback only when qualified and within request semantics.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Measure full prefill+transfer+decode queue+output SLO under asymmetric/burst traffic and degraded links; compare against colocated and replicas.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): couple phase split admission to decode capacity"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 8: Add advisory tenant-scoped prefix hints

**Files:**

- Create: `src/mesh/prefix_directory.h`, `src/mesh/prefix_directory.cpp`, `tests/unit/prefix_directory_test.cpp`
- Modify: `src/mesh/router.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 hint benefit/privacy gate and P3 exact local prefix lookup.
- Produces: `Status PrefixDirectory::publish(const PrefixHint&, uint64_t now_ns)`; `Result<PrefixHint> lookup(const HintQuery&, uint64_t now_ns)`; hints contain tenant/model/layout digest, approximate length, node and TTL only.

- [ ] **Step 1: Write `prefix_hints_expire_and_never_confer_ownership` assertions in `tests/unit/prefix_directory_test.cpp` and register CTest `p5-prefix-hints`.** Publish stale, other-tenant, wrong-layout and over-capacity entries, then route to a node whose actual prefix was evicted.

```cpp
CHECK(expired_hint.status() == Status::stale);
CHECK(other_tenant_hint.visible == false);
CHECK(directory.entries <= configured_entry_cap);
CHECK(stale_hint.creates_page_reference == false);
CHECK(target_exact_lookup_miss.handled_normally);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-prefix-hints$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use bounded metadata/TTL and authenticated publications; never expose mutable remote pointers or bypass target exact lookup/admission. Keep hints disabled without measured reuse benefit.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run churn/privacy/TTL tests and measure routing benefit under real repeated-prefix traffic.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(mesh): publish bounded advisory prefix locality hints"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 9: Integrate only an accepted speculative algorithm

**Files:**

- Create: `src/model/speculation.h`, `src/model/speculation.cpp`, `tests/fault/speculation_test.cpp`
- Modify: `src/engine/step_plan.h`, `src/memory/kv_transaction.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 selected draft/target profile, P2 private transactions and an additional accepted algorithm ADR required by the spec.
- Produces: `Result<SpeculativeTxn> begin_speculation(const DraftRequest&, const SpeculationLimits&)`; `Result<VerifiedPrefix> verify_greedy_prefix(const DraftTokens&, const TargetTokens&)`; `Status commit_verified(SpeculativeTxn&, const VerifiedPrefix&, const CommitProof&)`.

- [ ] **Step 1: Write `speculative_rejection_never_publishes_draft_suffix` assertions in `tests/fault/speculation_test.cpp` and register CTest `p5-speculation`.** Use a greedy draft with a mismatch, EOS inside the accepted prefix, cancellation and delayed suffix-write fences.

```cpp
CHECK(accepted_prefix.length == longest_target_matching_prefix);
CHECK(unverified_output_events == 0);
CHECK(rejected_suffix.reused_before_fence == false);
CHECK(target_rng_only_advances_for_committed_target_results);
CHECK(no_algorithm_adr.enable_status == Status::unsupported);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-speculation$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Default off. If selected, freeze greedy prefix/rollback cursor equations and draft/target RNG separation in the additional ADR before code. Bound draft length and private KV; retain final sampled-token accounting. Stochastic speculation remains disabled until its exact acceptance/rejection distribution algorithm and a dedicated extension plan are accepted.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run rollback/cancel/stop and independent target-semantic tests, then measure verification-inclusive net benefit. Do not claim stochastic distributional correctness from token matching.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(model): gate rollback safe speculative verification"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 10: Accept each advanced feature on fault and benefit evidence

**Files:**

- Create: `src/observability/p5_acceptance.h`, `src/observability/p5_acceptance.cpp`
- Create: `tests/perf/p5_acceptance_test.cpp`, `bench/workloads/p5-links.json`, `docs/qualification/p5/release-report.md`
- Modify: `tools/bench/main.cpp`, `tools/inspect/main.cpp`, `tools/replay/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Selected Tasks 1–9 and matched local recompute/replica/P4 baselines.
- Produces: `Status validate_p5_report(const PhaseEvidence&, const P5AcceptanceProfile&)`; report partitions PP, remote KV, P-D, hints and speculation with compatibility/fault/net-benefit evidence. Define P5AcceptanceProfile and the gate function in p5_acceptance.h/.cpp, consuming PhaseEvidence from P1 Task 10.

- [ ] **Step 1: Write `advanced_feature_acceptance_requires_complete_end_to_end_cost` assertions in `tests/perf/p5_acceptance_test.cpp` and register CTest `p5-acceptance`.** Reject a volatile-only authority report, a raw-line-rate-only transfer estimate and a handoff run with missing output events.

```cpp
CHECK(volatile_authority.production_enabled == false);
CHECK(line_rate_only.transfer_evidence_complete == false);
CHECK(missing_output_event.accepted == false);
CHECK(unselected_feature.enabled == false);
CHECK(feature_benefit_includes_copy_verify_admission_and_coordination);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p5-acceptance$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Extend inspect/replay with transfer/grant/delivery timelines. Publish encrypted transport/retention/failure profiles and feature-specific rollback switches; never average away an unqualified mode.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run source/target/coordinator failure, partition, stale grant, duplicate/corrupt chunk, lost ACK, cancel and output-drain cases on real compatible systems. Demonstrate capacity, latency, energy or goodput benefit per enabled feature against measured alternatives.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(mesh): qualify advanced distribution by feature"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

## Phase acceptance and handoff

Enable only selected features with independent numerical, ownership, security, bounded-resource and workload-benefit evidence. Remote handoff must lose/duplicate no forwarded event, fail closed under uncertain authority, and retain leases until safe retirement. PP/P-D/remote KV each need positive declared value; absence of any P5 feature cannot block an otherwise qualified P3/P4 R1.0.

## Spec coverage

| Spec sections | Tasks |
|---|---|
| §1 feature gates | 1 |
| §2 checkpoint/manifest | 2 |
| §3 authority/state machine | 3–5 |
| §4 output/cancellation | 5 |
| §5 transfer/cost | 4, 10 |
| §6 PP | 6 |
| §7 P-D/hints | 7–8 |
| §8 speculation | 9 |
| §9–10 verification/delivery | 10 and feature fault suites |
