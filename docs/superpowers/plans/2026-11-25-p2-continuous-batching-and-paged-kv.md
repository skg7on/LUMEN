# P2 Continuous Batching and Paged KV Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace fixed KV/static batching with safe transactional pages, continuous scheduling and replayable mixed-load evidence.

**Architecture:** Keep P1 engine authority and postprocess commits, replacing only the logical storage/policy mechanisms. Build CPU dense/paged equivalence before a qualified native/gather NPU path; calibrate policy with measured supported buckets.

**Tech Stack:** C++20, versioned C backend ABI, CMake/Ninja/CTest, CPU/mock adapters and a qualified NPU adapter.

**Spec:** [P2-Continuous-Batching-and-Paged-KV.md](../../architecture/P2-Continuous-Batching-and-Paged-KV.md) and [shared runtime contracts](../../architecture/Shared-Runtime-Contracts.md).

**Status:** Proposed implementation plan | Date: 2026-11-25 | Release: R0.2

## Global Constraints

- “Use P=16 for initial experiments, with page size encoded in the descriptor.”
- “Strict admission remains the default.”
- “A submitted NPU step is nonpreemptible unless target qualification proves otherwise.”
- “Partial launch is an execution failure requiring fencing, not a reservation rollback shortcut.”
- Retain one uncommitted step per sequence, bounded output/postprocess credits and atomic group commit.
- Follow the [execution conventions and prerequisite contract](README.md); commands and source paths below are planned interfaces, not available commands in this design-only checkout.
- Keep all writes in an isolated worktree under `.claude/worktrees/`; use conventional commit subjects and preserve CPU/mock checks without a vendor SDK.

## Review Focus

- Generation reuse after a delayed completion must never attach a stale page — Tasks 1–2.
- A final partial page and all-masked attention row must not read uninitialized padding — Task 3.
- An immediate completion or partial metadata launch must not bypass publication/fencing — Task 4.
- Continuous decode pressure must not starve an otherwise feasible aged prefill — Task 5.
- Unknown cost buckets, full trace buffers and corrupted replay must degrade explicitly — Tasks 6–7.

## Entry gate and task order

Require accepted P1 evidence and CPU/mock fixtures. Select native paged attention or an explicitly bounded gather fallback from target qualification; unsupported contexts are rejected. No APC, quantized KV or distributed ownership is added here.

Tasks 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8. Keep the P1 contiguous path runnable as a comparison/fallback within its declared envelope.

## File structure

| Paths | Responsibility |
|---|---|
| `src/memory/{page_pool,kv_transaction}.*` | Logical handles, lifecycle and atomic reservation |
| `src/kernels/reference/paged_attention.*`, `src/backend/target_npu/paged_runner.*` | Semantic oracle and device/gather execution |
| `src/scheduler/{continuous_policy,cost_table}.*` | Pure selection and measured costs |
| `src/observability/step_trace.*`, `sim/replay.*`, `tools/replay/main.cpp` | Bounded traces and independent replay checks |
| `tests/{property,oracle,fault,perf}/`, `docs/qualification/p2/` | Safety and R0.2 acceptance |

---

### Task 1: Create descriptor-based logical page pools

**Files:**

- Create: `src/memory/page_pool.h`, `src/memory/page_pool.cpp`, `tests/property/page_pool_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P1 checked layout sizes, backend buffer handles and canonical Status/Result.
- Produces: `Result<BlockHandle> PagePool::reserve()`; `Status release(BlockHandle, FenceState)`; `Result<PageSnapshot> inspect(BlockHandle) const`; descriptors contain page_tokens, aligned bytes, layout/pool IDs; handle carries slot/generation.

- [ ] **Step 1: Write `page_generation_and_lease_lifecycle` assertions in `tests/property/page_pool_test.cpp` and register CTest `p2-page-pool`.** Use a one-slot P=16 pool and delayed leases; drive 100,000 seeded transitions against an independent slot ledger.

```cpp
CHECK(pool.release(live_handle, pending_fence) == Status::stale);
CHECK(pool.inspect(reused_old_generation).status() == Status::stale);
CHECK(new_handle.generation > old_handle.generation);
CHECK(pool.free_slots() + pool.nonfree_slots() == 1);
CHECK(invariant_violations_after_100000_transitions == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-page-pool$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Preallocate slots/intrusive lists and track FREE/RESERVED/WRITING/SEALED/PRIVATE_TAIL/RETIRED states, valid length, references and leases. Detect generation exhaustion; never wrap to a valid old handle.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run seed replay, slot/size overflow and zero-capacity cases. Report actual aligned storage rather than ideal token bytes.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(memory): add generation checked logical kv pages"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 2: Reserve and publish KV mappings transactionally

**Files:**

- Create: `src/scheduler/step_selection.h`
- Create: `src/memory/kv_transaction.h`, `src/memory/kv_transaction.cpp`, `tests/fault/kv_transaction_test.cpp`
- Modify: `src/scheduler/admission.h`, `src/scheduler/admission.cpp`, `src/engine/engine.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 PagePool; P1 AdmissionTicket, immutable plan slab and owner commit.
- Produces: `Result<KvTxn> reserve_step(const StepSelection&, KvState&, Capacity&)`; `Status rollback_unsubmitted(KvTxn&)`; `Status commit_step(KvTxn&, const CommitProof&)`. Proof covers identities, device fences and decoder proposals; transaction includes mappings and all resource credits. Define `StepSelection`/SelectionItem in step_selection.h before Task 5 uses them; items contain GenerationKey, phase, input range/positions and resource/emit demand.

- [ ] **Step 1: Write `reservation_failure_has_no_partial_visibility` assertions in `tests/fault/kv_transaction_test.cpp` and register CTest `p2-kv-transaction`.** Fail the second requested page, output credit acquisition and a partial launch; preserve previously committed private-tail positions.

```cpp
CHECK(failed_reserve.public_mapping == original_mapping);
CHECK(failed_reserve.remaining_credits == original_credits);
CHECK(unfenced_commit == Status::stale);
CHECK(partial_launch.rollback_unsubmitted == Status::stale);
CHECK(partial_launch.touched_pages_quarantined);
CHECK(committed_tail_prefix_unchanged);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-kv-transaction$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Compute all checked page/tail/future demands before publication. Reserve all resources or none, retain private write ranges and logical snapshots, and publish valid positions only in the owner commit after postprocessing. Keep uncertain pages quarantined until reset/fence.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Inject cancel, failed reserve, stale plan and postprocess failure at each transaction boundary; run prior strict-admission tests.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(memory): transact kv mappings and step credits"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 3: Implement independent CPU paged attention

**Files:**

- Create: `src/kernels/reference/paged_attention.h`, `src/kernels/reference/paged_attention.cpp`, `tests/oracle/paged_attention_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 descriptors and P0 independent dense attention vectors.
- Produces: `Status paged_attention(const AttentionInput&, std::span<const BlockHandle>, const KvReader&, std::span<float> output, AttentionScratch&)`; AttentionInput declares query positions, context/valid lengths, head mapping and masks.

- [ ] **Step 1: Write `paged_attention_matches_dense_without_padding_reads` assertions in `tests/oracle/paged_attention_test.cpp` and register CTest `p2-cpu-attention`.** Randomize physical block order for contexts 1,15,16,17,31,32,33; poison padding and include GQA, causal chunk masks and all-masked rows.

```cpp
CHECK(max_abs_error(paged_output, dense_expected) <= fp32_tolerance);
CHECK(invalid_or_future_kv_reads == 0);
CHECK(all_masked_output == defined_zero_output);
CHECK(noncontiguous_output_matches_contiguous_reference);
CHECK(scratch_high_water <= declared_scratch_bytes);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-cpu-attention$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Read valid tokens only with checked block generation/strides. Keep the dense expected path independent of page indexing; document zero output for fully masked queries and explicit supported full-attention traits.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Sweep partial pages, chunk positions, GQA mappings and arbitrary valid lengths with predeclared tolerances.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(kernels): add cpu paged attention oracle"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 4: Integrate native or bounded gather execution

**Files:**

- Create: `src/backend/target_npu/paged_runner.h`, `src/backend/target_npu/paged_runner.cpp`, `tests/fault/paged_executor_test.cpp`, `tests/oracle/npu_paged_attention_test.cpp`
- Modify: `src/model/model_runner.cpp`, `src/engine/step_plan.h`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 2–3, P1 ModelRunner::launch and P0 ABI event semantics.
- Produces: `Result<Submission> launch_paged(const StepPlan&, PagedWorkspace&)`; `Status validate_paged_plan(const StepPlan&, const PagedCaps&)`. PagedCaps states native/gather mode, finite contexts/buckets, scratch and metadata capacities.

- [ ] **Step 1: Write `paged_execution_fences_metadata_and_partial_launch` assertions in `tests/fault/paged_executor_test.cpp` and register CTest `p2-executor`.** Delay metadata and gather copies, complete immediately on another run, and fail launch after one device command.

```cpp
CHECK(launch_before_gather_fence == Status::stale);
CHECK(outside_gather_context == Status::unsupported);
CHECK(metadata_reused_before_fence == 0);
CHECK(immediate_completion_commits_before_inflight_mark == 0);
CHECK(partial_launch.reclaimed_before_reset == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-executor$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Resolve logical slots inside workers with checked arithmetic. Lease metadata/gather buffers through completion and postprocess commit. Separate phase programs if mixed graphs are unsupported; explicitly report gather bytes/cost and reject over-limit contexts.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run device-labeled CPU/NPU attention comparisons on qualified shapes. Measure native versus gather copy/launch costs and verify no uninitialized reads.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(executor): fence paged device and gather paths"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 5: Add continuous decode-first scheduling with prefill service

**Files:**

- Create: `src/scheduler/continuous_policy.h`, `src/scheduler/continuous_policy.cpp`, `tests/unit/continuous_policy_test.cpp`
- Modify: `src/engine/engine.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 2 transactional capacity and P1 engine sequence/output snapshots.
- Produces: `StepSelection choose_step(const ScheduleSnapshot&, const PolicyConfig&)`; snapshot includes phases/cursors/deadlines/tenant deficits/age, credits, variants and resource/time budgets. Selection is immutable and allocates nothing.

- [ ] **Step 1: Write `decode_pressure_preserves_bounded_prefill_progress` assertions in `tests/unit/continuous_policy_test.cpp` and register CTest `p2-policy`.** Simulate constant decodes, an old feasible prefill, an infeasible head-of-line request, exhausted output credits and equal-deadline ties.

```cpp
CHECK(aged_prefill.service_delay <= config.max_prefill_service_delay);
CHECK(infeasible_head.blocks_all_feasible_work == false);
CHECK(selected_decode.input_count == 1);
CHECK(no_output_credit.dispatches == 0);
CHECK(equal_snapshot_selection == repeated_selection);
CHECK(intermediate_prompt_chunk.emit_token == false);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-policy$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Order decode by slack/fairness/arrival/stable ID, reserve due prefill service, and choose largest feasible supported chunks. Enforce strict memory/token/sequence/scratch/output/time budgets; shrink/defer after reservation failure. Do not assume chunk tokens equal page tokens.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run mixed short/long traffic and overload with separate/mixed graph capabilities; verify bounded service only inside the declared feasible envelope.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(scheduler): add continuous batching and fair prefill"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 6: Calibrate conservative bucket costs

**Files:**

- Create: `src/scheduler/cost_table.h`, `src/scheduler/cost_table.cpp`, `tests/unit/cost_table_test.cpp`, `bench/workloads/p2-buckets.json`
- Modify: `src/scheduler/continuous_policy.h`, `src/scheduler/continuous_policy.cpp`, `tools/bench/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 4 measured variants and Task 5 policy snapshots.
- Produces: `Result<CostEstimate> estimate_step(const CostKey&, const CostTable&)`; CostKey includes phase/batch/query/context histogram/precision/variant/thermal state; estimate declares measured range and conservative quantile.

- [ ] **Step 1: Write `unknown_cost_never_extrapolates_as_a_guarantee` assertions in `tests/unit/cost_table_test.cpp` and register CTest `p2-cost`.** Provide known buckets, gaps inside measured ranges, an unknown thermal state and contexts beyond the table.

```cpp
CHECK(known_estimate.conservative_ns >= known_median_ns);
CHECK(outside_range.status() == Status::unsupported);
CHECK(unknown_thermal.uses_safe_qualified_bucket);
CHECK(high_prediction_error.disables_aggressive_bucket);
CHECK(table_identity_recorded_in_selection);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-cost$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Version timing tables, constrain interpolation, track prediction error and expose safe fallback/defer reasons. Capture padding, gather and thermal effects rather than treating tokens as uniform cost.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Collect board distributions and held-out prediction errors; no simulator estimate satisfies the device timing evidence requirement.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(scheduler): use measured conservative step costs"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 7: Add bounded traces and independent replay checks

**Files:**

- Create: `src/observability/step_trace.h`, `src/observability/step_trace.cpp`, `sim/replay.h`, `sim/replay.cpp`, `tools/replay/main.cpp`, `tests/integration/replay_test.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 2, 5 and 6 event order, reservation outcomes, cost identities and selection snapshots.
- Produces: `Status TraceSink::try_record(const StepTrace&)`; `Result<ReplayReport> replay(const TraceInput&, const ReplayConfig&)`; CLI `lumen-replay --trace <path> --check-invariants --report <json>`.

- [ ] **Step 1: Write `replay_is_deterministic_and_trace_pressure_is_bounded` assertions in `tests/integration/replay_test.cpp` and register CTest `p2-replay`.** Replay identical ordered events twice, overflow a tiny trace ring, corrupt a record and inject a stale handle into an otherwise valid trace.

```cpp
CHECK(first.schedule_digest == second.schedule_digest);
CHECK(full_sink.dropped_records == expected_drops);
CHECK(full_sink.blocked_completion_threads == 0);
CHECK(corrupt_trace.status() == Status::corrupt);
CHECK(stale_trace.invariant_violations == 1);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-replay$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Version records with bounded lengths and exclude prompt contents by default. Reuse pure scheduling policy but implement an independent slot/cursor ledger, so replay can detect allocator mistakes instead of reproducing them.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Round-trip a real P2 trace and preserve configuration/cost/event identities; report numerical replay separately from schedule equality.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(observability): add bounded step traces and replay"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 8: Qualify safety and mixed-load goodput

**Files:**

- Create: `src/observability/p2_acceptance.h`, `src/observability/p2_acceptance.cpp`
- Create: `tests/perf/p2_acceptance_test.cpp`, `bench/workloads/p2-mixed.json`, `docs/qualification/p2/release-report.md`
- Modify: `tools/bench/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P1 run_workload/report contract, Tasks 1–7 evidence and frozen matched comparison profiles.
- Produces: `Status validate_p2_report(const PhaseEvidence&, const P2AcceptanceProfile&)`; reports 100,000 transitions, supported attention tolerance, strict-envelope safety, replay and matched goodput/tail deltas. Define P2AcceptanceProfile and the gate function in p2_acceptance.h/.cpp, consuming PhaseEvidence from P1 Task 10.

- [ ] **Step 1: Write `p2_gate_requires_safety_and_matched_comparisons` assertions in `tests/perf/p2_acceptance_test.cpp` and register CTest `p2-acceptance`.** Supply synthetic reports with a stale-handle violation, fewer than 100,000 transitions, changed precision and hidden rejected demand.

```cpp
CHECK(validate_p2_report(stale_handle_run, profile) == Status::corrupt);
CHECK(validate_p2_report(short_property_run, profile) == Status::invalid_input);
CHECK(validate_p2_report(unmatched_precision_run, profile) == Status::invalid_input);
CHECK(report.offered == report.admitted + report.rejected);
CHECK(report.failed_admitted_requests_are_slo_misses);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p2-acceptance$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Add gate evaluation to the benchmark report and collect all artifact digests/raw-trace links. Distinguish native paging and gather, strict and future elastic modes, and real versus simulated timings.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run ≥100,000 randomized/fault transitions, CPU/NPU supported-shape parity, overload with no strict-envelope OOM and deterministic replay. Compare R0.2/R0.1 at identical model/precision/workload; investigate gather/launch/padding costs if the provisional ≥1.3× goodput target is not met.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(memory): qualify paged batching safety and goodput"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

## Phase acceptance and handoff

Require every P2 safety invariant, qualified attention tolerances, explicit overload behavior and deterministic replay. Demonstrate improved representative SLO goodput without unacceptable p99 ITL regression on matched R0.1 inputs; ≥1.3× is a provisional goal, not an achieved result. Preserve offered/admitted/completed/rejected populations and record the accepted envelope before P3.

## Spec coverage

| Spec sections | Tasks |
|---|---|
| §1–3 pools/transactions | 1–2 |
| §4 scheduling | 5–6 |
| §5 attention/executor | 3–4 |
| §6 costs/replay/metrics | 6–7 |
| §7–8 gates/deliverables | 1–8 |
